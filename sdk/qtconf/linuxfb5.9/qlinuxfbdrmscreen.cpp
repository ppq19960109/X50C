/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Experimental DRM dumb buffer backend.
//
// TODO:
// Multiscreen: QWindow-QScreen(-output) association. Needs some reorg (device cannot be owned by screen)
// Find card via devicediscovery like in eglfs_kms.
// Mode restore like QEglFSKmsInterruptHandler.
// Formats other then 32 bpp?
// grabWindow

#include "qlinuxfbdrmscreen.h"
#include <QLoggingCategory>
#include <QGuiApplication>
#include <QPainter>
#include <QtCore/QRegularExpression>
#include <QtFbSupport/private/qfbcursor_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtKmsSupport/private/qkmsdevice_p.h>
#include <QtCore/private/qcore_unix_p.h>
#include <sys/mman.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcFbDrm, "qt.qpa.fb")

#define USE_RGB16

#define TRIPLE_BUFFER

#ifdef TRIPLE_BUFFER
static const int BUFFER_COUNT = 3;
#else
static const int BUFFER_COUNT = 2;
#endif

class QLinuxFbDevice : public QKmsDevice
{
public:
    struct Framebuffer {
        Framebuffer() : handle(0), pitch(0), size(0), fb(0), p(MAP_FAILED) { }
        uint32_t handle;
        uint32_t pitch;
        uint64_t size;
        uint32_t fb;
        void *p;
        QImage wrapper;
    };

    struct Output {
        Output() : backFb(0), flipPending(false) { }
        QKmsOutput kmsOutput;
        Framebuffer fb[BUFFER_COUNT];
        QRegion dirty[BUFFER_COUNT];
        int backFb;
        bool flipPending;
        QSize currentRes() const {
            const drmModeModeInfo &modeInfo(kmsOutput.modes[kmsOutput.mode]);
            return QSize(modeInfo.hdisplay, modeInfo.vdisplay);
        }
    };

    QLinuxFbDevice(QKmsScreenConfig *screenConfig);

    bool open() override;
    void close() override;

    void createFramebuffers();
    void destroyFramebuffers();
    void setMode();

    void swapBuffers(Output *output);
    void waitForFlip(Output *output);

    int outputCount() const { return m_outputs.count(); }
    Output *output(int idx) { return &m_outputs[idx]; }

private:
    void *nativeDisplay() const override;
    QPlatformScreen *createScreen(const QKmsOutput &output) override;
    void registerScreen(QPlatformScreen *screen,
                        bool isPrimary,
                        const QPoint &virtualPos,
                        const QList<QPlatformScreen *> &virtualSiblings) override;

    bool createFramebuffer(Output *output, int bufferIdx);
    void destroyFramebuffer(Output *output, int bufferIdx);

    static void pageFlipHandler(int fd, unsigned int sequence,
                                unsigned int tv_sec, unsigned int tv_usec, void *user_data);

    QVector<Output> m_outputs;
};

QLinuxFbDevice::QLinuxFbDevice(QKmsScreenConfig *screenConfig)
    : QKmsDevice(screenConfig, QStringLiteral("/dev/dri/card0"))
{
}

bool QLinuxFbDevice::open()
{
    int fd = qt_safe_open(devicePath().toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        qErrnoWarning("Could not open DRM device %s", qPrintable(devicePath()));
        return false;
    }

    uint64_t hasDumbBuf = 0;
    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &hasDumbBuf) == -1 || !hasDumbBuf) {
        qWarning("Dumb buffers not supported");
        qt_safe_close(fd);
        return false;
    }

    setFd(fd);

    qCDebug(qLcFbDrm, "DRM device %s opened", qPrintable(devicePath()));

    return true;
}

void QLinuxFbDevice::close()
{
    for (Output &output : m_outputs)
        output.kmsOutput.cleanup(this); // restore mode

    m_outputs.clear();

    if (fd() != -1) {
        qCDebug(qLcFbDrm, "Closing DRM device");
        qt_safe_close(fd());
        setFd(-1);
    }
}

void *QLinuxFbDevice::nativeDisplay() const
{
    Q_UNREACHABLE();
    return nullptr;
}

QPlatformScreen *QLinuxFbDevice::createScreen(const QKmsOutput &output)
{
    qCDebug(qLcFbDrm, "Got a new output: %s", qPrintable(output.name));
    Output o;
    o.kmsOutput = output;
    m_outputs.append(o);
    return nullptr; // no platformscreen, we are not a platform plugin
}

void QLinuxFbDevice::registerScreen(QPlatformScreen *screen,
                                    bool isPrimary,
                                    const QPoint &virtualPos,
                                    const QList<QPlatformScreen *> &virtualSiblings)
{
    Q_UNUSED(screen);
    Q_UNUSED(isPrimary);
    Q_UNUSED(virtualPos);
    Q_UNUSED(virtualSiblings);
    Q_UNREACHABLE();
}

