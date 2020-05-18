// netdel.c

#include "vmc.h"

// Delete given HostOnly network name
void netDel(int argc, char *argv[])
{
    // Accept one argument only
    if (argc != 1) {
        printf("Usage: %s netdel <vboxnetX>\n", prgname);
        Exit(EXIT_FAILURE);
    }
    char vboxnet[64] = "";
    strcpy(vboxnet, argv[0]);

    // Find the NIC object with this name, if it exists
    BSTR name_16;
    Convert8to16(vboxnet, &name_16);
    IHostNetworkInterface *nic = NULL;
    HRESULT rc = IHost_FindHostNetworkInterfaceByName(ihost, name_16, &nic);
    FreeBSTR(name_16);
    if (FAILED(rc) || !nic) {
        fprintf(stderr, "HostOnly network '%s' doesn't exist.\n", vboxnet);
        Exit(EXIT_FAILURE);
    }

    // Get its ID
    PRUnichar *id;
    IHostNetworkInterface_GetId(nic, &id);

    // Delete this NIC by ID
    IProgress *progress = NULL;
    rc = IHost_RemoveHostOnlyNetworkInterface(ihost, id, &progress);
    HandleProgress(progress, rc, -1);  // Timeout of -1 means wait indefinitely
}
