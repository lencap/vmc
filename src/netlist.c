// netlist.c

#include "vmc.h"

// Print list of all NIC interfaces on this host
void netList(void)
{
    // Get list of NICs
    ULONG nicCount = 0;
    IHostNetworkInterface **nicList = NULL;
    GetNICList(&nicList, &nicCount);

    if (!nicCount) {
        fprintf(stderr, "This host has no NICs??");
        Exit(EXIT_FAILURE);
    }

    // Header
    printf("%-24s%-12s%-12s%-16s%-16s%s\n", 
        "NAME", "TYPE", "DHCP", "IPADDRESS", "NETMASK", "STATUS");

    // Now parse every object in array
    ULONG i;
    for (i = 0; i < nicCount; ++i) {
        IHostNetworkInterface *nic = nicList[i];
        if (!nic) {
            printf("NULL valued NIC found??\n");
            continue;
        }
        // Get NIC name
        BSTR name_16;
        char *name;
        IHostNetworkInterface_GetName(nic, &name_16);
        Convert16to8(name_16, &name);
        FreeBSTR(name_16);
        // NIC type
        ULONG type;
        IHostNetworkInterface_GetInterfaceType(nic, &type);
        char nicType[16];
        strcpy(nicType, NICifType[type]);
        // DHCP status
        BOOL dhcp;
        IHostNetworkInterface_GetDHCPEnabled(nic, &dhcp);
        char dhcpStatus[] = "Disabled";
        if (dhcp) { strcpy(dhcpStatus, "Enabled"); }
        // IP address
        BSTR ip_16;
        char *ip;
        IHostNetworkInterface_GetIPAddress(nic, &ip_16);
        Convert16to8(ip_16, &ip);
        FreeBSTR(ip_16);
        // IP netmask
        BSTR netMask_16;
        char *netMask;
        IHostNetworkInterface_GetNetworkMask(nic, &netMask_16);
        Convert16to8(netMask_16, &netMask);
        FreeBSTR(netMask_16);
        // NIC status
        ULONG status;
        IHostNetworkInterface_GetStatus(nic, &status);
        char nicStatus[16];
        strcpy(nicStatus, NICStatus[status]);

        // Print 'em out
        printf("%-24s%-12s%-12s%-16s%-16s%s\n", 
            name, nicType, dhcpStatus, ip, netMask, nicStatus);
        // Free some memory
        free(name);
        free(ip);
        free(netMask);
    }
    // Release objects in the array
    for (i = 0; i < nicCount; ++i) {
        if (nicList[i]) { IHostNetworkInterface_Release(nicList[i]); }
    }    
    ArrayOutFree(nicList);

}


// Get list of NICs on this host
void GetNICList(IHostNetworkInterface ***List, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *SA = SAOutParamAlloc();

    HRESULT rc = IHost_GetNetworkInterfaces(ihost,
        ComSafeArrayAsOutIfaceParam(SA, IHostNetworkInterface *));
    ExitIfFailure(rc, "GetNetworkInterfaces", __FILE__, __LINE__);

    // REMINDER: Caller must free allocated memory

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)List, Count, SA);
    SADestroy(SA);
}


// Get this host's main NIC
char * GetHostMainNIC(void)
{
    // WARNING!
    // OS DEPENDENT! Assumes Apple macOS, where main NIC names start with 'en'
    // DUBIOUS LOGIC! We can never really know what a host's main NIC really is,
    // so this is really just guess work, but it should work in most cases.
    
    // Get list of all NICs on this host
    ULONG nicCount = 0;
    IHostNetworkInterface **nicList = NULL;
    GetNICList(&nicList, &nicCount);

    if (!nicCount) {
        fprintf(stderr, "FATAL. This host has no NICs??");
        Exit(EXIT_FAILURE);
    }

    char *hostMainNic = NewString(64);
    
    char *nicName;
    BOOL found = FALSE;
    ULONG i;
    for (i = 0; i < nicCount; ++i) {
        IHostNetworkInterface *nic = nicList[i];
        if (!nic) { continue; }
        ULONG iType;
        IHostNetworkInterface_GetInterfaceType(nic, &iType);
        if (iType != HostNetworkInterfaceType_Bridged) { continue; }  // Skip unless it's Bridged
        // Skip ones whose names does not start with 'en'
        // This logic will need revision for non-macOS OSes
        BSTR name_16;
        IHostNetworkInterface_GetName(nic, &name_16);
        Convert16to8(name_16, &nicName);
        FreeBSTR(name_16);
        if (nicName[0] != 'e'  && nicName[1] != 'n') {
            free(nicName);
            continue;
        }

        // Skip ones with 0.0.0.0 IP address
        BSTR ip_16;
        char *ip;
        IHostNetworkInterface_GetIPAddress(nic, &ip_16);
        Convert16to8(ip_16, &ip);
        FreeBSTR(ip_16);
        if (Equal(ip, "0.0.0.0")) {
            free(ip);
            continue;
        }

        // If we get here, condition must be true for first 'en*' named NIC with a non 0.0.0.0 IP 
        found = TRUE;
        break;
    }

    // Release objects in the array
    for (i = 0; i < nicCount; ++i) {
        if (nicList[i]) { IHostNetworkInterface_Release(nicList[i]); }
    }    
    ArrayOutFree(nicList);

    // Last nicName is our answer
    if (found) { strcpy(hostMainNic, nicName); }
    free(nicName);
    
    if (Equal(hostMainNic, "")) {
        fprintf(stderr, "FATAL: Couldn't locate this host's main NIC.\n");
        Exit(EXIT_FAILURE);
    }
    return hostMainNic;
}