bool QLinuxFbDevice::createFramebuffer(QLinuxFbDevice::Output *output, int bufferIdx)
{
    const QSize size = output->currentRes();
    const uint32_t w = size.width();
    const uint32_t h = size.height();

#ifdef USE_RGB16
    const int depth = 16;
    const int bpp = 16;
#else
    const int depth = 24;
    const int bpp = 32;
#endif

    drm_mode_create_dumb creq = {
        h,
        w,
        bpp,
        0, 0, 0, 0
    };
    if (drmIoctl(fd(), DRM_IOCTL_MODE_CREATE_DUMB, &creq) == -1) {
        qErrnoWarning(errno, "Failed to create dumb buffer");
        return false;
    }

    Framebuffer &fb(output->fb[bufferIdx]);
    fb.handle = creq.handle;
    fb.pitch = creq.pitch;
    fb.size = creq.size;
    qCDebug(qLcFbDrm, "Got a dumb buffer for size %dx%d, handle %u, pitch %u, size %u",
            w, h, fb.handle, fb.pitch, (uint) fb.size);

    if (drmModeAddFB(fd(), w, h, depth, bpp, fb.pitch, fb.handle, &fb.fb) == -1) {
        qErrnoWarning(errno, "Failed to add FB");
        return false;
    }

    drm_mode_map_dumb mreq = {
        fb.handle,
        0, 0
    };
    if (drmIoctl(fd(), DRM_IOCTL_MODE_MAP_DUMB, &mreq) == -1) {
        qErrnoWarning(errno, "Failed to map dumb buffer");
        return false;
    }
    fb.p = mmap(0, fb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd(), mreq.offset);
    if (fb.p == MAP_FAILED) {
        qErrnoWarning(errno, "Failed to mmap dumb buffer");
        return false;
    }

    qCDebug(qLcFbDrm, "FB is %u, mapped at %p", fb.fb, fb.p);
    memset(fb.p, 0, fb.size);

#ifdef USE_RGB16
    fb.wrapper = QImage(static_cast<uchar *>(fb.p), w, h, fb.pitch, QImage::Format_RGB16);
#else
    fb.wrapper = QImage(static_cast<uchar *>(fb.p), w, h, fb.pitch, QImage::Format_ARGB32);
#endif

    return true;
}

void QLinuxFbDevice::createFramebuffers()
{
    for (Output &output : m_outputs) {
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            if (!createFramebuffer(&output, i))
                return;
        }
        output.backFb = 0;
        output.flipPending = false;
    }
}

void QLinuxFbDevice::destroyFramebuffer(QLinuxFbDevice::Output *output, int bufferIdx)
{
    Framebuffer &fb(output->fb[bufferIdx]);
    if (fb.p != MAP_FAILED)
        munmap(fb.p, fb.size);
    if (fb.fb) {
        if (drmModeRmFB(fd(), fb.fb) == -1)
            qErrnoWarning("Failed to remove fb");
    }
    if (fb.handle) {
        drm_mode_destroy_dumb dreq = { fb.handle };
        if (drmIoctl(fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &dreq) == -1)
            qErrnoWarning(errno, "Failed to destroy dumb buffer %u", fb.handle);
    }
    fb = Framebuffer();
}

void QLinuxFbDevice::destroyFramebuffers()
{
    for (Output &output : m_outputs) {
        for (int i = 0; i < BUFFER_COUNT; ++i)
            destroyFramebuffer(&output, i);
    }
}

void QLinuxFbDevice::setMode()
{
    QPixmap pixmap;
    bool hasLogo = false;

    static QByteArray logoFile = qgetenv("QT_LINUXFB_DRM_LOGO");
    if (!logoFile.isEmpty()) {
        if(pixmap.load(logoFile.constData())) {
            hasLogo = true;
            qWarning("pixmap load logo succeed %s\n", logoFile.constData());
    } else {
            qWarning("pixmap load logo failed %s\n", logoFile.constData());
        }
    }

    for (Output &output : m_outputs) {
        drmModeModeInfo &modeInfo(output.kmsOutput.modes[output.kmsOutput.mode]);
        if (hasLogo) {
            QPainter pntr(&output.fb[0].wrapper);
            pntr.drawPixmap(0,0, pixmap);
        }
        if (drmModeSetCrtc(fd(), output.kmsOutput.crtc_id, output.fb[0].fb, 0, 0,
                           &output.kmsOutput.connector_id, 1, &modeInfo) == -1) {
            qErrnoWarning(errno, "Failed to set mode");
            return;
        }

        output.kmsOutput.mode_set = true; // have cleanup() to restore the mode
        output.kmsOutput.setPowerState(this, QPlatformScreen::PowerStateOn);
    }
}

void QLinuxFbDevice::pageFlipHandler(int fd, unsigned int sequence,
                                     unsigned int tv_sec, unsigned int tv_usec,
                                     void *user_data)
{
    Q_UNUSED(fd);
    Q_UNUSED(sequence);
    Q_UNUSED(tv_sec);
    Q_UNUSED(tv_usec);

    Output *output = static_cast<Output *>(user_data);

#ifndef TRIPLE_BUFFER
    // The next buffer would be available after flipped
    output->backFb = (output->backFb + 1) % BUFFER_COUNT;
#endif

    output->flipPending = false;
}

