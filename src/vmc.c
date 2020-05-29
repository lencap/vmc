// vmc.c
// Simple Linux VM manager

#include "vmc.h"

// @todo
// Setup keyboard interrupt handler
// Allow easy switching/update of vmuser username, password, and ssh keys
// Allow direct file transfer between VMs
// Allow multiple vmcopy and vmrun
// Don't mess with VMs that were not created by this program
// Don't allow creation of HO net of any existing host net
// Use the proper Free8/Free16/FreeBST/free function
// Combine vmmod and vmip
// bug: vmcreate using img from any folder

// Global constants and variables
const char prgver[]  = "v122";
const char prgname[] = "vmc";
const char vmconf[]  = "vm.conf";         // Default provisioning filename
const char vmdefip[] = "10.11.12.2";      // Default IP address
const char vmuser[]  = "vmuser";          // Default username
const char vmdir[]   = ".vmc";            // Config dir name
const char sshpri[]  = "vmkey";           // Private SSH key filename
const char sshpub[]  = "vmkey.pub";       // Public SSH key filename
IVirtualBoxClient *vboxclient = NULL;     // Global IVirtualBoxClient pointer
IVirtualBox *vbox             = NULL;     // Global IVirtualBox pointer
ISystemProperties *sysprop    = NULL;     // Global ISystemProperties pointer
IHost *ihost       = NULL;                // Global IHost pointer
IMachine **VMList  = NULL;                // Global VM array pointer
ULONG VMListCount  = 0;                   // Global VM list counter
char *vbhome;                             // Default VirtualBox MachineFolder directory
char vmsshpri[256] = "";                  // Private SSH key full path
char vmhome[256]   = "";                  // This program's config directory

// These constants of the enum types are a bit repetitive, but they make
// things so much more clearer
// MachineState
const char *VMStateStr[] = { "Null", "PoweredOff", "Saved", "Teleported", "Aborted",
    "Running", "Paused", "Stuck", "Teleporting", "LiveSnapshotting", "Starting",
    "Stopping", "Saving", "Restoring", "TeleportingPausedVM", "TeleportingIn",
    "DeletingSnapshotOnline", "DeletingSnapshotPaused", "OnlineSnapshotting",
    "RestoringSnapshot", "DeletingSnapshot", "SettingUp", "Snapshotting",
    "FirstOnline", "LastOnline", "FirstTransient", "LastTransient",
};
// HostNetworkInterfaceType
const char *NICifType[] = { "Null", "Bridged", "HostOnly" };
// HostNetworkInterfaceStatus
const char *NICStatus[] = { "Unknown", "Up", "Down" };
// SessionState
const char *SessState[] = { "Null", "Unlocked", "Locked", "Spawning", "Unlocking" };
// NetworkAdapterType
const char *NICType[] = { "Null", "Am79C970A", "Am79C973", "I82540EM", "I82543GC",
    "I82545EM", "Virtio", "Am79C960", "Virtio_1_0" };
// NetworkAttachmentType
const char *NICattType[] = { "Null", "NAT", "Bridged", "Internal", "HostOnly",
    "Generic", "NATNetwork", "Cloud" };
// NetworkAdapterPromiscModePolicy
const char *PromiscMode[] = { "Undefined", "Deny", "AllowNetwork", "AllowAll" };
// Boolean
const char *BoolStr[] = { "False", "True" };
// StorageBus
const char *BusStr[] = { "Null", "IDE", "SATA", "SCSI", "Floppy", "SAS", "USB",
    "PCIe", "VirtioSCSI" };
// StorageControllerType
const char *CtrlType[] = { "Null", "LsiLogic", "BusLogic", "IntelAhci", "PIIX3",
    "PIIX4", "ICH6", "I82078", "LsiLogicSas", "USB", "VNMe", "VirtioSCSI" };
// DeviceType
const char *DevType[] = { "Null", "Foppy", "DVD", "HardDisk", "Network",
    "USB", "SharedFolder", "Graphics3D" };
