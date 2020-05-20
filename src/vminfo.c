// vminfo.c

#include "vmc.h"

// Display the most important VM info
int vmInfo(int argc, char *argv[])
{
    char vmName[64] = "";
    if (argc != 1) {
        printf("Usage: %s info <vmName>\n", prgname);
        Exit(EXIT_FAILURE);
    }
    argCopy(vmName, 64, argv[0]);

    // Get VM object
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        printf("VM '%s' is not registered\n", vmName);
        Exit(EXIT_FAILURE);
    }

    // Print the most important additional VM attributes
    
    // NAME
    printf("%-40s  %s\n", "Name", vmName);

    // ID
    BSTR vmID_16 = NULL;
    char *vmID = NULL;
    IMachine_GetId(vm, &vmID_16);
    Convert16to8(vmID_16, &vmID);
    FreeBSTR(vmID_16);
    printf("%-40s  %s\n", "ID", vmID);

    // SESSION STATUS
    PRUint32 sessionState;
    IMachine_GetSessionState(vm, &sessionState);
    printf("%-40s  %s\n", "SessionStatus", SessState[sessionState]);

    // STORAGECONTROLLERS
    printStorageControllers(vm);

    // MEDIA ATTACHMENTS
    printMediaAttachments(vm);

    // SHARED FOLDERS
    printSharedFolders(vm);

    // NETWORK_ADAPTERS
    printNetworkAdapters(vm);

    // GUEST ADDITION(GA) PROPERTIES
    printGuestAdditionProps(vm);
    return 0;
}


int printStorageControllers(IMachine *vm)
{
    ULONG ctlrCount = 0;
    IStorageController **ctlrList = getCtlrList(vm, &ctlrCount);
    for (int i = 0; i < ctlrCount; ++i) {
        IStorageController *ctrl = ctlrList[i];

        BSTR name_16;
        IStorageController_GetName(ctrl, &name_16);
        char *name;
        Convert16to8(name_16, &name);
        FreeBSTR(name_16);

        ULONG bus;
        IStorageController_GetBus(ctrl, &bus);

        ULONG type;
        IStorageController_GetControllerType(ctrl, &type);

        printf("%-40s  %-16s: bus=%u type=%s\n", "StorageControllers", name, bus, CtrlType[type]);
        free(name);
    }
    // Free mem
    for (int i = 0; i < ctlrCount; ++i) {
        if (ctlrList[i]) { IStorageController_Release(ctlrList[i]); }
    }
    ArrayOutFree(ctlrList);
    return 0;
}


// Get list of storage controllers in this VM
IStorageController ** getCtlrList(IMachine *vm, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *SA = SAOutParamAlloc();

    HRESULT rc = IMachine_GetStorageControllers(vm,
        ComSafeArrayAsOutIfaceParam(SA, IStorageController *));
    ExitIfFailure(rc, "IMachine_GetStorageControllers", __FILE__, __LINE__);

    // REMINDER: List object and elements must be freed by caller
    IStorageController **List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, SA);
    SADestroy(SA);
    return List;
}
