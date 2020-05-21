// imgdel.c

#include "vmc.h"

// Delete given image
void imgDelete(int argc, char *argv[])
{
    char imgName[64] = "", option[1] = "";
    if (argc == 2 && strlen(argv[1]) == 1) {
        argCopy(imgName, 64, argv[0]);
        argCopy(option, 1, argv[1]);
    }
    else if (argc == 1) {
        argCopy(imgName, 64, argv[0]);
    }
    else {
        printf("Usage: %s imgdel <imgName> [f]\n", prgname);
        Exit(EXIT_FAILURE);
    }

    char imgFile[512];
    sprintf(imgFile, "%s%c%s", vmhome, PATHCHAR, imgName);
    if (!isFile(imgFile)) {
        fprintf(stderr, "No '%s' OVA file.\n", imgFile);
        Exit(EXIT_FAILURE);
    }

    // Get explicit confirmation
    if (!Equal(option, "f")) {       
        char response;
        printf("Delete '%s'? y/n ", imgName);
        scanf("%c", &response);
        if (response != 'y') { Exit(EXIT_SUCCESS); }
    }

    int rc;
    rc = remove(imgFile);
    if (rc != 0) { fprintf(stderr, "Error deleting '%s'\n", imgFile); }

    Exit(EXIT_SUCCESS);
}
