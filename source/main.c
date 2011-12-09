#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>

#include "fat_process.h"

int main(int argc, char *argv[])
{
    char* szFilename = 0;
    int   nPatchFat = 0;
    int   nReturnValue = 0;

    if (argc == 2)
    {
        szFilename = argv[1];
        nPatchFat = 0;
    }
    else if (argc == 3)
    {
        szFilename = argv[1];
        nPatchFat = 0;

        if (0 != strcmp(argv[2], "--patch"))
        {
            nReturnValue = -1;
            goto usage;
        }
        else
        {
            nPatchFat = 1;
        }
    }
    else
    {
        nReturnValue = -1;
        goto usage;
    }

    nReturnValue = process_image_file(szFilename, nPatchFat);
    goto exit;

usage:
    fprintf(stderr, "Usage: %s [input file] [--patch]\n", argv[0]);

exit:
#ifdef _DEBUG
    system("pause");
#endif

    return nReturnValue;
}
