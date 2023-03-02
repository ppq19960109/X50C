#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
// #include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
/**** RGB888颜色定义 ****/
typedef struct rgb888_type
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} __attribute__((packed)) rgb888_t;

/**** BMP文件头数据结构 ****/
typedef struct
{
    unsigned char type[2];    // 文件类型
    unsigned int size;        // 文件大小
    unsigned short reserved1; // 保留字段1
    unsigned short reserved2; // 保留字段2
    unsigned int offset;      // 到位图数据的偏移量
} __attribute__((packed)) bmp_file_header;

/**** 位图信息头数据结构 ****/
typedef struct
{
    unsigned int size;        // 位图信息头大小
    int width;                // 图像宽度
    int height;               // 图像高度
    unsigned short planes;    // 位面数
    unsigned short bpp;       // 像素深度
    unsigned int compression; // 压缩方式
    unsigned int image_size;  // 图像大小
    int x_pels_per_meter;     // 像素/米
    int y_pels_per_meter;     // 像素/米
    unsigned int clr_used;
    unsigned int clr_omportant;
} __attribute__((packed)) bmp_info_header;

/**** 静态全局变量 ****/
struct buffer_object
{
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
};

static struct buffer_object buf;

/********************************************************************
 * 函数名称： show_bmp_image
 * 功能描述： 在LCD上显示指定的BMP图片
 * 输入参数： 文件路径
 * 返 回 值： 无
 ********************************************************************/
static void show_bmp_image(const char *path)
{
    bmp_file_header file_h;
    bmp_info_header info_h;
    int fd = -1;

    /* 打开文件 */
    if (0 > (fd = open(path, O_RDONLY)))
    {
        perror("open error");
        return;
    }

    /* 读取BMP文件头 */
    if (sizeof(bmp_file_header) !=
        read(fd, &file_h, sizeof(bmp_file_header)))
    {
        perror("read error");
        close(fd);
        return;
    }

    if (0 != memcmp(file_h.type, "BM", 2))
    {
        fprintf(stderr, "it's not a BMP file\n");
        close(fd);
        return;
    }

    /* 读取位图信息头 */
    if (sizeof(bmp_info_header) !=
        read(fd, &info_h, sizeof(bmp_info_header)))
    {
        perror("read error");
        close(fd);
        return;
    }

    /* 打印信息 */
    printf("文件大小:%d\n"
           "到位图数据的偏移量:%d\n"
           "位图信息头大小:%d\n"
           "图像分辨率:%d*%d\n"
           "像素深度:%d\n",
           file_h.size, file_h.offset,
           info_h.size, info_h.width, info_h.height,
           info_h.bpp);

    /* 将文件读写位置移动到图像数据开始处 */
    if (-1 == lseek(fd, file_h.offset, SEEK_SET))
    {
        perror("lseek error");
        close(fd);
        return;
    }

    /**** 读取图像数据显示到LCD ****/
    /*******************************************
     * 为了软件处理上方便，这个示例代码便不去做兼容性设计了
     * 我们默认传入的bmp图像是RGB565格式
     * bmp图像分辨率大小与LCD屏分辨率一样
     * 并且是倒向的位图
     *******************************************/
    unsigned int *base = buf.vaddr;
    unsigned int line_bytes = info_h.width * info_h.bpp / 8;
    printf("line_bytes:%d\n", line_bytes);

    // unsigned int *buf = (unsigned int *)malloc(line_bytes);
    for (int j = info_h.height - 1; j >= 0; j--)
    {
        read(fd, base + j * info_h.width, line_bytes);
    }
    // free(buf);
    close(fd);
}

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = {};

    create.width = bo->width;
    create.height = bo->height;
    create.bpp = 32;
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;
    drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
                 bo->handle, &bo->fb_id);

    map.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, map.offset);

    memset(bo->vaddr, 0xff, bo->size);

    return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_destroy_dumb destroy = {};

    drmModeRmFB(fd, bo->fb_id);

    munmap(bo->vaddr, bo->size);

    destroy.handle = bo->handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}
int main(int argc, char *argv[])
{
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    uint32_t conn_id;
    uint32_t crtc_id;

    /* 打开framebuffer设备 */
    if (0 > (fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC)))
    {
        perror("open error");
        exit(-1);
    }
    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;

    printf("buf.width:%d,buf.height:%d\n", buf.width, buf.height);

    modeset_create_fb(fd, &buf);

    drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);

    show_bmp_image("logo.bmp");
    getchar();
    modeset_destroy_fb(fd, &buf);

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    close(fd);
    exit(0); // 退出进程
}
