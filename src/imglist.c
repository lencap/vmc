// imglist.c

#include "vmc.h"
#include <dirent.h>

void imgList(void)
{
    // Create a list of existing OVA file names
    DIR *d = opendir(vmhome);
    if (!d) {
        fprintf(stderr, "Error opening directory: %s\n", vmhome);
        Exit(EXIT_FAILURE);
    }

    // Impose a limit of 32 max files, with names no longer than 64 characters
    char fList[32][64];
    int Count = 0;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (Count > 31) {
            fprintf(stderr, "More than 31 file is not supported\n");
            Exit(EXIT_FAILURE);
        }

        if (strlen(dir->d_name) > 64) {
            fprintf(stderr, "Filename exceeds 64 char limit.\n  %s\n", dir->d_name);
            Exit(EXIT_FAILURE);
        }

        char tmpName[64];
        strcpy(tmpName, dir->d_name);
        Lower(tmpName);
        if (!endsWith(tmpName, ".ova")) { continue; }

        // Add this one to our filename list
        strcpy(fList[Count], dir->d_name);
        ++Count;
    }
    closedir(d);

    if (Count < 1) { return; }

    // Print them
    printf("LOCATION: %s/\n", vmhome);
    printf("%-34s%-8s%s\n", "NAME", "SIZE", "DATE");
    for (int i = 0; i < Count; i++) {
        if ( (strlen(vmhome)+strlen(fList[i])) > 511 ) {
            fprintf(stderr, "OVA file full path exceeds 512 chars:\n  %s%s%s\n",
                vmhome, PATHSEPSTR, fList[i]);
            Exit(EXIT_FAILURE);
        }
        char fullPath[512];
        sprintf(fullPath, "%s%s%s", vmhome, PATHSEPSTR, fList[i]);

        // Calculate file size in megabytes
        char fSize[8] = "";
        long int size = fileSize(fullPath);
        if (size != -1)  {
            sprintf(fSize, "%luMB", size/(1024*1024));
        }
        else {
            strcpy(fSize, "Error?");
        }

        // Get and format file date/time string
        char fDate[32] = "";
        struct stat f;
        if (!stat(fullPath, &f)) {
            strftime(fDate, sizeof(fDate), "%Y-%m-%d %H:%M", localtime(&f.st_mtime));
        }
        else {
            sprintf(fDate, "Error?");
        }

        printf("%-34s%-8s%s\n", fList[i], fSize, fDate);
    }

    Exit(EXIT_SUCCESS);    
}
