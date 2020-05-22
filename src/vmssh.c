// vmssh.c

#include "vmc.h"

// SSH into VM
int vmSSH(int argc, char *argv[])
{
    char vmName[64] = "", cmd[980] = "";
    if (argc == 2) {
        argCopy(vmName, 64, argv[0]);
        argCopy(cmd, 980, argv[1]);
    }
    else if (argc == 1) {
        argCopy(vmName, 64, argv[0]);
    }
    else {
        printf("Usage: %s ssh <vmName> [<cmd-to-run>]\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Get VM object
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        printf("VM '%s' is not registered\n", vmName);
        Exit(EXIT_FAILURE);
    }

    // See if it's running
    if (VMState(vm) != MachineState_Running) {
        printf("VM '%s' is not running\n", vmName);
        Exit(EXIT_FAILURE);
    }

    return SSHVM(vm, cmd, TRUE);
}


// SSH into VM, raw mode
int SSHVM(IMachine *vm, char *cmd, bool verbose)
{
    char ssh[512] = "ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no";
    sprintf(ssh, "%s -o UserKnownHostsFile=/dev/null -i %s", ssh, vmsshpri);

    char *ip = GetVMProp(vm, "/vm/ip");

    if (!SSHPortOpen(ip)) {
        fprintf(stderr, "VM not reachable over %s:22\n", ip);
        return 1;
    }

    // Run remote SSH command or do interactive logon. Note, vmuser is global
    char sshRun[1980];
    if (*cmd) { sprintf(sshRun, "%s %s@%s \"%s\"", ssh, vmuser, ip, cmd); }
    else { sprintf(sshRun, "%s %s@%s", ssh, vmuser, ip); }

    if (!verbose) { sprintf(sshRun, "%s > /dev/null 2>&1", sshRun); }

    int rc = system(sshRun);
    if (rc < -1) {
        fprintf(stderr, "Error running: %s\n", cmd);
        return 1;    // Some other failure
    }
    return rc;       // Zero means success
}


// Returns TRUE if IP is reachable over SSH port 22
bool SSHPortOpen(char *ip)
{
    if (!ValidIpStr(ip)) { return false; }

    // Create socket
    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        fprintf(stderr, "%s:%d socket create error\n", __FILE__, __LINE__);
        return false;
    }

    // Use non-blocking socket, to save time
    int rc;
#ifdef _WIN32
    rc = ioctlsocket(socket_desc, FIONBIO, 0);
#else
    rc = fcntl(socket_desc, F_SETFL, O_NONBLOCK);
#endif
    if (rc < 0) {
        fprintf(stderr, "%s:%d fcntl error\n", __FILE__, __LINE__);
        return false;
    }

    // Prepare address destination
    struct sockaddr_in addrdst;
    addrdst.sin_family = AF_INET;              // IPV4
    addrdst.sin_addr.s_addr = inet_addr(ip);   // IP address
    addrdst.sin_port = htons(22);              // SSH port

    // Try connection
    connect(socket_desc, (struct sockaddr *)&addrdst, sizeof(addrdst));

#include <sys/select.h>
    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(socket_desc, &fdset);
    tv.tv_sec = 2;                 // Two (2) second timeout
    tv.tv_usec = 0;

    if (select(socket_desc + 1, NULL, &fdset, NULL, &tv) == 1) {
        int so_error;
        socklen_t len = sizeof so_error;
        getsockopt(socket_desc, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            close(socket_desc);
            return true;          // It is reachable
        }
    }

    close(socket_desc);
    return false;         // It is NOT reachable
}


// Secure Copy srcPath to dstPath on given vmName
int SCPVM(char *srcPath, char *vmName, char *dstPath, bool verbose)
{
    // TODO:
    // 1. Do simple (src, dst, verbose) where dst must be a target string in the form
    //    of "<vm>:<filePath>", and if the <vm> part if omitted for src, then we
    //    know it's from this host. 
    
    // Get VM object
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        printf("VM '%s' is not registered\n", vmName);
        return 1;
    }

    // See if it's running
    if (VMState(vm) != MachineState_Running) {
        printf("VM '%s' is not running\n", vmName);
        return 1;
    }

    char scp[512] = "ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no";
    sprintf(scp, "%s -o UserKnownHostsFile=/dev/null -i %s", scp, vmsshpri);

    char *ip = GetVMProp(vm, "/vm/ip");
    if (!SSHPortOpen(ip)) {
        fprintf(stderr, "Cannot SCP to VM IP '%s' over port 22\n", ip);
        return 1;
    }

    // We will copy 'srcPath' file to 'vm:dstPath'
    char sshCopy[1980];
    sprintf(sshCopy, "%s %s %s@%s:%s", scp, srcPath, vmuser, ip, dstPath);

    if (!verbose) { sprintf(sshCopy, "%s > /dev/null 2>&1", sshCopy); }

    // Do SSH Copy
    int rc = system(sshCopy);
    if (rc == -1) {
        fprintf(stderr, "Error running:\n  %s\n", sshCopy);
        return 1;
    }
    return rc;  // Zero means success
}
