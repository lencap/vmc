// vmcreate.c

#include "vmc.h"

// Create VM
void vmCreate(int argc, char *argv[])
{
    char vmName[64] = "", image[256] = "";
    if (argc == 2) {
        argCopy(vmName, 64, argv[0]);
        argCopy(image, 256, argv[1]);
    }
    else {
        printf("Usage: %s create <vmName> <[ovaFile|imgName]>\n", prgname);
        Exit(EXIT_FAILURE);
    }

    // Lookup VM object
    IMachine *vm = GetVM(vmName);
    if (vm) {
        fprintf(stderr, "There's already a VM registered as '%s'\n", vmName);
        Exit(EXIT_FAILURE);
    }
    
    // First, let's find out if the 'image' argument is an OVA file,
    // or the name of one of our registered images.

    // First, let's presume the registered image's full filepath
    char imgFile[1024];
    char *imageBase = baseName(image);   // Careful, just a pointer, not a new string
    sprintf(imgFile, "%s%s%s", vmhome, PATHSEPSTR, imageBase);

    if (isFile(image) && endsWith(image, ".ova") && isTarFile(image)) {
        // FIRST option: User has given us a proper OVA file
        strcpy(imgFile, image);  // Make it the image file
    }
    else if (isFile(imgFile)) {
        // SECOND option: User is referencing a registered image
        ; // Nothing to do. We've already set up the imgFile path
    }
    else {
        fprintf(stderr, "'%s' is neither a proper OVA file, nor a registered image\n", image);
        Exit(EXIT_FAILURE);
    }

    // Create the VM
    vm = CreateVM(vmName, imgFile);

    Exit(EXIT_SUCCESS);
}


