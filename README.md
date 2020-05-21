# VMC
A small command line [VirtualBox](https://www.virtualbox.org/) front-end utility to manage Linux VMs on __macOS__.

It's similar to VirtualBox's own [`VBoxManage`](https://www.virtualbox.org/manual/ch08.html), but limited to only those functions I find myself needing 99% of the time when I'm working with Linux VMs. In addition, it allows the automated provisioning of one or multiple VMs, like a poor-man's Vagrant, albeit a much more destitute one 😀.

Things to keep in mind:

- Again, __only__ Linux VMs, and only on __macOS__
- Still a work in progress, so expect to resolve some issues by manually hopping back on the VirtualBox GUI
- This is the __C__ language version ([a Python version sits here](https://github.com/lencap/vm))
- The prime driver with functionality and code is the KISS principle
- Constructive comments and suggestions are always welcome

## VirtualBox SDK
This repo contains a subset of the [VirtualBox SDK code](https://www.virtualbox.org/wiki/Downloads) under `./bindings/`. It is needed to compile against the VBOX C API, and the code is licensed as per the vendor.

To copy the pertinent SDK files, at any time, run the `./getbindings` script from the root of a checked-out copy of this repo. 

## Prerequisites
* Virtual machines created and managed by this utility __must__ be based on OVA files created by using repo https://github.com/lencap/osimages. Run `vmc imgpack` for more information on how to do that. 
* Tested on macOS v10.15.4 with VirtualBox v6.1.6

## Provisioning VMs
The `vmc prov` command provisions VMs automatically based on a simple configuration file.

You can create a sample skeleton config file by running `vmc prov c`. By default, this file will be named `vmc.conf`, which the `vmc prov` command will read and follow to provision things accordingly. But you can name the file whatever you wish, so you can then have multiple of these provisioning config files in your repo, which you can then provision as `vmc prov myprov1.conf`, and so on.

## Networking Modes
Two networking modes are supported: The default __HostOnly__ mode, or the optional and experimental __Bridged__ mode.

HostOnly networking, as used in most local VM configurations, sets up NIC1 as NAT for external traffic, and NIC2 as HostOnly for intra-VM traffic. This is usually the most popular mode, as it allows one to set up a mini network of VMs for whatever work one is doing.  

Bridged networking allows one use the local LAN, with a static IP address for each VM, all running from your own host machine. This option allows others on the same LAN to access services running on your VMs. __IMPORTANT__: For this to work A) you need local host __administrator privileges__, and B) you need to be allowed to assign STATIC IP ADDRESSES on your local network. This mode is not as popular, but can be useful in some unique settings.

## Installation Options
There are two install options:
- `brew install lencap/tools/vmc` to use latest Homebrew release, or ...
- `make`, then `make install` to place binary under `/usr/local/bin/`, if you're good compiling this yourself

## General Usage
```
$ vmc
Simple Linux VM Manager v117
vmc list                                   List all VMs
vmc create    <vmName> <ovaFile|imgName>   Create VM from given ovaFile, or imgName
vmc del       <vmName> [f]                 Delete VM. Force option
vmc start     <vmName> [g]                 Start VM. GUI option
vmc stop      <vmName> [f]                 Stop VM. Force option
vmc ssh       <vmName> [<cmd arg>]         SSH into or optionally run command on VM
vmc prov      [<vmConf>|c]                 Provision VMs in given vmConf file; Create skeleton file option
vmc info      <vmName>                     Dump subset of all VM details for common troubleshooting
vmc mod       <vmName> <cpus> [<mem>]      Modify VM CPUs and memory. Memory defaults to 1024
vmc ip        <vmName> <ip>                Set VM IP address
vmc imglist                                List all available images
vmc imgcreate <imgName> <vmName>           Create imgName from existing VM
vmc imgpack                                How-to create brand new OVA image with Hashicorp packer
vmc imgimp    <imgFile>                    Import image. Make available to this program
vmc imgdel    <imgName> [f]                Delete image. Force option
vmc nettype   <vmName> <ho[bri]>           Set NIC type to Bridge as option; default is always HostOnly
vmc netlist                                List available HostOnly networks
vmc netadd    <ip>                         Create new HostOnly network
vmc netdel    <vboxnetX>                   Delete given HostOnly network
```
