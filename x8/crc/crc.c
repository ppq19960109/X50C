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

unsigned short crc16_maxim_multi(unsigned char *ptr, int len, unsigned short before_crc)
{
    int i;
    unsigned short crc = before_crc;
    if (len == 0)
        return ~crc;
    while (len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = (crc >> 1);
        }
    }
    return crc;
}

int main(int argc, char *argv[])
{
    int fd;
    unsigned short crc = 0;
    unsigned char read_buf[2048];
    if (argc < 2)
    {
        printf("argc numner less\n");
        return -1;
    }
    printf("open file:%s\n", argv[1]);
    struct stat statbuff;
    if (stat(argv[1], &statbuff) < 0)
    {
        return -1;
    }

    if (0 > (fd = open(argv[1], O_RDONLY)))
    {
        perror("open error");
        exit(-1);
    }
    int read_len = 0;
    int read_count = 0;
    do
    {
        read_len = read(fd, read_buf, sizeof(read_buf));
        // if (read_len > 0)
        // {
        crc = crc16_maxim_multi(read_buf, read_len, crc);
        ++read_count;
        // printf("read_len:%d read_count:%d\n", read_len, read_count);
        // }
    } while (read_len >= sizeof(read_buf));
    printf("file size:%ld crc value:0x%x,%d read_count:%d\n", statbuff.st_size, crc, crc, read_count);
    close(fd);
    exit(0); //退出进程
    return 0;
}
