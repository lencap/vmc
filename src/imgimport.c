// imgimport.c

#include "vmc.h"

// Import given image
void imgImport(int argc, char *argv[])
{
    // Take only one argument
    if (argc != 1) {
        printf("Usage: %s imgimp <imgFile>\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Confirm file exists
    char imgFile[512] = "";
    argCopy(imgFile, 512, argv[0]);
    if (!isFile(imgFile)) {
        fprintf(stderr, "No '%s' file.\n", imgFile);
        Exit(EXIT_FAILURE);
    }

    // Reject if no .ova extension, or if not a real OVA/tar file
    char tmpName[512];         // Temp variable to aid with lowercase comparison
    strcpy(tmpName, imgFile);
    Lower(tmpName);
    if (!endsWith(tmpName, ".ova") || !isTarFile(imgFile)) {
        fprintf(stderr, "File '%s' lacks '.ova' extension, or it's not an OVA file.\n", imgFile);
        Exit(EXIT_FAILURE);
    }

    // Get imgFile basename
    char *imgFileBase = baseName(imgFile);   // Careful, just a pointer, not a new string
    
    // Check if there is already a file named the same, using baseName
    char targetFile[1024];
    sprintf(targetFile, "%s%s%s", vmhome, PATHSEPSTR, imgFileBase);
    if (isFile(targetFile)) {
        fprintf(stderr, "Image '%s' already exists\n", targetFile);
        Exit(EXIT_FAILURE);
    }

    // Copy the file
    int rc = copyFile(imgFile, targetFile);
    if (rc != 0) { fprintf(stderr, "Error copying '%s' to '%s'\n", imgFile, targetFile); }

    Exit(EXIT_SUCCESS);
}
