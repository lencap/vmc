// vminfo-media.c

#include "vmc.h"

// Print media connections
int printMediaAttachments(IMachine *vm)
{
    ULONG maCount = 0;
    IMediumAttachment **maList = getMAList(vm, &maCount);
    for (int i = 0; i < maCount; ++i) {
        IMediumAttachment *ma = maList[i];

        BSTR ctrl_16;
        IMediumAttachment_GetController(ma, &ctrl_16);
        char *ctrl;
        Convert16to8(ctrl_16, &ctrl);
        FreeBSTR(ctrl_16);

        PRInt32 port;
        IMediumAttachment_GetPort(ma, &port);

        PRInt32 dev;
        IMediumAttachment_GetDevice(ma, &dev);

        ULONG type = 0;
        IMediumAttachment_GetType(ma, &type);

        printf("%-40s  controller=%-16s: port/dev=%u/%u type=%s\n",
            "MEDIA", ctrl, port, dev, DevType[type]);
        free(ctrl);

        BOOL passthrough;
        IMediumAttachment_GetPassthrough(ma, &passthrough);

        // Get and display medium
        IMedium *m = NULL;
        IMediumAttachment_GetMedium(ma, &m);
        if (!m) { continue; }   // Empty drive, no media inserted, skip

        BSTR id_16 = NULL;
        IMedium_GetId(m, &id_16);
        char *id = NULL;
        Convert16to8(id_16, &id);
        FreeBSTR(id_16);

        BSTR location_16 = NULL;
        IMedium_GetLocation(m, &location_16);
        char *location = NULL;
        Convert16to8(location_16, &location);
        FreeBSTR(location_16);

        BOOL hostDrive;
        IMedium_GetHostDrive(m, &hostDrive);

        // We only care about displaying HDD and DVD media
        ULONG devType;
        IMedium_GetDeviceType(m, &devType);
        if (devType == DeviceType_HardDisk) {
            printf("%-40s  %s\n", "  ID", id);
            printf("%-40s  %s\n", "  Path", location);
        }
        else if (devType == DeviceType_DVD) {
            printf("%-40s  %s\n", "  ID", id);
            if (hostDrive) {
                printf("%-40s  %s\n", "  HostDVD", location);
                if (passthrough) {
                    printf("%-40s\n", "  [passthrough mode]");
                }
            }
            else {
                printf("%-40s  %s\n", "  Path", location);
            }
        }
        IMedium_Release(m);
    }
    // Free mem
    for (int i = 0; i < maCount; ++i) {
        if (maList[i]) { IMediumAttachment_Release(maList[i]); }
    }
    ArrayOutFree(maList);
    return 0;
}


// Get list of medium attachments
IMediumAttachment ** getMAList(IMachine *vm, ULONG *Count)
{
    // Temp safe array for the media attachment list
    SAFEARRAY *ListSA = SAOutParamAlloc();

    HRESULT rc = IMachine_GetMediumAttachments(vm,
        ComSafeArrayAsOutIfaceParam(ListSA, IMediumAttachment *));
    ExitIfFailure(rc, "IMachine_GetMediumAttachments", __FILE__, __LINE__); 

    // REMINDER: Caller must free allocated memory
    IMediumAttachment **List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, ListSA);
    SADestroy(ListSA);
    return List;
}
