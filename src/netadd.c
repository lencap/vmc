// netadd.c

#include "vmc.h"

// Add a new HostOnly NIC to this host, using given gateway IP address
void netAdd(int argc, char *argv[])
{
    // Accept one argument only
    if (argc != 1) {
        printf("Usage: %s netadd <gateway-ip>\n", prgname);
        Exit(EXIT_FAILURE);
    }
    char ip[32] = "";
    strcpy(ip, argv[0]);

    if (!ValidIpStr(ip)) {
        printf("'%s' is not a valid IP.\n", ip);
        Exit(EXIT_FAILURE);
    }

    if (!endsWith(ip, ".1")) {
        fprintf(stderr, "IP does not end in .1\n");
        Exit(EXIT_FAILURE);
    }
    
    // Create new NIC on host
    char *newHOName = CreateHONIC(ip);
    free(newHOName);
}


// Create a new HostOnly NIC on this host
char * CreateHONIC(char *ip)
{
    IHostNetworkInterface *newNIC = NULL;
    IProgress *progress = NULL;
    HRESULT rc = IHost_CreateHostOnlyNetworkInterface(ihost, &newNIC, &progress);
    HandleProgress(progress, rc, -1);  // Timeout of -1 means wait indefinitely

    // Abort if new NIC does not have a proper name
    BSTR name_16;
    char *name;
    IHostNetworkInterface_GetName(newNIC, &name_16);
    Convert16to8(name_16, &name);
    FreeBSTR(name_16);
    if (Equal(name, "")) {
        fprintf(stderr, "Error creating new vboxnet");
        return "";   // Null string
    }

    // Assign given IP address to the new vboxnet
    BSTR ip_16, netmask_16;
    char netmask[] = "255.255.255.0";
    Convert8to16(ip, &ip_16);
    Convert8to16(netmask, &netmask_16);
    rc = IHostNetworkInterface_EnableStaticIPConfig(newNIC, ip_16, netmask_16);
    FreeBSTR(ip_16);
    FreeBSTR(netmask_16);
    if (FAILED(rc)) {
        fprintf(stderr, "%s:%d IHostNetworkInterface_EnableStaticIPConfig error\n", __FILE__, __LINE__);
        PrintVBoxException();
        return "";   // Null string
    }    

    return name; 
}
