// vmlist.c

#include "vmc.h"

// Print list of all VMs on this host
void PrintVMList(void)
{
    // Exit if there are no VMs
    if (!VMListCount) { Exit(EXIT_SUCCESS); }

    // Header
    printf("%-34s%-16s%-5s%-7s%-12s%s\n",
        "NAME", "OS", "CPU", "MEM", "STATE", "SSH");

    // Now parse every VM object in array
    for (int i = 0; i < VMListCount; ++i) {
        IMachine *vm = VMList[i];

        BOOL accessible = FALSE;
        IMachine_GetAccessible(vm, &accessible);
        if (!accessible) { continue; }

        // Get VM name
        char *name = GetVMName(VMList[i]);
        int l = strlen(name);
        // Add a trailing space to make name more readable when printed below
        if (l > 34) { name[l] = ' '; name[l+1] = '\0'; }
        
        // OS Type ID
        BSTR ostypeid_16;
        IMachine_GetOSTypeId(vm, &ostypeid_16);
        char *ostypeid;
        Convert16to8(ostypeid_16, &ostypeid);
        FreeBSTR(ostypeid_16);

        // CPU count
        ULONG cpucnt;
        IMachine_GetCPUCount(vm, &cpucnt);

        // Memory size
        ULONG memsize;
        IMachine_GetMemorySize(vm, &memsize);

        // VM state
        char state[32];
        strcpy(state, VMStateStr[VMState(vm)]);

        // SSH coonection string
        char *ip = GetVMProp(vm, "/vm/ip");
        char sshconn[32] = "";
        sprintf(sshconn, "%s@%s", vmuser, ip);

        // Print 'em out
        printf("%-34s%-16s%-5u%-7u%-12s%s\n",
            name, ostypeid, cpucnt, memsize, state, sshconn);
        // Free some memory
        free(name);
        free(ostypeid);
    }
    Exit(EXIT_SUCCESS);   // Will also release VMList   
}


// Update in-memory global list of VMs on this host
void UpdateVMList(void)
{
    // The in-memory global VMList optimizes this program because
    // this list can be referenced a few times within the same run

    // Temp safe array to get list of these objects
    SAFEARRAY *SA = SAOutParamAlloc();

    // Get list and store in SA
    IVirtualBox_GetMachines(vbox,
        ComSafeArrayAsOutIfaceParam(SA, IMachine *));
    ExitIfNull(SA, "VMList SA", __FILE__, __LINE__);
    
    FreeVMList();  // Release existing one in memory

    // Transfer from safe array to regular array, updating counter 
    SACopyOutIfaceParamHelper((IUnknown ***)&VMList, &VMListCount, SA);
    ExitIfNull(VMList, "VMList array", __FILE__, __LINE__);
    SADestroy(SA);
}


// Free exiting list of VM and its members
void FreeVMList(void)
{
    // DISSABLED: Causes "Segmentation fault: 11"
    // for (int i = 0; i < VMListCount; ++i) {
    //     if (VMList[i]) { IMachine_Release(VMList[i]); }
    // }
    // !!!!***********************************!!!!
    // Also wasted a number of days trying to figure out why, to no avail :-(
    // It may be that IMachine_Release is only supported in Web API 
    // !!!!***********************************!!!!

    if (VMList) { ArrayOutFree(VMList); }
    VMList = NULL;
    VMListCount = 0;
}


// Get pointer reference to a VM. Return NULL if it doesn't exist
IMachine * GetVM(char *vmName)
{
    // Update global list if empty
    if (!VMListCount) { UpdateVMList(); }

    // We'll return NULL if we cannot find it
    IMachine *vm = NULL;

    // Locate and return given VM object in memory
    for (int i = 0; i < VMListCount; ++i) {
        char *name = GetVMName(VMList[i]);
        if (Equal(name, vmName)) {
            vm = VMList[i];
            free(name);
            break;
        }
        free(name);
    }
    return vm;
}


// Get VM state
PRUint32 VMState(IMachine *vm)
{
    PRUint32 state;
    IMachine_GetState(vm, &state);
    return state;
}


// Get VM name
char * GetVMName(IMachine *vm)
{
    // Lookup VM UTF16 name
    BSTR name_16;
    IMachine_GetName(vm, &name_16);

    // Lack of UTF16 strlen forces more cumbersome temp circumventions
    char *tempName;
    Convert16to8(name_16, &tempName);
    int l = strlen(tempName);
    free(tempName);

    // NOTE: We're allocating new memory because otherwise tempName
    // wouldn't survive after this function exits

    // Allocate only required memory for length of UTF8 name
    char *name = NewString(l);

    // Transfer UTF16 name to UTF8 name
    Convert16to8(name_16, &name);
    FreeBSTR(name_16);  // Free UTF16 one

    // REMINDER: Caller must free allocated memory
    return name;
}


// Get VM Guest Property value
char * GetVMProp(IMachine *vm, char *path)
{
    // Unfortunately API only works with UTF16 (BSTR) and not UTF8 (char *),
    // so this involves some cumbersome temp allocations and conversions of
    // the property path we ask for, and the value we get back 
    BSTR path_16, value_16;
    Convert8to16(path, &path_16);
    IMachine_GetGuestPropertyValue(vm, path_16, &value_16);
    FreeBSTR(path_16);   // Free UTF16

    // Lack of UTF16 strlen forces more cumbersome temp circumventions
    char *tempValue;
    Convert16to8(value_16, &tempValue);
    int l = strlen(tempValue);
    free(tempValue);

    // Return NULL if there is no value
    if (l < 1) { return NULL; } 

    // Allocate only required memory for length of UTF8 value
    char *value = NewString(l);

    // Transfer UTF16 name to UTF8 name
    Convert16to8(value_16, &value);
    FreeBSTR(value_16);  // Free UTF16

    // REMINDER: Caller must free allocated memory
    return value;
}


// Set VM Guest Property value
void SetVMProp(IMachine *vm, char *path, const char *value)
{
    // Unfortunately API only works with UTF16 (BSTR) and not UTF8 (char *),
    // so this involves some cumbersome temp allocations and conversions of
    // the property path and values we are asking to update 
    BSTR path_16, value_16;
    Convert8to16(path, &path_16);   // Conversion macro
    Convert8to16(value, &value_16);

    HRESULT rc = IMachine_SetGuestPropertyValue(vm, path_16, value_16);
    if (FAILED(rc)) {
        fprintf(stderr, "%s:%d IMachine_SetGuestPropertyValue error\n", __FILE__, __LINE__);
        PrintVBoxException();
        Exit(EXIT_FAILURE);
    }
    
    FreeBSTR(path_16);   // Free UTF16 vars
    FreeBSTR(value_16);
}
