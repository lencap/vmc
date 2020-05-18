// imgpack.c

#include "vmc.h"

void imgPack(void)
{
    printf("To create a brand new OVA image from an ISO, first make sure you install\n"
        "'git' and 'packer', then do the following:\n\n"
        "1. git clone https://github.com/lencap/osimages\n"
        "2. Choose which Linux OS template to use, say Ubuntu, then\n"
        "3. packer validate ubuntu1804.json\n"
        "4. packer build ubuntu1804.json\n"
        "5. %s imgimp output-virtualbox-iso/ubuntu1804.ova\n"
        "6. %s imglist To confirm image is then available\n\n"
        "You should be able to use any of the OVA packer templates in above repo, and\n"
        "of course you can also create your very own by reviewing existing templates.\n"
        "In particular, you need to ensure the image uses the /usr/local/bin/vmnet\n"
        "script correctly.\n", prgname, prgname);
    Exit(EXIT_SUCCESS);    
}