// VirtualSystemDescriptionType
const char *VSDType[] = { "Null", "Ignore", "OS", "Name", "Product", "Vendor",
    "Version", "ProductUrl", "VendorUrl", "Description", "License", "Miscellaneous",
    "CPU", "Memory", "HardDiskControllerIDE", "HardDiskControllerSATA",
    "HardDiskControllerSCSI", "HardDiskControllerSAS", "HardDiskImage", "Floppy",
    "CDROM", "NetworkAdapter", "USBController", "SoundCard", "SettingsFile",
    "BaseFolder", "PrimaryGroup", "CloudInstanceShape", "CloudDomain",
    "CloudBootDiskSize", "CloudBucket", "CloudOCIVCN", "CloudPublicIP",
    "CloudProfileName", "CloudOCISubnet", "CloudKeepObject", "CloudLaunchInstance",
    "CloudInstanceId", "CloudImageId", "CloudInstanceState", "CloudImageState",
    "CloudInstanceDisplayName", "CloudImageDisplayName", "CloudOCILaunchMode",
    "CloudPrivateIP", "CloudBootVolumeId", "CloudOCIVCNCompartment",
    "CloudOCISubnetCompartment", "CloudPublicSSHKey", "BootingFirmware" };


// MAIN
int main(int argc, char *argv[])
{
    // If less than 2 arguments just print usage (and exit)
    if (argc < 2) { PrintUsage(); }

    // Initialize global objects: vboxclient, box, sysprop, vbhome, and vmhome
    InitGlobalObjects();

    // Update in-memory global VMList
    UpdateVMList();
    // It is referenced often, so this optimizes program run
    // If necessary, other functions can re-run to update list again
    
    // Create SSH keys on every run to ensure they're always readily available
    CreateSSHKeys();

    // Use 2nd argument as the action command (1st arg is always the executable)
    const char *command = argv[1];

    // Shift arguments by 2, for better readability
    argc -= 2 ; argv += 2;

    if (Equal(command, "list"))           { PrintVMList(); }          // vmlist.c
    else if (Equal(command, "create"))    { vmCreate(argc, argv); }   // vmcreate.c
    else if (Equal(command, "del"))       { vmDelete(argc, argv); }   // vmdel.c
    else if (Equal(command, "start"))     { vmStart(argc, argv); }    // vmstart.c
    else if (Equal(command, "stop"))      { vmStop(argc, argv); }     // vmstop.c
    else if (Equal(command, "ssh"))       { vmSSH(argc, argv); }      // vmssh.c
    else if (Equal(command, "prov"))      { vmProv(argc, argv); }     // vmprov.c
    else if (Equal(command, "info"))      { vmInfo(argc, argv); }     // vminfo.c
    else if (Equal(command, "mod"))       { vmMod(argc, argv); }      // vmmod.c
    else if (Equal(command, "ip"))        { vmIP(argc, argv); }       // vmip.c
    else if (Equal(command, "imglist"))   { imgList(); }              // imglist.c
    else if (Equal(command, "imgcreate")) { imgCreate(argc, argv); }  // imgcreate.c
    else if (Equal(command, "imgpack"))   { imgPack(); }              // imgpack.c
    else if (Equal(command, "imgdel"))    { imgDelete(argc, argv); }  // imgdel.c
    else if (Equal(command, "imgimp"))    { imgImport(argc, argv); }  // imgimp.c
    else if (Equal(command, "nettype"))   { netType(argc, argv); }    // nettype.c
    else if (Equal(command, "netlist"))   { netList(); }              // netlist.c
    else if (Equal(command, "netadd"))    { netAdd(argc, argv); }     // netadd.c
    else if (Equal(command, "netdel"))    { netDel(argc, argv); }     // netdelete.c
    else { PrintUsage(); }                                            // usage.c

    Exit(EXIT_SUCCESS);
}
