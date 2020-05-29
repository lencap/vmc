// usage.c

#include "vmc.h"

void PrintUsage(void)
{
    const char *p = prgname;
    printf("Simple Linux VM Manager %s\n"
        "%s list                                   List all VMs\n"
        "%s create    <vmName> <ovaFile|imgName>   Create VM from given ovaFile, or imgName\n"
        "%s del       <vmName> [f]                 Delete VM. Force option\n"
        "%s start     <vmName> [g]                 Start VM. GUI option\n"
        "%s stop      <vmName> [f]                 Stop VM. Force option\n"
        "%s ssh       <vmName> [<cmd arg>]         SSH into or optionally run command on VM\n"
        "%s prov      [<vmConf>|c]                 Provision VMs in given vmConf file; Create skeleton file option\n"
        "%s info      <vmName>                     Dump extended VM details\n"
        "%s mod       <vmName> <cpus> [<mem>]      Modify VM CPUs and memory. Memory defaults to 1024\n"
        "%s ip        <vmName> <ip>                Set VM IP address\n"
        "%s imglist                                List all available images\n"
        "%s imgcreate <imgName> <vmName>           Create imgName from existing VM\n"
        "%s imgpack                                How-to create brand new OVA image with Hashicorp packer\n"
        "%s imgimp    <imgFile>                    Import image. Make available to this program\n"
        "%s imgdel    <imgName> [f]                Delete image. Force option\n"
        "%s nettype   <vmName> <ho[bri]>           Set NIC type to Bridge as option; default is always HostOnly\n"
        "%s netlist                                List available HostOnly networks\n"
        "%s netadd    <ip>                         Create new HostOnly network\n"
        "%s netdel    <vboxnetX>                   Delete given HostOnly network\n"
        , prgver, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p);
        
    Exit(EXIT_SUCCESS);
}