// Create VM with validated vmName and imgFile path arguments
IMachine * CreateVM(char *vmName, char *imgFile)
{
    // FIRST, inspect the OVA before we import/create the new VM
    
    // Create an empty IAppliance object
    IAppliance *appliance = NULL;
    IVirtualBox_CreateAppliance(vbox, &appliance);
    ExitIfNull(appliance, "IVirtualBox_CreateAppliance", __FILE__, __LINE__);

    // Read OVA file into this IAppliance object
    BSTR imgFile_16;
    Convert8to16(imgFile, &imgFile_16);  // Convert imgFile path to API UTF16
    IProgress *progress = NULL;
    HRESULT rc = IAppliance_Read(appliance, imgFile_16, &progress);
    FreeBSTR(imgFile_16);
    HandleProgress(progress, rc, -1);  // Note, rc is used here

    // Populate the VirtualSystemDescriptions object within the appliance object
    rc = IAppliance_Interpret(appliance);
    if (FAILED(rc)) {
        PrintVBoxException();
        Exit(EXIT_FAILURE);
    }

    // Check for list of warnings during OVA interpretation of appliance
    ULONG warnCount = 0;
    BSTR *warnings = getWarningsList(appliance, &warnCount);
    for (int i = 0; i < warnCount; ++i) {
        char *warn;
        Convert16to8(warnings[i], &warn);
        fprintf(stderr, "%s\n", warn);
        free(warn);
    }
    for (int i = 0; i < warnCount; ++i) {
        if (warnings[i]) { FreeBSTR(warnings[i]); }
    }
    if (warnings) { ArrayOutFree(warnings); }

    // Get list of VM VirtualSystemDescription (VSD) in this OVA. Although
    // rarely used, the OVF standard allows for an OVA to contain
    // more than just one (1) VM definition, so here we check for that
    ULONG sysVSDCount = 0;
    IVirtualSystemDescription **sysVSDList = GetSysVSDList(appliance, &sysVSDCount);
    // This array and its elements are released at the end of this functions

    // We only support OVAs with ONE system VSD
    if (sysVSDCount == 0) {
        fprintf(stderr, "Zero (0) VMs described in OVA.\n");
        Exit(EXIT_FAILURE);
    }
    else if (sysVSDCount > 1) {
        fprintf(stderr, "More than one (1) VM defined in OVA. Unsupported.\n");
        Exit(EXIT_FAILURE);
    }

    // ---------------------------
    // At this point sysVSDList[0] holds our single VM definition
    // ---------------------------

    // BACKGROUND: A VM "VSD" is made up of five (5) different arrays:
    // 1. Types: A ULONG array to hold the respective VirtualSystemDescriptionType type element
    //      01 = Ignore                  02 = OS                     03 = Name
    //      04 = Product                 05 = Vendor                 06 = Version
    //      07 = ProductUrl              08 = VendorUrl              09 = Description
    //      10 = License                 11 = Miscellaneous          12 = CPU
    //      13 = Memory                  14 = HardDiskControllerIDE  15 = HardDiskControllerSATA
    //      16 = HardDiskControllerSCSI  17 = HardDiskControllerSAS  18 = HardDiskImage
    //      19 = Floppy                  20 = CDROM                  21 = NetworkAdapter
    //      22 = USBController           23 = SoundCard              24 = SettingsFile
    //      25 = BaseFolder              26 = PrimaryGroup           27 = CloudInstanceShape
    //      ... there are others, but at the moment we don't care about them ...
    // 2. Refs: A BSTR array used by some of the above Types element to store some values
    // 3. OVFValues: A BSTR array of all original OVA values for each Types element
    // 4. VBoxValues: A BSTR array of what values to use for each Types when creating a new VM
    // 5. ExtraConfigValues: A BSTR array of additional values for each Types

    // Note that the index in each array corresponds to values the other arrays. So when
    // Types[2] = 3 (machine name), then the actual name is stored in the VBoxValues[2].
    // Please read the SDK documentation pages 383+ for more info on this.
    
    // Also note that the most important value for each type description is whether or
    // not it is ENABLED, and that's a boolean array WE HAVE TO create new ourselves,
    // separately (that I calling Enabled here), which we will later pass to the
    // IVirtualSystemDescription_SetFinalValues function, to create the VM using this OVA.

    // Before we even get started, let's remove any HardDiskControllerIDE types,
    // since we never use those for development VMs, just SATA ones.
    rc = IVirtualSystemDescription_RemoveDescriptionByType(sysVSDList[0],
        VirtualSystemDescriptionType_HardDiskControllerIDE);
    if (FAILED(rc)) {
        PrintVBoxException();
        Exit(EXIT_FAILURE);
    }

    // Get the 5 individual VSD arrays that describe our sole sysVSDList[0] VM object
    ULONG vsdCount = 0;   // Virtual System Description entry index counter
    ULONG *Types            = NULL;
    BSTR *Refs              = NULL;
    BSTR *OVFValues         = NULL;
    BSTR *VBoxValues        = NULL;
    BSTR *ExtraConfigValues = NULL;
    GetVSDArrays(sysVSDList[0], &vsdCount, &Types, 
        &Refs, &OVFValues, &VBoxValues, &ExtraConfigValues);

    // Create and inititialize our boolean Enable array
    BOOL *Enabled = malloc(sizeof(BOOL) * vsdCount);
    ExitIfNull(Enabled, "malloc", __FILE__, __LINE__);
    // Start off assuming everything will be enabled, then selectibly disable in below for loop
    for (int i = 0; i < vsdCount; ++i) { Enabled[i] = TRUE; }

    // Update the five(5) lists to create a VM with the following values
    for (int i = 0; i < vsdCount; ++i) {
        // Disable all virtual hardware types that are hardly ever used in most VM
        // configurations used for development (and that we're allow to disable)
        if (Types[i] == VirtualSystemDescriptionType_HardDiskControllerIDE ||   // 14
            Types[i] == VirtualSystemDescriptionType_HardDiskControllerSCSI ||  // 16
            Types[i] == VirtualSystemDescriptionType_Floppy ||                  // 19
            Types[i] == VirtualSystemDescriptionType_CDROM ||                   // 20
            Types[i] == VirtualSystemDescriptionType_USBController ||           // 22
            Types[i] == VirtualSystemDescriptionType_SoundCard ) {              // 23
            Enabled[i] = FALSE;
            continue;
        }
        // Set Name of VM
        if (Types[i] == VirtualSystemDescriptionType_Name) {  // 03
            Convert8to16(vmName, &VBoxValues[i]);
            continue;
        }
        // Set CPU count
        if (Types[i] == VirtualSystemDescriptionType_CPU) {  // 12
            Convert8to16("1", &VBoxValues[i]);
            continue;
        }
        // Set Memory size
        if (Types[i] == VirtualSystemDescriptionType_Memory) {  // 13
            Convert8to16("1024", &VBoxValues[i]);
            continue;
        }
        // Set HardDiskImage path and parameters
        if (Types[i] == VirtualSystemDescriptionType_HardDiskImage) {  // 18
            Convert8to16("1024", &VBoxValues[i]);
            char hddpath[512];
            sprintf(hddpath, "%s%s%s%shd1.vmdk", vbhome, PATHSEPSTR, vmName, PATHSEPSTR);
            Convert8to16(hddpath, &VBoxValues[i]);
            Convert8to16("controller=6;channel=0", &ExtraConfigValues[i]);
            continue;
        }
        // Set NetworkAdapter in slot 1 to NAT
        if (Types[i] == VirtualSystemDescriptionType_NetworkAdapter) {  // 21
            Convert8to16("NAT", &VBoxValues[i]);
            Convert8to16("slot=0;type=NAT", &ExtraConfigValues[i]);
            continue;
        }
        // Set SettingsFile path
        if (Types[i] == VirtualSystemDescriptionType_SettingsFile) {  // 24
            char sfilepath[512];
            sprintf(sfilepath, "%s%s%s%s%s.vbox", vbhome, PATHSEPSTR,
                vmName, PATHSEPSTR, vmName);
            Convert8to16(sfilepath, &VBoxValues[i]);
        }
    }

    // // DEBUG
    // printf("UPDATED\n");
    // printf("[%2s][%-2s_%-28s][%-6s][%-12s][%-50s][%s]\n", "i", "xx",
    //     "aType", "Enable", "aRef", "OVFval", "ExtraCfg");
    // for (int i = 0; i < vsdCount; ++i) {
    //     char *ref;
    //     Convert16to8(Refs[i], &ref);
    //     char *val;
    //     Convert16to8(VBoxValues[i], &val);
    //     char *ext;
    //     Convert16to8(ExtraConfigValues[i], &ext);
    //     printf("[%2d][%02d_%-28s][%-6s][%-12s][%-50s][%s]\n", i, Types[i],
    //         VSDType[Types[i]], BoolStr[Enabled[i]], ref, val, ext);
    //     free(ref); free(val); free(ext);
    // }

    // Call IVirtualSystemDescription::setFinalValues() with the 3 updated VSD arrays
    UpdateVSDArrays(sysVSDList[0], vsdCount,
        Enabled, VBoxValues, ExtraConfigValues);

    SAFEARRAY *OptionsSA = NULL;
    // The import Options list needs to be a SAFEARRAY of ULONG. An empty(NULL) array
    // means new MAC addresses will be generated for all NICs. Having elements '1' and '2'
    // would mean DO NOT generate new MAC addresses for regular(1) and NAT(2) interfaces.

    // Import the appliance, create the VM, and wait till it's complete
    progress = NULL;
    rc = IAppliance_ImportMachines(appliance,
        ComSafeArrayAsInParam(OptionsSA),
        &progress);
    HandleProgress(progress, rc, -1);  // Timeout of -1 means wait indefinitely

    // Confirm VM was indeed created
    UpdateVMList();   // But first, let's update the master VMList
    IMachine *vm = GetVM(vmName);
    if (!vm) {
        fprintf(stderr, "Error creating VM '%s'\n", vmName);
        PrintVBoxException();
        Exit(EXIT_FAILURE);
    }

    // Ensure disk1 is the only bootable device on this VM
    IMachine *vmMuta = NULL;
    ISession *session = GetSession(vm, LockType_Write, &vmMuta);
    IMachine_SetBootOrder(vmMuta, 1, DeviceType_HardDisk);
    IMachine_SetBootOrder(vmMuta, 2, DeviceType_Null);
    // We don't bother with order 3, 4 or others, since they are hardly used
    
    // Some default network settings
    SetVMProp(vmMuta, "/vm/nettype", "ho");

    CloseSession(session);

    // Finally, let's make sure VM is set up with next unique IP address
    char *ip = NextUniqueIP(vmdefip);

    SetVMIP(vm, ip);   // Set IP on new VM

    // Free memory
    free(ip);
    for (int i = 0; i < vsdCount; ++i) {
        // No need to free Enabled[i], since they are not pointers
        // Nor are Types[i]
        if (Refs[i]) { FreeBSTR(Refs[i]); }
        if (OVFValues[i]) { FreeBSTR(OVFValues[i]); }
        if (VBoxValues[i]) { FreeBSTR(VBoxValues[i]); }
        if (ExtraConfigValues[i]) { FreeBSTR(ExtraConfigValues[i]); }
    }
    if (Enabled) { free(Enabled); }
    if (Types) { ArrayOutFree(Types); }
    if (Refs) { ArrayOutFree(Refs); }
    if (OVFValues) { ArrayOutFree(OVFValues); }
    if (VBoxValues) { ArrayOutFree(VBoxValues); }
    if (ExtraConfigValues) { ArrayOutFree(ExtraConfigValues); }

    if (sysVSDList[0]) { IVirtualSystemDescription_Release(sysVSDList[0]); }
    if (sysVSDList) { ArrayOutFree(sysVSDList); }

    return vm;
}


