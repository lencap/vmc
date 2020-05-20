// vminfo.c

#include "vmc.h"

// Print network adapters
int printNetworkAdapters(IMachine *vm)
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
