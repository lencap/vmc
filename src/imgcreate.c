// imgcreate.c

#include "vmc.h"

// Create image from existing VM
void imgCreate(int argc, char *argv[])
{
    char imgName[64] = "", vmName[64] = "";
    if (argc == 2) {
        argCopy(imgName, 64, argv[0]);
        argCopy(vmName, 64, argv[1]);
    }
    else {
        printf("Usage: %s imgcreate <imgName> <vmName>\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Reject imgName if no .ova extension
    char tmpName[256];    // Temp variable to aid with lowercase comparison
    strcpy(tmpName, imgName);
    Lower(tmpName);
    if (!endsWith(tmpName, ".ova")) {
        fprintf(stderr, "imgName must have an '.ova' extension.\n");
        Exit(EXIT_FAILURE);
    }

    // Check if an image with same name already exists
    char imgFile[512] = "";
    sprintf(imgFile, "%s%c%s", vmhome, PATHCHAR, imgName);
    if (isFile(imgFile)) {
        fprintf(stderr, "Image file %s already exists.\n", imgFile);
        Exit(EXIT_FAILURE);
    }

    // Get VM object
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        printf("VM '%s' is not registered\n", vmName);
        Exit(EXIT_FAILURE);
    }

    // See if it's running
    if (VMState(vm) == MachineState_Running) {
        printf("VM '%s' needs to be powered off for this\n", vmName);
        Exit(EXIT_FAILURE);
    }

    // Creating an OVA = Creating an empty IAppliance object and exporting VM to it
    IAppliance *appliance = NULL;
    HRESULT rc = IVirtualBox_CreateAppliance(vbox, &appliance);
    ExitIfFailure(rc, "IVirtualBox_CreateAppliance", __FILE__, __LINE__);

    BSTR imgFile_16;
    Convert8to16(imgFile, &imgFile_16);  // Convert imgFile to API 16bit char format

    // Each exportTo call can add another VM to sysDesc array, but we
    // only care to define one (1) VM in this program (sysDesc[0])
    IVirtualSystemDescription *sysDesc = NULL;
    rc = IMachine_ExportTo(vm, appliance, imgFile_16, &sysDesc);
    ExitIfFailure(rc, "IMachine_ExportTo", __FILE__, __LINE__);

    // FUTURE OPTION
    // Here we could override any VirtualSystemDescriptionType value for each VM
    // by calling IVirtualSystemDescription_SetFinalValues. See vmCreate for how
    // to do that using SAFEARRAYS.

    // ExportOptions needs to be a SAFEARRAY of ULONG values, denoting one of the
    // following options for each VM:
    //   CreateManifest = 1  ExportDVDImages    = 2
    //   StripAllMACs   = 3  StripAllNonNATMACs = 4
    // The API doesn't define the default ExportOptions_Null (0), is which simply
    // to ignore these options. That default option is achieved by setting the
    // safe array to NULL 
    SAFEARRAY *OptionsSA = NULL;

    // Export appliance, invoking OVA creation, and handle the progress
    char *format = "ovf-2.0";
    // Currently supported formats are ovf-0.9, ovf-1.0, ovf2.0 and opc-1.0
    BSTR format_16;
    Convert8to16(format, &format_16);
    IProgress *progress = NULL;
    rc = IAppliance_Write(appliance,
        format_16,
        ComSafeArrayAsInParam(OptionsSA),
        imgFile_16,
        &progress);
    printf("Creating OVA image ...\n");
    HandleProgress(progress, rc, -1);  // Timeout of -1 means wait indefinitely
    // FreeBSTR(format_16); FreeBSTR(imgFile_16);

    Exit(EXIT_SUCCESS);
}
