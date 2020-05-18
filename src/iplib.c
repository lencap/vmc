// iplib.c

#include "vmc.h"

// Find next usable, unique IP in /24 subnet, starting with given IP
char * NextUniqueIP(const char *ip)
{
    // Allocate mem for our next unique IP address string
    char *nextGoodIP = NewString(16);
    strcpy(nextGoodIP, ip);  // Start with given IP 

    // Get /24 network substring of IP
    char *net = GetIPNet(ip);

    // Check if IP is taken/used until we find an available one
    while (UsedIP(nextGoodIP, "")) {
        char *fourth = strrchr(nextGoodIP, '.');   // Get 4th octet substring
        ++fourth;                                  // without the dot

        int fourthInt = atoi(fourth) + 1;              // Increment to next IP in subnet
        if (fourthInt > 254) { fourthInt = 2; }        // Boundary check
        sprintf(nextGoodIP, "%s.%u", net, fourthInt);  // Convert it to string
    }

    free(net);

    // REMINDER: Caller must free allocated memory
    return nextGoodIP;
}


// Check if IP address string is valid
int ValidIpStr(const char *ipString)
{
    struct sockaddr_in destination;
    int rc = inet_pton(AF_INET, ipString, &(destination.sin_addr));
    if (rc < 0) {
        fprintf(stderr, "%s:%d inet_pton error", __FILE__, __LINE__);
        Exit(EXIT_FAILURE);
    }
    return rc;  // 0 = FALSE = Invalid and 1 = TRUE = Valid
}


// Return /24 network part (first 3 octets) of given IP string
char * GetIPNet(const char *ip)
{
    char *net = NewString(16);

    if (!ValidIpStr(ip)) {
        net[0] = '\0';
        return net;      // Returns a null string
    }

    // Walk every char backwards, and break when we find first '.'
    strcpy(net, ip);
    while ( *(net + strlen(net) - 1) != '.' ) {
        *(net + strlen(net) - 1) = '\0';  // Keep shortenting the string
    }
    *(net + strlen(net) - 1) = '\0';  // Shorten last find

    // REMINDER: Caller must free allocated memory
    return net;
}


// Check if IP is already in use. Skip given vmName
BOOL UsedIP(char *ip, char *vmName)
{
    // Update in-memory list of VM if it's empty
    if (!VMList) { UpdateVMList(); }
    // NOTE: UsedIP() gets called a lot, so above check optimizes this function

    // Now parse every VM object in array
    for (int i = 0; i < VMListCount; ++i) {
        BOOL accessible = FALSE;
        IMachine_GetAccessible(VMList[i], &accessible);
        if (!accessible) { continue; }

        // Skip this VM
        char *name = GetVMName(VMList[i]);
        if (Equal(name, vmName)) {
            free(name);    
            continue;
        }
        free(name);

        // Return TRUE if given IP has already been assigned to this other VM
        char *tmpip = GetVMProp(VMList[i], "/vm/ip");
        if (Equal(tmpip, ip)) {
            free(tmpip);        
            return TRUE;
        }
        free(tmpip);
    }

    // Check if IP could also be in use by something other than one our VMs
    //if (ActiveIP(ip)) { return TRUE; } // DISABLED: See comments in ActiveIP()
    
    return FALSE;
}


// Check if IP is active by doing simple ping 
BOOL ActiveIP(char *ip)
{
    // On second thought, let's not use this function. It slows things down a bit too much,
    // so let's put the onus back on the user to allocate IP addresses correctly. We could
    // implement as a FUTURE OPTION, using raw sockets as detailed here:
    // https://stackoverflow.com/questions/7647219/how-to-check-if-a-given-ip-address-is-reachable-in-c-program

    // Send 1 packet and wait 300 milliseconds for a reply. A non-zero
    // value return means the IP is NOT alive, and probably not used
    int rc;
    char cmd[256];
    sprintf(cmd, "ping -c 1 -W 300 %s >/dev/null 2>&1", ip);
    rc = system(cmd);
    if (rc == -1) {
        fprintf(stderr, "Error running: %s\n", cmd);
        Exit(EXIT_FAILURE);
    }
    if (rc == 0) { return TRUE; }
    return FALSE;
}
