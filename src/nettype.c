// netype.c

#include "vmc.h"

// Add a new HostOnly NIC to this host, using given gateway IP address
void netType(int argc, char *argv[])
{
    // Requires two arguments
    if (argc != 2) {
        printf("Usage: %s nettype <vmName> <ho[bri]>\n", prgname);
        Exit(EXIT_FAILURE);
    }
    char vmName[64] = "", nettype[] = "bri";
    argCopy(vmName, 64, argv[0]);
    argCopy(nettype, 3, argv[1]);
    
    if (!Equal(nettype, "ho") && !Equal(nettype, "bri")) {
        printf("Usage: %s nettype <vmName> <ho[bri]>\n", prgname);
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

    if (!SetVMNetType(vm, nettype)) {
        fprintf(stderr, "Error updating VM '%s' net type to `%s`\n", vmName, nettype);
        Exit(EXIT_FAILURE);
    }

    Exit(EXIT_SUCCESS);
}


// Set network type on this VM
bool SetVMNetType(IMachine *vm, char *nettype)
{
    // Get new ISession object - Type2
    IMachine *vmMuta = NULL;   // For temporary mutable VM object
    ISession *session = GetSession(vm, LockType_Write, &vmMuta);
    if (!vmMuta) {
        CloseSession(session);
        return false;
    }
    // Note: From here on we're modifying the writeable vmMuta object

    SetVMProp(vmMuta, "/vm/nettype", nettype);

    CloseSession(session);
    return true; 
}
