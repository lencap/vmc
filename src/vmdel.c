// vmdel.c

#include "vmc.h"

// Manually set VM IP address
void vmDelete(int argc, char *argv[])
{
    char vmName[64] = "", option[2] = "n";
    if (argc == 2 && strlen(argv[1]) == 1) {
        argCopy(vmName, 64, argv[0]);
        argCopy(option, 1, argv[1]);
    }
    else if (argc == 1) {
        argCopy(vmName, 64, argv[0]);
    }
    else {
        printf("Usage: %s del <vmName> [f]\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Get explicit confirmation
    if (!Equal(option, "f")) {       
        char response;
        printf("Destroy VM '%s'? y/n ", vmName);
        scanf("%c", &response);
        if (response != 'y') { Exit(EXIT_SUCCESS); }
    }

    // Get VM object
    IMachine *vm = GetVM(vmName);
    ExitIfNull(vm, "", "", 0);

    // Stop it if it is running
    if (VMState(vm) == MachineState_Running) { StopVM(vm); }

    // Unregister machine and get the list of media attached to it
    SAFEARRAY *SA = SAOutParamAlloc();  // Temp safe array to hold media list
    HRESULT rc = IMachine_Unregister(vm,
        CleanupMode_DetachAllReturnHardDisksOnly,
        // 0 = CleanupMode_UnregisterOnly
        // 1 = CleanupMode_DetachAllReturnNone
        // 3 = CleanupMode_DetachAllReturnHardDisksOnly
        // 4 = CleanupMode_Full
        ComSafeArrayAsOutIfaceParam(SA, IMedium *));
    ExitIfNull(SA, "Media List SA", __FILE__, __LINE__);  

    // Delete VM configuration, using returned media list SafeArray
    IProgress *progress;
    rc = IMachine_DeleteConfig(vm,
        ComSafeArrayAsInParam(SA),
        &progress);
    HandleProgress(progress, rc, -1);
    // Timeout of -1 means wait indefinitely

    //BUG
    // Everything works fine when we call this function, but if running, the
    // VirtualBox GUI crashes. Maybe we need to register as De-Registion event?

    SADestroy(SA);
    Exit(EXIT_SUCCESS);
}
