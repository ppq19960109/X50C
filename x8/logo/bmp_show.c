#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

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
    unsigned char type[2];    //文件类型
    unsigned int size;        //文件大小
    unsigned short reserved1; //保留字段1
    unsigned short reserved2; //保留字段2
    unsigned int offset;      //到位图数据的偏移量
} __attribute__((packed)) bmp_file_header;

/**** 位图信息头数据结构 ****/
typedef struct
{
    unsigned int size;        //位图信息头大小
    int width;                //图像宽度
    int height;               //图像高度
    unsigned short planes;    //位面数
    unsigned short bpp;       //像素深度
    unsigned int compression; //压缩方式
    unsigned int image_size;  //图像大小
    int x_pels_per_meter;     //像素/米
    int y_pels_per_meter;     //像素/米
    unsigned int clr_used;
    unsigned int clr_omportant;
} __attribute__((packed)) bmp_info_header;

/**** 静态全局变量 ****/
static int width;                // LCD X分辨率
static int height;               // LCD Y分辨率
static void *screen_base = NULL; //映射后的显存基地址
static int bpp;                  //像素深度

void show_fix_screeninfo(struct fb_fix_screeninfo *p)
{
    printf("=== FIX SCREEN INFO === \n");

    printf("\tid: %s\n", p->id);
    printf("\tsmem_start: %#x\n", p->smem_start);
    printf("\tsmem_len: %u bytes\n", p->smem_len);

    printf("\ttype:");
    switch (p->type)
    {
    case FB_TYPE_PACKED_PIXELS:
        printf("PACKED_PIXELS\n");
        break;
    case FB_TYPE_PLANES:
        printf("PLANES\n");
        break;
    case FB_TYPE_INTERLEAVED_PLANES:
        printf("INTERLEAVED_PLANES\n");
        break;
    case FB_TYPE_TEXT:
        printf("TEXT\n");
        break;
    case FB_TYPE_VGA_PLANES:
        printf("VGA_PLANES\n");
        break;
    }

    printf("\tvisual:");
    switch (p->visual)
    {
    case FB_VISUAL_MONO01:
        printf("MONO01\n");
        break;
    case FB_VISUAL_MONO10:
        printf("MONO10\n");
        break;
    case FB_VISUAL_TRUECOLOR:
        printf("TRUECOLOR\n");
        break;
    case FB_VISUAL_PSEUDOCOLOR:
        printf("PSEUDOCOLOR\n");
        break;
    case FB_VISUAL_DIRECTCOLOR:
        printf("DIRECTCOLOR\n");
        break;
    case FB_VISUAL_STATIC_PSEUDOCOLOR:
        printf("STATIC_PSEUDOCOLOR\n");
        break;
    }

    printf("\txpanstep: %u\n", p->xpanstep);
    printf("\typanstep: %u\n", p->ypanstep);
    printf("\tywrapstep: %u\n", p->ywrapstep);
    printf("\tline_len: %u bytes\n", p->line_length);

    printf("\tmmio_start: %#x\n", p->mmio_start);
    printf("\tmmio_len: %u bytes\n", p->mmio_len);

    printf("\taccel: ");
    switch (p->accel)
    {
    case FB_ACCEL_NONE:
        printf("none\n");
        break;
    default:
        printf("unkown\n");
    }

    printf("\n");
}

void show_var_screeninfo(struct fb_var_screeninfo *p)
{
    printf("=== VAR SCREEN INFO === \n");

    printf("\thsync_len: %u\n", p->hsync_len);
    printf("\tvsync_len: %u\n", p->vsync_len);
    printf("\tvmode: %u\n", p->vmode);

    printf("\tvisible screen size: %ux%u\n",
           p->xres, p->yres);
    printf("\tvirtual screen size: %ux%u\n\n",
           p->xres_virtual,
           p->yres_virtual);

    printf("\tbits per pixel: %u\n", p->bits_per_pixel);
    printf("\tactivate: %u\n\n", p->activate);

    printf("\txoffset: %d\n", p->xoffset);
    printf("\tyoffset: %d\n", p->yoffset);

    printf("\tcolor bit-fields:\n");
    printf("\tR: [%u:%u]\n", p->red.offset,
           p->red.offset + p->red.length - 1);
    printf("\tG: [%u:%u]\n", p->green.offset,
           p->green.offset + p->green.length - 1);
    printf("\tB: [%u:%u]\n", p->blue.offset,
           p->blue.offset + p->blue.length - 1);
    printf("\tA: [%u:%u]\n", p->transp.offset,
           p->transp.offset + p->transp.length - 1);
    printf("\n");
}
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
    unsigned int *base = screen_base;
    unsigned int line_bytes = info_h.width * info_h.bpp / 8;
    printf("line_bytes:%d\n", line_bytes);

    unsigned int *buf = (unsigned int *)malloc(line_bytes);
    for (int j = info_h.height - 1; j >= 0; j--)
    {
        read(fd, base + j * width, line_bytes);
    }
    free(buf);
    close(fd);
}

int main(int argc, char *argv[])
{
    struct fb_fix_screeninfo fb_fix;
    struct fb_var_screeninfo fb_var;
    unsigned int screen_size;
    int fd;

    /* 打开framebuffer设备 */
    if (0 > (fd = open("/dev/fb0", O_RDWR)))
    {
        perror("open error");
        exit(-1);
    }

    /* 获取参数信息 */
    ioctl(fd, FBIOGET_VSCREENINFO, &fb_var);
    ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix);

    show_fix_screeninfo(&fb_fix);
    show_var_screeninfo(&fb_var);

    screen_size = fb_fix.line_length * fb_var.yres;
    width = fb_var.xres;
    height = fb_var.yres;
    bpp = fb_var.bits_per_pixel;
    printf("xres:%d,yres:%d,bits_per_pixel:%d,line_length:%d smem_len:%d screen_size:%d\n", fb_var.xres, fb_var.yres, fb_var.bits_per_pixel, fb_fix.line_length, fb_fix.smem_len, screen_size);
    /* 将显示缓冲区映射到进程地址空间 */
    screen_base = mmap(NULL, fb_fix.smem_len, PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == screen_base)
    {
        perror("mmap error");
        close(fd);
        exit(-1);
    }

    /* 显示BMP图片 */
    memset(screen_base, 0xFF, screen_size);
    show_bmp_image("logo.bmp");

    /* 退出 */
    munmap(screen_base, screen_size); //取消映射
    close(fd);                        //关闭文件
    exit(0);                          //退出进程
}
