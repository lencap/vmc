// vmmod.c

#include "vmc.h"

// Modify VM's CPU and memory setting
void vmMod(int argc, char *argv[])
{
    char vmName[64] = "", cpu[] = "01", mem[] = "01024";
    if (argc == 3) {
        argCopy(vmName, 64, argv[0]);
        argCopy(cpu, 2, argv[1]);
        argCopy(mem, 5, argv[2]);
    }
    else if (argc == 2) {
        argCopy(vmName, 64, argv[0]);
        argCopy(cpu, 2, argv[1]);
    }
    else {
        printf("Usage: %s mod <vmName> <cpus> [<memory>]\n", prgname);
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

    if (!ModVM(vm, cpu, mem)) {
        Exit(EXIT_FAILURE);
    }

    Exit(EXIT_SUCCESS);
}


// Mod VM cpu count and memory size
bool ModVM(IMachine *vm, char *cpuCount, char *memSize)
{
    int vmCpu = (int)atoi(cpuCount);
    int vmMem = (int)atoi(memSize);

    PRUint32 hostCpu;
    IHost_GetProcessorOnlineCount(ihost, &hostCpu);
    if (hostCpu - vmCpu < 2) {
        fprintf(stderr, "This host only has %u CPUs. Assigning %u will oversubscribe it\n",
            hostCpu, vmCpu);
        return false;
    }

    PRUint32 hostMem;
    IHost_GetMemoryAvailable(ihost, &hostMem);
    if (hostMem - vmMem < 8192) {
        fprintf(stderr, "This host only has %uMB of RAM. Assigning %u will oversubscribe it\n",
            hostMem, vmMem);
        return false;
    }

    IMachine *vmMuta = NULL;
    ISession *session = GetSession(vm, LockType_Write, &vmMuta);

    IMachine_SetCPUCount(vmMuta, vmCpu);
    IMachine_SetMemorySize(vmMuta, vmMem);

    CloseSession(session);
    return true;
}
