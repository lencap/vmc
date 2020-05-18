// vmstop.c

#include "vmc.h"

// Stop VM
void vmStop(int argc, char *argv[])
{
    char vmName[64] = "", option[1] = "";
    if (argc == 2 && strlen(argv[1]) == 1) {
        argCopy(vmName, 64, argv[0]);
        argCopy(option, 1, argv[1]);
    }
    else if (argc == 1) {
        argCopy(vmName, 64, argv[0]);
    }
    else {
        printf("Usage: %s stop <vmName> [f]\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Get VM object
    IMachine *vm = GetVM(vmName);
    ExitIfNull(vm, "", "", 0);

    // See if it's running
    if (VMState(vm) != MachineState_Running) {
        printf("VM '%s' is not running\n", vmName);
        return;
    }

    // Get explicit confirmation
    if (!Equal(option, "f")) {       
        char response;
        printf("Stop VM '%s'? y/n ", vmName);
        scanf("%c", &response);
        if (response != 'y') { Exit(EXIT_SUCCESS); }
    }

    if (!StopVM(vm)) {
        printf("Error stopping %s\n", vmName);
        Exit(EXIT_FAILURE);
    }
    Exit(EXIT_SUCCESS);
}


// Stop given VM object
bool StopVM(IMachine *vm)
{
    // First, try running poweroff command from within the VM (most graceful method)
    int rc = 0;
    if (VMState(vm) == MachineState_Running) {
        rc = SSHVM(vm, "sudo poweroff", false);
    }
    if (rc == 0) {   // If SSH shutdown command succeeded
        // Give it about 3 seconds to finish
        int timeout = 12;
        while (timeout > 0 || VMState(vm) == MachineState_Running) {
            // Do nothing for .25 seconds. This granularity allows us
            // to exit early, as soon as State becomes PoweredOff
            usleep(250000);
            --timeout;
        }
    }

    // Finally, try normal powerDown API call
    if (VMState(vm) == MachineState_Running) {

        IMachine *vmMuta = NULL;
        ISession *session = GetSession(vm, LockType_Shared, &vmMuta);

        IConsole *console = NULL;
        ISession_GetConsole(session, &console);
        IProgress *progress = NULL;

        HRESULT rc = IConsole_PowerDown(console, &progress);
        HandleProgress(progress, rc, -1);
        // Is another timeout delay here necessary?

        CloseSession(session);
    }

    // Give last try about 3 seconds to finish also
    int timeout = 12;
    while (timeout > 0 || VMState(vm) == MachineState_Running) {
        usleep(250000);
        --timeout;
    }

    if (VMState(vm) == MachineState_Running) {
        // Future bug: There's an unlikely chance we'll get here.
        // May need additional ways to ensure it is shuttered
        return false;
    }
    return true;
}
