#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "linkkit_solo.h"
#include "cloud.h"

int main(int argc, char **argv)
{
    printf("main start...\n");
    if (cloud_init() != 0)
    {
        printf("main cloud_init err\n");
        return -1;
    }

    linkkit_main("a1YTZpQDGwn", "oE99dmyBcH5RAWE3", "X50_test1", "5fe43d0b7a6b2928c4310cc0d5fcb4b6");
}