// Get list of warning messages from OVA interpretation appliance
BSTR * getWarningsList(IAppliance *appliance, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *SA = SAOutParamAlloc();

    IAppliance_GetWarnings(appliance,
        ComSafeArrayAsOutIfaceParam(SA, PRUnichar *));
    ExitIfNull(SA, "GetWarnings", __FILE__, __LINE__);    
    
    // REMINDER: Caller must free allocated memory
    BSTR *List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, SA);
    SADestroy(SA);
    return List;
}


// Get list of system descriptions in appliance
IVirtualSystemDescription ** GetSysVSDList(IAppliance *appliance, ULONG *Count)
{
    // Temp safe array to get list of these objects
    SAFEARRAY *SA = SAOutParamAlloc();

    IAppliance_GetVirtualSystemDescriptions(appliance,
        ComSafeArrayAsOutIfaceParam(SA, IVirtualSystemDescription *));
    ExitIfNull(SA, "GetVirtualSystemDescriptions", __FILE__, __LINE__);   

    // Caller MUST release this array and its individual element objects when done.
    IVirtualSystemDescription **List = NULL;

    // Transfer from safe array to regular array, also update Count 
    SACopyOutIfaceParamHelper((IUnknown ***)&List, Count, SA);
    SADestroy(SA);
    return List;
}


