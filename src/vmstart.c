// vmstart.c

#include "vmc.h"

void vmStart(int argc, char *argv[])
{
    char vmName[64] = "", option[] = "headless";
    if (argc == 2) {
        argCopy(vmName, 64, argv[0]);
        argCopy(option, 8, argv[1]);
    }
    else if (argc == 1) {
        argCopy(vmName, 64, argv[0]);
    }
    else {
        printf("Usage: %s start <vmName> [g]\n", prgname);
        Exit(1);
    }

    if (Equal(option, "g")) {
        strcpy(option, "gui");
    } 

    // Lookup VM object
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        printf("VM '%s' is not registered\n", vmName);
        Exit(EXIT_FAILURE);
    }

    if (VMState(vm) == MachineState_Running) {
        printf("VM '%s' is already running\n", vmName);
        Exit(EXIT_FAILURE);
    }

    // Don't start VM unless the IP has been properly defined and is valid
    char *ip = GetVMProp(vm, "/vm/ip");
    if (Equal(ip, "<undefined>") || !ValidIpStr(ip)) {
        printf("IP address '%s' is not valid.\n", ip);
        Exit(EXIT_FAILURE); 
    }

    // Don't start if unable to set IP address
    if (!SetVMIP(vm, ip)) {
        printf("Unable to set VM '%s' IP to '%s'\n", vmName, ip);
        Exit(EXIT_FAILURE);
    }

    if (!StartVM(vm, option)) {
        printf("Error starting VM\n");
        Exit(EXIT_FAILURE);
    }

    Exit(EXIT_SUCCESS);
}


// Start VM. Assumes name & ip have been validated 
bool StartVM(IMachine *vm, char *option)
{
    // Get new ISession object - Type1
    ISession *session = GetSession(NULL, LockType_Null, NULL);

    IProgress *progress = NULL;
    SAFEARRAY *env = NULL;        // Environment string
    BSTR fetype_16;
    Convert8to16(option, &fetype_16);  // Front-End Type [gui, headless, sdl, '']
    
    // Start the VM and hangle progress
    HRESULT rc = IMachine_LaunchVMProcess(vm,
        session,
        fetype_16,
        ComSafeArrayAsInParam(env),
        &progress);
    FreeBSTR(fetype_16);
    HandleProgress(progress, rc, -1);  // Timeout of -1 means wait indefinitely

    CloseSession(session);

    if (VMState(vm) == MachineState_Running) { 
        return true;
    }
    else {
        return false;
    }
}
