// vmprov.c

#include "vmc.h"

// Provision VMs
void vmProv(int argc, char *argv[])
{
    // Create the template file if that's the request
    if (argc == 1 && strlen(argv[0]) == 1 && tolower(argv[0][0]) == 'c') {
        if (isFile(vmconf)) {
            char response;
            printf("File '%s' exists. Overwrite it? y/n ", vmconf);
            scanf("%c", &response);
            if (response != 'y') { Exit(EXIT_SUCCESS); }
        }
        CreateVMConf();
        Exit(EXIT_SUCCESS);
    }

    char *provFile = NewString(257);
    if (argc == 1 && isFile(argv[0])) {
        // Provision with given existing file as requested
        argCopy(provFile, 256, argv[0]);
    }
    else if (isFile(vmconf)) {
        // Provision with default vmconf file, existing in CWD
        strcpy(provFile, vmconf);
    }
    else {
        printf("Usage: %s prov [<vmConf>|c]\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Prohibit running this command from user's HOME directory
    char *cwd = NewString(1024);
    if (getcwd(cwd, 1024) == NULL) {
        fprintf(stderr, "Error with getcwd\n");
        Exit(EXIT_FAILURE);
    }
    char *homedir = NewString(1024);
    homedir = getenv("HOME");
    if (homedir == NULL) {
        fprintf(stderr, "Error with getenv\n");
        Exit(EXIT_FAILURE);
    }
    if (Equal(cwd, homedir)) {
        printf("Running this utility from your HOME directory is highly discouraged for\n"
            "security reasons. Please work from a different, separate working directory.\n");
        Exit(EXIT_FAILURE);
    }
    // if (cwd) { free(cwd); }
    // if (homedir) { free(homedir); }

    ProvisionConfig(provFile);

    Exit(EXIT_SUCCESS);
}


// Provision VM(s) as defined in given INI configuration file
void ProvisionConfig(char *provFile)
{
    // Check INI config file for inconsistencies
    struct ini_t *cfg = ini_load(provFile);
    if (!cfg) {
        fprintf(stderr, "=> Couldn't load file '%s'\n", provFile);
        Exit(EXIT_FAILURE);
    }

    // Get the sections (VMs) defined
    int count;
    const char **sections = ini_GetSections(cfg, &count);
    if (count < 1) {
        fprintf(stderr, "=> No VM sections defined\n");
        Exit(EXIT_FAILURE);
    }

    printf("=> Provisioning %d VM(s) defined in file '%s'\n", count, provFile);

    for (int i = 0; i < count; i++) {
        // printf("[%p]%02d|%s|\n", sections[i], i, sections[i]);
        const char *image = ini_get(cfg, sections[i], "image");
        const char *netip = ini_get(cfg, sections[i], "netip");
        // printf("vm=%s image=%s netip=%s\n", sections[i], image, netip);
        if (image && netip) { continue; }
        fprintf(stderr, "=> Error. Each section (VM) requires at least 'image' and 'netip' defined\n");
        Exit(EXIT_FAILURE);
    }

    // Provision each VM as per its parameters defined in its vmconf section
    for (int i = 0; i < count; i++) {
        // NOTE: We leave an existing VM running if it is both named and configured exactly as
        // defined in vmconf. If it's configured differently, then we'll stop it, modify it
        // according to vmconf, then restart it. If the VM doesn't exist then the process is
        // to simply create a new one.

        // READ VMCONF FILE
        // Get the 8 possible config entries for this VM from the vmconf file

        // #1 name (Mandatory). Same as the section name
        char *vmName = malloc(sizeof(char) * strlen(sections[i]) + 1);
        ExitIfNull(vmName, __FILE__, __LINE__);
        strcpy(vmName, sections[i]);
        printf("[%s] Provisioning this VM\n", vmName);

        // #2 image (Mandatory)
        const char *image = ini_get(cfg, sections[i], "image");
        char *vmImg = malloc(sizeof(char) * strlen(image) + 1);
        ExitIfNull(vmImg, __FILE__, __LINE__);
        strcpy(vmImg, image);
        char vmImgPath[1024] = "";
        sprintf(vmImgPath, "%s%s%s", vmhome, PATHSEPSTR, vmImg);
        if (!isFile(vmImgPath)) {
            fprintf(stderr, "[%s] Image '%s' doesn't exist. Please specify an available image\n",
                vmName, vmImg);
            Exit(EXIT_FAILURE);
        }

        // #3 netip (Mandatory)
        const char *netip = ini_get(cfg, sections[i], "netip");
        if (!ValidIpStr(netip) || endsWith(netip, ".1")) {
            printf("[%s] IP address '%s' is invalid", vmName, netip);
            Exit(EXIT_FAILURE);
        }
        char vmNetIp[16] = "";
        strcpy(vmNetIp, netip);

        // #4 cpus
        const char *cpus = ini_get(cfg, sections[i], "cpus");
        char vmCpus[3] = "1";
        if (cpus) { strcpy(vmCpus, cpus); }

        // #5 memory
        const char *memory = ini_get(cfg, sections[i], "memory");
        char vmMemory[6] = "1024";
        if (memory) { strcpy(vmMemory, memory); }

        // #6 vmcopy
        const char *vmcopy = ini_get(cfg, sections[i], "vmcopy");
        char vmCopy[1024] = "";
        if (vmcopy) { strcpy(vmCopy, vmcopy); }

        // #7 vmrun
        const char *vmrun = ini_get(cfg, sections[i], "vmrun");
        char vmRun[1024] = "";
        if (vmrun) { strcpy(vmRun, vmrun); }

        // #8 nettype
        const char *nettype = ini_get(cfg, sections[i], "nettype");
        char vmNettype[4] = "ho";
        if (nettype) { strcpy(vmNettype, nettype); }

        // Note, basic parameters updates can only be applied when the VM is powered off,
        // which is why we're doing the checks/updates here. If the VM is running and it's
        // already configured as per vmconf then we want to leave it alone

        // Does the VM exist already?
        IMachine *vm = GetVM(vmName);
        if (vm) {
            // Yes, it does. We'll ensure its configured as per vmconf 
            printf("[%s] This VM already exists\n", vmName);
        }
        else {
            // No, it does not. Let's create it
            vm = CreateVM(vmName, vmImgPath);
            if (!vm) {
                fprintf(stderr, "[%s] Error creating this VM\n", vmName);
                Exit(EXIT_FAILURE);
            }
        }
        // By now 'vm' is a handle to the VM being update/created

        // COMPARE TO EXISTING VM VALUES
        // Next, let's assume VM is already configured as per vmconf, by checking
        // each of the 8 parameters on the VM itself, to see if any of them is false.
        bool sameConfig = true;

        // Compare IP Address
        char *currentVmNetIp = GetVMProp(vm, "/vm/ip");
        if (!Equal(currentVmNetIp, vmNetIp)) {
            // Check if proposed IP is already taken by another VM
            if (UsedIP(vmNetIp, vmName)) {
                fprintf(stderr, "[%s] IP '%s' is already taken by another VM\n", vmName, vmNetIp);
                Exit(EXIT_FAILURE);
            }
            sameConfig = false;
            printf("[%s] Setting IP address to '%s'\n", vmName, vmNetIp);
        }

        // Compare CPU count
        ULONG cpuCount;
        IMachine_GetCPUCount(vm, &cpuCount);
        if (cpuCount != (ULONG)atoi(vmCpus)) {
            sameConfig = false;
            printf("[%s] Setting CPU count to '%s'\n", vmName, vmCpus);
        }

        // Compare Memory setting
        ULONG memsize;
        IMachine_GetMemorySize(vm, &memsize);
        if (memsize != (ULONG)atoi(vmMemory)) {
            sameConfig = false;
            printf("[%s] Setting memory to '%s'\n", vmName, vmMemory);
        }

        // Now perform updates, if any one parameter was different
        if (sameConfig) {
            printf("[%s] VM already configured as per vmconf. Done.\n", vmName);
            // Start machine if not already running
            if (VMState(vm) != MachineState_Running) {
                if (!StartVM(vm, "headless")) {
                    fprintf(stderr, "[%s] Error starting this VM\n", vmName);
                    Exit(EXIT_FAILURE);
                }
            }
        }
        else {
            printf("[%s] Applying configurations\n", vmName);
            // Stop machine if already running
            if (VMState(vm) == MachineState_Running) {
                if (!StopVM(vm)) {
                    fprintf(stderr, "[%s] Error stopping this VM\n", vmName);
                    Exit(EXIT_FAILURE);
                }
            }

            // Let's not exit on any of these errors, so we continue with other VMs 
            if (!SetVMIP(vm, vmNetIp)) {
                fprintf(stderr, "[%s] Error updating IP address!\n", vmName);
            }

            if (!ModVM(vm, vmCpus, vmMemory)) {
                fprintf(stderr, "[%s] Error updating CPU count and/or memory size!\n", vmName);
            }

            if (!SetVMNetType(vm, vmNettype)) {
                fprintf(stderr, "[%s] Error updating net type!\n", vmName);
            }

            if (!StartVM(vm, "headless")) {
                fprintf(stderr, "[%s] Error starting this VM!\n", vmName);
            }
        }
    
        // Run VMCOPY COMMAND
        if (vmCopy[0] != '\0') {
            int i = 0;
            char **vmCopyList = strSplit(vmCopy, ' ', &i);

            if (i != 2) {
                fprintf(stderr, "[%s] Error with 'vmcopy' entry:\n"
                    "It should be a SOURCE local file, separated by a single space,\n"
                    "then the full path destination of the TARGET file within the VM,\n"
                    "surrounded by double-quote. See example in skeleton file.\n", vmName);
                Exit(EXIT_FAILURE);
            }
            printf("%s: VMCOPY: '%s'\n", vmName, vmCopy);
            char *source = vmCopyList[0];
            char *destination = vmCopyList[1];

            // Since the VM may only have started a moment ago, let's give SSH time to be ready
            int delay = 600;
            while (!sshAccess(vmNetIp) && delay > 0) {
                usleep(100000);  // Do nothing for .1 second
                --delay;
            }
            if (SCPVM(source, vmName, destination, true)) {
                fprintf(stderr, "%s: Error with VMCOPY!\n", vmName);
            }
        }

        // Run VMRUN COMMAND
        if (vmRun[0] != '\0') {
            printf("%s: VMRUN: '%s'\n", vmName, vmRun);
            if (SSHVM(vm, vmRun, true)) {
                fprintf(stderr, "%s: Error with VMRUN!\n", vmName);
            }
        }
    }

    free(sections);
    ini_free(cfg);
}


// Create skeleton provisioning config file
void CreateVMConf(void)
{
    FILE *fp = fopen(vmconf, "w");
    if (!fp) {
        fprintf(stderr, "Error creating '%s'\n", vmconf);
        Exit(EXIT_FAILURE);
    }
    fprintf(fp, "# vm.conf\n"
        "# Running '%s prov' in a directory with this file in it will automatically\n"
        "# provision the VMs defined here. Each VM requires its own section name,\n"
        "# which becomes the VM name. Then there are 7 other possible keys you can\n"
        "# define. Two of which are mandatory (image and netip). The other 5 (cpus,\n"
        "# memory, vmcopy, vmrun, and nettype) are optional. Lines starting with a\n"
        "# hash(#) are treated as comments. Spaces can only be used within double\n"
        "# quotes (\"). vmcopy and vmrun are perfect for copying/running bootstrapping\n"
        "# scripts. Note these last 2 can only appear once a piece, as duplicate keys\n"
        "# are not yet allowed. It is best to put everything inside just one bootstrapping\n"
        "# script. You can also name this file anything you want and provision with\n"
        "# '%s prov MYFILE'.\n\n"
        "#[dev1]\n"
        "#image   = centos72003.ova\n"
        "#netip   = 10.11.12.2\n"
        "#cpus    = 1\n"
        "#memory  = 1024\n"
        "#vmcopy  = \"./bootstrap.sh /tmp/bootstrap.sh\"\n"
        "#vmrun   = \"/tmp/bootstrap.sh\"\n\n"
        "#[dev2]\n"
        "#image   = ubuntu1804.ova\n"
        "#netip   = 10.11.12.3\n"
        "#nettype = bri\n", prgname, prgname);
    fclose(fp);
    // Not entirely clear to me why this conversion is needed
    int mode = strtol("0644", 0, 8);
    if (chmod(vmconf, mode) < 0) {
        fprintf(stderr, "Error chmod 600 %s\n", vmconf);
        Exit(EXIT_FAILURE);
    }
}