// Get the five(5) descryption arrays from given system description
void GetVSDArrays(IVirtualSystemDescription *sysDesc, ULONG *Count, ULONG **List1,
    BSTR **List2, BSTR **List3, BSTR **List4, BSTR **List5)
{
    // Use temporary safe arrays for each of the 5 description entries
    SAFEARRAY *SA1 = SAOutParamAlloc();  // Types
    SAFEARRAY *SA2 = SAOutParamAlloc();  // Refs
    SAFEARRAY *SA3 = SAOutParamAlloc();  // OVFValues
    SAFEARRAY *SA4 = SAOutParamAlloc();  // VBoxValues
    SAFEARRAY *SA5 = SAOutParamAlloc();  // ExtraConfigValues

    // Get the number of virtual system description entries. See page 383 in SDK
    IVirtualSystemDescription_GetCount(sysDesc, Count);

    // Get the the 5 description entries, store them in the each SA. See page 383/4 in SDK
    IVirtualSystemDescription_GetDescription(sysDesc,
        ComSafeArrayAsOutIfaceParam(SA1, ULONG),
        ComSafeArrayAsOutIfaceParam(SA2, BSTR),
        ComSafeArrayAsOutIfaceParam(SA3, BSTR),
        ComSafeArrayAsOutIfaceParam(SA4, BSTR),
        ComSafeArrayAsOutIfaceParam(SA5, BSTR)); 
    ExitIfNull(SA1, "Types SA", __FILE__, __LINE__);   
    ExitIfNull(SA2, "Refs SA", __FILE__, __LINE__);   
    ExitIfNull(SA3, "OVFValues SA", __FILE__, __LINE__);   
    ExitIfNull(SA4, "VBoxValues SA", __FILE__, __LINE__);   
    ExitIfNull(SA5, "ExtraConfigValues SA", __FILE__, __LINE__);   

    // REMINDER: Caller must free allocated memory

    // Transfer from safe arrays to regular arrays
    SACopyOutIfaceParamHelper((IUnknown ***)List1, Count, SA1);
    SACopyOutIfaceParamHelper((IUnknown ***)List2, Count, SA2);
    SACopyOutIfaceParamHelper((IUnknown ***)List3, Count, SA3);
    SACopyOutIfaceParamHelper((IUnknown ***)List4, Count, SA4);
    SACopyOutIfaceParamHelper((IUnknown ***)List5, Count, SA5);
    SADestroy(SA1);
    SADestroy(SA2);
    SADestroy(SA3);
    SADestroy(SA4);
    SADestroy(SA5);
}


