// vmip.c

#include "vmc.h"

// Manually set VM IP address
void vmIP(int argc, char *argv[])
{
    char vmName[64] = "", ip[32] = "";
    if (argc == 2) {
        argCopy(vmName, 64, argv[0]);
        argCopy(ip, 32, argv[1]);
    }
    else {
        printf("Usage: %s ip <vmName> <ip>\n", prgname);
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

    if (!SetVMIP(vm, ip)) {
        Exit(EXIT_FAILURE);
    }

    Exit(EXIT_SUCCESS);
}


// Set up IP and network devices, and store details in VM property area
bool SetVMIP(IMachine *vm, char *ip)
{
    if (!ValidIpStr(ip)) {
        printf("'%s' is not a valid IP.\n", ip);
        return false;
    }

    if (endsWith(ip, ".1")) {
        fprintf(stderr, "IP ending in .1 is reserved for gateway devices\n");
        return false;
    }

    char *vmName = GetVMName(vm);

    // Abort if IP is already taken
    if (UsedIP(ip, vmName)) {
        printf("IP '%s' is already in used by another VM,"
            "or a host in same network.\n", ip);
        return false;
    }

    // Get new ISession object - Type2
    IMachine *vmMuta = NULL;   // For temporary mutable VM object
    ISession *session = GetSession(vm, LockType_Write, &vmMuta);
    // Note: From here on we're modifying the writeable vmMuta object

    // Get /24 network address (first 3 octets)
    char *ipNet = GetIPNet(ip);

    // Set up networking based on netType
    char *netType = GetVMProp(vmMuta, "/vm/nettype");
    if (Equal(netType, "bri")) {
        // BRIDGED NETWORKING
        // DISABLE enp0s3 w/ Null. No networking
        INetworkAdapter *nic0 = NULL;
        HRESULT rc = IMachine_GetNetworkAdapter(vmMuta, 0, &nic0);
        ExitIfFailure(rc, "IMachine_GetNetworkAdapter", __FILE__, __LINE__);
        INetworkAdapter_SetEnabled(nic0, FALSE);

        // ENABLE enp0s8 w/ Bridge networking. Real-world access (assumes local admin privs)
        INetworkAdapter *nic1 = NULL;
        rc = IMachine_GetNetworkAdapter(vmMuta, 1, &nic1);
        ExitIfFailure(rc, "IMachine_GetNetworkAdapter", __FILE__, __LINE__);
        INetworkAdapter_SetEnabled(nic1, TRUE);
        INetworkAdapter_SetAdapterType(nic1, NetworkAdapterType_Virtio);
        INetworkAdapter_SetAttachmentType(nic1, NetworkAttachmentType_Bridged);

        // Find and use host's main NIC to bridge to
        char *mainNIC = getHostMainNIC();
        BSTR mainNIC_16;
        Convert8to16(mainNIC, &mainNIC_16);

        rc = INetworkAdapter_SetBridgedInterface(nic1, mainNIC_16);
        ExitIfFailure(rc, "INetworkAdapter_SetBridgedInterface", __FILE__, __LINE__);
        FreeBSTR(mainNIC_16);
        free(mainNIC);
    }
    else {
        // HOSTONLY NETWORKING
        // ENABLE enp0s3 w/ NAT networking, to access external world, and no SSH port forwarding
        INetworkAdapter *nic0 = NULL;
        HRESULT rc = IMachine_GetNetworkAdapter(vmMuta, 0, &nic0);
        ExitIfFailure(rc, "IMachine_GetNetworkAdapter", __FILE__, __LINE__);
        INetworkAdapter_SetEnabled(nic0, TRUE);
        INetworkAdapter_SetAdapterType(nic0, NetworkAdapterType_Virtio);
        INetworkAdapter_SetAttachmentType(nic0, NetworkAttachmentType_NAT);
        INATEngine *natEng = NULL;
        rc = INetworkAdapter_GetNATEngine(nic0, &natEng);
        ExitIfFailure(rc, "INetworkAdapter_GetNATEngine", __FILE__, __LINE__);
        INATEngine_SetDNSPassDomain(natEng, TRUE);
        INATEngine_SetDNSUseHostResolver(natEng, TRUE);

        // ENABLE enp0s8 w/ HostOnly networking. Network connectivity between VMs only
        INetworkAdapter *nic1 = NULL;
        rc = IMachine_GetNetworkAdapter(vmMuta, 1, &nic1);
        ExitIfFailure(rc, "IMachine_GetNetworkAdapter", __FILE__, __LINE__);
        INetworkAdapter_SetEnabled(nic1, TRUE);
        INetworkAdapter_SetAdapterType(nic1, NetworkAdapterType_Virtio);
        INetworkAdapter_SetAttachmentType(nic1, NetworkAttachmentType_HostOnly);

        // Search all current HostOnly networks, and if given IP address fits into
        // one of them, let's put it in that vboxnet, else create a new one for it

        // Get all interfaces and loop through them
        ULONG nicCount = 0;
        IHostNetworkInterface **nicList = NULL;
        GetNICList(&nicList, &nicCount);

        if (!nicCount) {
            fprintf(stderr, "This host has no NICs??");
            Exit(EXIT_FAILURE);
        }

        char newHOName[64] = "";
        
        ULONG i;
        for (i = 0; i < nicCount; ++i) {
            IHostNetworkInterface *nic = nicList[i];
            if (!nic) { continue; }

            ULONG iType;
            IHostNetworkInterface_GetInterfaceType(nic, &iType);
            if (iType != HostNetworkInterfaceType_HostOnly) { continue; }  // Only HostOnly type

            BSTR name_16;
            char *name;
            IHostNetworkInterface_GetName(nic, &name_16);
            Convert16to8(name_16, &name);
            FreeBSTR(name_16);

            BSTR nicIP_16;
            char *nicIP;
            IHostNetworkInterface_GetIPAddress(nic, &nicIP_16);
            Convert16to8(nicIP_16, &nicIP);
            FreeBSTR(nicIP_16);
            char *nicIPnet = GetIPNet(nicIP);
            if (Equal(ipNet, nicIPnet)) {
                strcpy(newHOName, name);
                break;
            }
        }
        // Release objects in the array
        for (i = 0; i < nicCount; ++i) {
            if (nicList[i]) { IHostNetworkInterface_Release(nicList[i]); }
        }
        ArrayOutFree(nicList);        

        // If not assigned by above loop, it means we have to create a new vboxnet
        if (newHOName[0] == '\0') {
            char tmpIP[16];
            sprintf(tmpIP, "%s.1", ipNet);
            char *tmpHOName = CreateHONIC(tmpIP);
            strcpy(newHOName, tmpHOName);
            free(tmpHOName);
        }

        // Set the HostOnly interface name
        BSTR newHOName_16;
        Convert8to16(newHOName, &newHOName_16);
        rc = INetworkAdapter_SetHostOnlyInterface(nic1, newHOName_16);
        if (FAILED(rc)) {
            fprintf(stderr, "%s:%d INetworkAdapter_SetHostOnlyInterface error\n",
                __FILE__, __LINE__);
            PrintVBoxException();
            return false;
        }
        FreeBSTR(newHOName_16);
    }

    // Let's store these essential details in the Guest Additions property area of
    // the VM, under path '/vm/*'. VMs using this program need to set up the
    // /usr/local/bin/vmnet utility to query this area using GuestAdditions tool,
    // to properly set up networking on the VM during boot up.
    SetVMProp(vmMuta, "/vm/name", vmName);
    SetVMProp(vmMuta, "/vm/nettype", netType);
    SetVMProp(vmMuta, "/vm/ip", ip);
    SetVMProp(vmMuta, "/vm/netmask", "255.255.255.0");
    char broadcast[32];
    sprintf(broadcast, "%s.255", ipNet);
    SetVMProp(vmMuta, "/vm/broadcast", broadcast);

    CloseSession(session);

    free(vmName);

    return true;
}