void QLinuxFbDevice::waitForFlip(Output *output)
{
    while (output->flipPending) {
        drmEventContext drmEvent;
        memset(&drmEvent, 0, sizeof(drmEvent));
        drmEvent.version = 2;
        drmEvent.vblank_handler = nullptr;
        drmEvent.page_flip_handler = pageFlipHandler;
        // Blocks until there is something to read on the drm fd
        // and calls back pageFlipHandler once the flip completes.
        drmHandleEvent(fd(), &drmEvent);
    }
}

void QLinuxFbDevice::swapBuffers(Output *output)
{
#ifdef TRIPLE_BUFFER
    // Wait flip to make sure last buffer displayed
    waitForFlip(output);
#endif

    Framebuffer &fb(output->fb[output->backFb]);
    if (drmModePageFlip(fd(), output->kmsOutput.crtc_id, fb.fb, DRM_MODE_PAGE_FLIP_EVENT, output) == -1) {
        qErrnoWarning(errno, "Page flip failed");
        return;
    }

    output->flipPending = true;

#ifdef TRIPLE_BUFFER
    // The next buffer should always available in triple buffer case.
    output->backFb = (output->backFb + 1) % BUFFER_COUNT;
#endif
}

QLinuxFbDrmScreen::QLinuxFbDrmScreen(const QStringList &args)
    : m_screenConfig(nullptr),
      m_device(nullptr),
      m_rotation(0)
{
    QRegularExpression rotationRx(QLatin1String("rotation=(0|90|180|270)"));

    for (const QString &arg : qAsConst(args)) {
        QRegularExpressionMatch match;
        if (arg.contains(rotationRx, &match))
            m_rotation = match.captured(1).toInt();
    }
}

QLinuxFbDrmScreen::~QLinuxFbDrmScreen()
{
    if (m_device) {
        m_device->destroyFramebuffers();
        m_device->close();
        delete m_device;
    }
    delete m_screenConfig;
}

bool QLinuxFbDrmScreen::initialize()
{
    m_screenConfig = new QKmsScreenConfig;
    m_device = new QLinuxFbDevice(m_screenConfig);
    if (!m_device->open())
        return false;

    // Discover outputs. Calls back Device::createScreen().
    m_device->createScreens();
    // Now off to dumb buffer specifics.
    m_device->createFramebuffers();
    // Do the modesetting.
    m_device->setMode();

    QLinuxFbDevice::Output *output(m_device->output(0));

    mGeometry = QRect(QPoint(0, 0), output->currentRes());

    if(m_rotation % 180) {
        int tmp = mGeometry.width();
        mGeometry.setWidth(mGeometry.height());
        mGeometry.setHeight(tmp);
    }

#ifdef USE_RGB16
    mDepth = 16;
    mFormat = QImage::Format_RGB16;
#else
    mDepth = 32;
    mFormat = QImage::Format_ARGB32;
#endif
    mPhysicalSize = output->kmsOutput.physical_size;
    qCDebug(qLcFbDrm) << mGeometry << mPhysicalSize;

    QFbScreen::initializeCompositor();

    mCursor = new QFbCursor(this);

    return true;
}

QRegion QLinuxFbDrmScreen::doRedraw()
{
    const QRegion dirty = QFbScreen::doRedraw();
    if (dirty.isEmpty())
        return dirty;

    QLinuxFbDevice::Output *output(m_device->output(0));

    for (int i = 0; i < BUFFER_COUNT; ++i)
        output->dirty[i] += dirty;

#ifndef TRIPLE_BUFFER
    // Wait flip before accessing new buffer
    m_device->waitForFlip(output);
#endif

    if (output->fb[output->backFb].wrapper.isNull())
        return dirty;

    QPainter pntr(&output->fb[output->backFb].wrapper);
    // Image has alpha but no need for blending at this stage.
    // Do not waste time with the default SourceOver.
    pntr.setCompositionMode(QPainter::CompositionMode_Source);
    for (const QRect &rect : qAsConst(output->dirty[output->backFb])) {
        if(m_rotation) {
            if(m_rotation == 180)
                pntr.translate(mGeometry.width()/2, mGeometry.height()/2);
            else
                pntr.translate(mGeometry.height()/2, mGeometry.width()/2);

            pntr.rotate(m_rotation);
            pntr.translate(-mGeometry.width()/2, -mGeometry.height()/2);
        }

        pntr.drawImage(rect, mScreenImage, rect);

        pntr.resetTransform();
    }
    pntr.end();

    output->dirty[output->backFb] = QRegion();

    m_device->swapBuffers(output);

    return dirty;
}

QPixmap QLinuxFbDrmScreen::grabWindow(WId wid, int x, int y, int width, int height) const
{
    Q_UNUSED(wid);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);

    return QPixmap();
}

QT_END_NAMESPACE
