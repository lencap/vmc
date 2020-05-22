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

    // Name
    printf("%-40s  %s\n", "Name", vmName);

    // ID
    BSTR vmID_16 = NULL;
    char *vmID = NULL;
    IMachine_GetId(vm, &vmID_16);
    Convert16to8(vmID_16, &vmID);
    FreeBSTR(vmID_16);
    printf("%-40s  %s\n", "ID", vmID);

    // Session Status
    PRUint32 sessionState;
    IMachine_GetSessionState(vm, &sessionState);
    printf("%-40s  %s\n", "SessionStatus", SessState[sessionState]);

    PrintStorageControllers(vm);  // Storage Controllers
    PrintMediaAttachments(vm);    // Media Attachments
    PrintSharedFolders(vm);       // Shared Folders
    PrintNetworkAdapters(vm);     // Network Adapters
    PrintGAProperties(vm);        // Guest Addition Properties
    return 0;
}


int PrintStorageControllers(IMachine *vm)
{
    ULONG ctlrCount = 0;
    IStorageController **ctlrList = GetCtlrList(vm, &ctlrCount);
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
IStorageController ** GetCtlrList(IMachine *vm, ULONG *Count)
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


// Print media connections
int PrintMediaAttachments(IMachine *vm)
{
    ULONG maCount = 0;
    IMediumAttachment **maList = GetMAList(vm, &maCount);
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
IMediumAttachment ** GetMAList(IMachine *vm, ULONG *Count)
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


// Print shared folders
int PrintSharedFolders(IMachine *vm)
{
    ULONG sfCount = 0;
    ISharedFolder **sfList = GetSFList(vm, &sfCount);
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
ISharedFolder **GetSFList(IMachine *vm, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *ListSA = SAOutParamAlloc();

    HRESULT rc = IMachine_GetSharedFolders(vm,
        ComSafeArrayAsOutIfaceParam(ListSA, ISharedFolder *));
    ExitIfFailure(rc, "IMachine_GetSharedFolders", __FILE__, __LINE__);

    // REMINDER: Caller must free allocated memory
    ISharedFolder **List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, ListSA);
    SADestroy(ListSA);
    return List;
}


// Print network adapters
int PrintNetworkAdapters(IMachine *vm)
{
    printf("NETWORK_ADAPTERS\n");
    for (int slot = 0; slot < 2 ; ++slot) {      // We only care about NICs in slot 0 and 1
        INetworkAdapter *nic = NULL;
        IMachine_GetNetworkAdapter(vm, slot, &nic);

        BOOL status;
        INetworkAdapter_GetEnabled(nic, &status);
        char stat[] = "Disabled", ethStr[] = "ethX";
        sprintf(ethStr, "eth%u", slot);
        if (status) { printf("  %-40sEnabled\n", ethStr); }
        else        { continue; }

        ULONG val;
        INetworkAdapter_GetAdapterType(nic, &val);
        printf("    %-38s%s\n", "Type", NICType[val]);

        ULONG attachType;  
        INetworkAdapter_GetAttachmentType(nic, &attachType);
        printf("    %-38s%s\n", "AttachmentType", NICattType[attachType]);

        BSTR mac_16;
        INetworkAdapter_GetMACAddress(nic, &mac_16);
        char *mac;
        Convert16to8(mac_16, &mac);
        FreeBSTR(mac_16);
        printf("    %-38s%s\n", "MAC", mac);
        free(mac);

        INetworkAdapter_GetCableConnected(nic, &status);
        strcpy(stat, "False");
        if (status) { strcpy(stat, "True"); }
        printf("    %-38s%s\n", "CableConnected", stat);

        INetworkAdapter_GetPromiscModePolicy(nic, &val);
        printf("    %-38s%s\n", "PromiscuousMode", PromiscMode[val]);

        if (attachType == NetworkAttachmentType_Bridged) {
            BSTR bri_16;
            INetworkAdapter_GetBridgedInterface(nic, &bri_16);
            FreeBSTR(bri_16);
            char *bri;
            Convert16to8(bri_16, &bri);
            printf("    %-38s%s\n", "Bridged_Interface", bri);
            free(bri);
        }
        else if (attachType == NetworkAttachmentType_HostOnly) {
            BSTR ho_16;
            INetworkAdapter_GetHostOnlyInterface(nic, &ho_16);
            FreeBSTR(ho_16);
            char *ho;
            Convert16to8(ho_16, &ho);
            printf("    %-38s%s\n", "HostOnly_Interface", ho);
            free(ho);
        }
        else if (attachType == NetworkAttachmentType_NAT) {
            printf("    %-38s%s\n", "NAT_Interface", "N/A");
            
            INATEngine *natEng = NULL;
            INetworkAdapter_GetNATEngine(nic, &natEng);

            // Temp safe array for the port-pwd list
            SAFEARRAY *pfRulesSA = SAOutParamAlloc();
            HRESULT rc = INATEngine_GetRedirects(natEng,
                ComSafeArrayAsOutTypeParam(pfRulesSA, BSTR));
            ExitIfFailure(rc, "INATEngine_GetRedirects", __FILE__, __LINE__);

            BSTR *pfRules = NULL;
            ULONG bPfCount = 0;
            // Transfer from safe arrays to regular arrays, also update Counts 
            SACopyOutParamHelper((void **)&pfRules, &bPfCount, VT_BSTR, pfRulesSA);
            SADestroy(pfRulesSA);

            // Convert UTF16 size counter to byte size counter
            ULONG i, pfCount = bPfCount / sizeof(pfRules[0]);

            if (pfCount) {
                printf("    NATEngine\n");
                for (i = 0; i < pfCount; i++) {
                    char *rule = NULL;
                    Convert16to8(pfRules[i], &rule);

                    int Count = 0;
                    char **List = strSplit(rule, ',', &Count);
                    // REMINDER: Caller must free allocated memory
                    char rules[256] = "";
                    char prot[] = "tcp";
                    if (*List[1] == '0') {  // 2nd element is either 0:udp or 1:tcp
                        strcpy(prot, "udp");
                    }
                    sprintf(rules, "%s,%s,%s,%s,%s,%s", List[0], prot, List[2], List[3], List[4], List[5]);
                    printf("      %-36s%s\n", "PortFwdRule", rules);

                    // Free elements and array; and rule string
                    for (int j = 0; j < Count; j++) { free(List[j]); }
                    free(List);
                    free(rule);
                }
            }
            // Release all objects
            for (i = 0; i < pfCount; ++i) {
                if (pfRules[i]) { FreeBSTR(pfRules[i]); }
            }
            ArrayOutFree(pfRules);   // Only for SACopyOutParamHelper C arrays

            // Print NAT Engine DNS stats 
            int stat;
            INATEngine_GetDNSPassDomain(natEng, &stat);
            printf("      %-36s%s\n", "DNSPassDomain", BoolStr[stat]);

            INATEngine_GetDNSProxy(natEng, &stat);
            printf("      %-36s%s\n", "DNSProxy", BoolStr[stat]);

            INATEngine_GetDNSUseHostResolver(natEng, &stat);
            printf("      %-36s%s\n", "DNSUseHostResolver", BoolStr[stat]);
        }

        char propPath[64] = "";
        sprintf(propPath, "/VirtualBox/GuestInfo/Net/%u/V4/IP", slot);
        char *ip = GetVMProp(vm, propPath);
        if (ip) { printf("    %-38s%s\n", "IPAddress", ip); }
        else {    printf("    %-38s\n", "IPAddress"); }
        free(ip);
    }
    return 0;
}


// Print guest addition properties
int PrintGAProperties(IMachine *vm)
{
    // Do the ISO path manually (from ISystemProperties, not IMachine)
    BSTR gaISO_16 = NULL;
    ISystemProperties_GetDefaultAdditionsISO(sysprop, &gaISO_16);
    char *gaISO = NULL;
    Convert16to8(gaISO_16, &gaISO);
    FreeBSTR(gaISO_16);
    printf("%-40s  %s\n", "/VirtualBox/GuestAdd/ISOFile", gaISO);

    // Below mess is only to print all IMachine GA properties.

    // Temp safe arrays to get list of these objects
    SAFEARRAY *nameSA  = SAOutParamAlloc();
    SAFEARRAY *valueSA = SAOutParamAlloc();
    SAFEARRAY *timeSA  = SAOutParamAlloc();
    SAFEARRAY *flagSA  = SAOutParamAlloc();

    HRESULT rc = IMachine_EnumerateGuestProperties(vm,
        NULL,    // A NULL filter returns all properties
        ComSafeArrayAsOutTypeParam(nameSA, BSTR),
        ComSafeArrayAsOutTypeParam(valueSA, BSTR),
        ComSafeArrayAsOutTypeParam(timeSA, PRInt64),
        ComSafeArrayAsOutTypeParam(flagSA, BSTR));
    ExitIfFailure(rc, "IMachine_EnumerateGuestProperties", __FILE__, __LINE__);     

    // We only care about name/value pair, so destroy timestamp and flag SAs now
    SADestroy(timeSA);
    SADestroy(flagSA);

    // Using temp intermediate UTF16 size counter to capture number of Guest Props
    ULONG gpCount_16 = 0;
    // We're also using the same counter for both BSTR lists
    BSTR *nameList = NULL, *valueList = NULL;

    // Transfer safe arrays to regular C arrays, using BSTR variant type
    SACopyOutParamHelper((void **)&nameList, &gpCount_16, VT_BSTR, nameSA);
    SACopyOutParamHelper((void **)&valueList, &gpCount_16, VT_BSTR, valueSA);
    SADestroy(nameSA);
    SADestroy(valueSA);

    // Translate UTF16 size counters to regular byte size counters
    ULONG gpCount = gpCount_16 / sizeof(nameList[0]);

    // Print the Value and Name of each property. Remember: We're disregarding TimeStamp and Flag.
    for (int i = 0; i < gpCount; ++i) {
        char *name, *value;
        Convert16to8(nameList[i], &name);
        Convert16to8(valueList[i], &value);
        printf("%-40s  %s\n", name, value);
        free(name); free(value);
    }

    // Release all objects
    for (int i = 0; i < gpCount; ++i) {
        if (nameList[i]) { FreeBSTR(nameList[i]); }
        if (valueList[i]) { FreeBSTR(valueList[i]); }
    }
    ArrayOutFree(nameList);   // Only for SACopyOutParamHelper C arrays
    ArrayOutFree(valueList);
    return 0;
}
