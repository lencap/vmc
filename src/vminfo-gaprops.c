// vminfo-gaprops.c

#include "vmc.h"

// Print guest addition properties
int printGuestAdditionProps(IMachine *vm)
{
    // Do the ISO path manually (from ISystemProperties, not IMachine)
    BSTR gaISO_16 = NULL;
    ISystemProperties_GetDefaultAdditionsISO(sysprop, &gaISO_16);
    char *gaISO = NULL;
    Convert16to8(gaISO_16, &gaISO);
    FreeBSTR(gaISO_16);
    printf("%-40s  %s\n", "/VirtualBox/GuestAdd/ISOFile", gaISO);

    // Below mess is only to print all IMachine GA properties.

    // Temp safe arrays to get list of these objects
    SAFEARRAY *nameSA  = SAOutParamAlloc();
    SAFEARRAY *valueSA = SAOutParamAlloc();
    SAFEARRAY *timeSA  = SAOutParamAlloc();
    SAFEARRAY *flagSA  = SAOutParamAlloc();

    IMachine_EnumerateGuestProperties(vm,
        NULL,    // A NULL filter returns all properties
        ComSafeArrayAsOutTypeParam(nameSA, BSTR),
        ComSafeArrayAsOutTypeParam(valueSA, BSTR),
        ComSafeArrayAsOutTypeParam(timeSA, PRInt64),
        ComSafeArrayAsOutTypeParam(flagSA, BSTR));
    ExitIfNull(nameSA, "Name SA", __FILE__, __LINE__);     
    ExitIfNull(valueSA, "Value SA", __FILE__, __LINE__);     
    ExitIfNull(timeSA, "Time SA", __FILE__, __LINE__);     
    ExitIfNull(flagSA, "Flag SA", __FILE__, __LINE__);     

    // We only care about name/value pair, so destroy timestamp and flag SAs now
    SADestroy(timeSA);
    SADestroy(flagSA);

    // Using temp intermediate UTF16 size counter to capture number of Guest Props
    ULONG gpCount_16 = 0;
    // We're also using the same counter for both BSTR lists
    BSTR *nameList = NULL, *valueList = NULL;

    // Transfer safe arrays to regular C arrays, using BSTR variant type
    SACopyOutParamHelper((void **)&nameList, &gpCount_16, VT_BSTR, nameSA);
    SACopyOutParamHelper((void **)&valueList, &gpCount_16, VT_BSTR, valueSA);
    SADestroy(nameSA);
    SADestroy(valueSA);

    // Translate UTF16 size counters to regular byte size counters
    ULONG gpCount = gpCount_16 / sizeof(nameList[0]);

    // Print the Value and Name of each property. Remember: We're disregarding TimeStamp and Flag.
    for (int i = 0; i < gpCount; ++i) {
        char *name, *value;
        Convert16to8(nameList[i], &name);
        Convert16to8(valueList[i], &value);
        printf("%-40s  %s\n", name, value);
        free(name); free(value);
    }

    // Release all objects
    for (int i = 0; i < gpCount; ++i) {
        if (nameList[i]) { FreeBSTR(nameList[i]); }
        if (valueList[i]) { FreeBSTR(valueList[i]); }
    }
    ArrayOutFree(nameList);   // Only for SACopyOutParamHelper C arrays
    ArrayOutFree(valueList);
    return 0;
}
