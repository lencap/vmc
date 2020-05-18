// vminfo-folders.c

#include "vmc.h"

// Print shared folders
int printSharedFolders(IMachine *vm)
{
    ULONG sfCount = 0;
    ISharedFolder **sfList = getSFList(vm, &sfCount);
    if (sfCount) { printf("%-40s\n", "SHARED_FOLDERS"); }
    for (int i = 0; i < sfCount; ++i) {
        ISharedFolder *sf = sfList[i];

        BSTR buf_16;
        ISharedFolder_GetName(sf, &buf_16);
        char *buf;
        Convert16to8(buf_16, &buf);
        FreeBSTR(buf_16);
        printf("%-40s  %s\n", "  Name", buf);
        free(buf);

        ISharedFolder_GetHostPath(sf, &buf_16);
        Convert16to8(buf_16, &buf);
        FreeBSTR(buf_16);
        printf("%-40s  %s\n", "    HostPath", buf);
        free(buf);

        ISharedFolder_GetAutoMountPoint(sf, &buf_16);
        Convert16to8(buf_16, &buf);
        FreeBSTR(buf_16);
        printf("%-40s  %s\n", "    AutoMountPoint", buf);
        free(buf);

        BOOL stat;
        ISharedFolder_GetAccessible(sf, &stat);
        printf("%-40s  %s\n", "    Accessible", BoolStr[stat]);

        ISharedFolder_GetWritable(sf, &stat);
        printf("%-40s  %s\n", "    Writable", BoolStr[stat]);

        ISharedFolder_GetAutoMount(sf, &stat);
        printf("%-40s  %s\n", "    AutoMount", BoolStr[stat]);

        ISharedFolder_GetLastAccessError(sf, &buf_16);
        Convert16to8(buf_16, &buf);
        FreeBSTR(buf_16);
        printf("%-40s  %s\n", "    LastAccessErr", buf);
        free(buf);
    }
    // Free mem
    for (int i = 0; i < sfCount; ++i) {
        if (sfList[i]) { ISharedFolder_Release(sfList[i]); }
    }
    ArrayOutFree(sfList);
    return 0;
}


// Return list of permanent Shared Folders
ISharedFolder **getSFList(IMachine *vm, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *ListSA = SAOutParamAlloc();

    IMachine_GetSharedFolders(vm,
        ComSafeArrayAsOutIfaceParam(ListSA, ISharedFolder *));
    ExitIfNull(ListSA, "GetSharedFolders SA", __FILE__, __LINE__);          

    // REMINDER: Caller must free allocated memory
    ISharedFolder **List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, ListSA);
    SADestroy(ListSA);
    return List;
}
