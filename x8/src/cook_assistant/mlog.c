#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void mlogPrintf(char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void MLOG_INT_PRINTF(char *title, void *data,int len, int element_size)
{
    int element, i, j;
    unsigned char *value = (unsigned char *)data;
    printf("%s", title);
    for (i = 0; i < len; ++i)
    {
        if (i % 16 == 0)
            printf("\n");
        element = value[i * element_size + element_size - 1];
        for (j = element_size - 2; j >= 0; --j)
        {
            element = element * 256 + value[i * element_size + j];
        }
        printf("%d ", element);
    }
    printf("\n");
}

void MLOG_HEX(char *title, void *data,int len, int element_size)
{
    int element, i, j;
    unsigned char *value = (unsigned char *)data;
    printf("%s", title);
    for (i = 0; i < len; ++i)
    {
        if (i % 16 == 0)
            printf("\n");
        element = value[i * element_size + element_size - 1];
        for (j = element_size - 2; j >= 0; --j)
        {
            element = element * 256 + value[i * element_size + j];
        }
        printf("0x%x ", element);
    }
    printf("\n");
}