// Update the three (3) required arrays in given Virtual System Description
void UpdateVSDArrays(IVirtualSystemDescription *sysDesc, ULONG Count,
    BOOL *List1, BSTR *List2, BSTR *List3)
{
    // This function is an example of how to pass arrays to the API
    // You have to use SAFEARRAYs as intermediaries. First create the SA,
    // then use ParamHelper functions to transfer the regular C array into
    // it. Note that Count is needed because we're in a function, where
    // doing sizeof(array) wouldn't work.

    ULONG Size1 = (sizeof(List1[0]) * Count);
    ULONG Size2 = (sizeof(List2[0]) * Count);
    ULONG Size3 = (sizeof(List3[0]) * Count);

    // !!!!***********************************!!!!
    // Wasted a number of days trying to use VT_BOOL instead of VT_I4!
    // !!!!***********************************!!!!

    // Create safe arrays (for passing arrays to COM/XPCOM)
    SAFEARRAY *SA1 = SACreateVector(VT_I4, 0, Count);     // List1 = Enabled
    SAFEARRAY *SA2 = SACreateVector(VT_BSTR, 0, Count);   // List2 = VBoxValues 
    SAFEARRAY *SA3 = SACreateVector(VT_BSTR, 0, Count);   // List3 = ExtraConfigValues
    ExitIfNull(SA1, "Enabled SA", __FILE__, __LINE__);   
    ExitIfNull(SA2, "VBoxValues SA", __FILE__, __LINE__);   
    ExitIfNull(SA3, "ExtraConfigValues SA", __FILE__, __LINE__);

    // Copy C arrays into safe arrays, for passing as input parameters below
    SACopyInParamHelper(SA1, List1, Size1);
    SACopyInParamHelper(SA2, List2, Size2);
    SACopyInParamHelper(SA3, List3, Size3);

    // Call setFinalValues to register update VSD values for this VM
    IVirtualSystemDescription_SetFinalValues(sysDesc,
        ComSafeArrayAsInParam(SA1),
        ComSafeArrayAsInParam(SA2),
        ComSafeArrayAsInParam(SA3));
    SADestroy(SA1);
    SADestroy(SA2);
    SADestroy(SA3);
}
