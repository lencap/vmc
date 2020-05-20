// vbapi.c

#include "vmc.h"

// Initialize all global API objects
void InitGlobalObjects(void) {
    // Initialize VirtualBox C API Glue
    if (VBoxCGlueInit()) {
        // VBoxCGlueInit loads the necessary dynamic library, handles errors, and makes
        // available g_pVBoxFuncs, which is a pointer to the VBOXCAPI function table.
        // When all's done, this should be matched by a call to corresponding VBoxCGlueTerm.
        // We do that in the Exit() function.
        fprintf(stderr, "VBoxCGlueInit failed:\n%s\n", g_szVBoxErrMsg);
        Exit(EXIT_FAILURE);
    }

    // This utility only works with API version 5 or above
    // g_pVBoxFuncs should have been made globally available by VBoxCGlueInit
    unsigned int ver = g_pVBoxFuncs->pfnGetAPIVersion() / 1000;
    if (ver < 5) {
        fprintf(stderr, "Requires VirtualBox API v5.0 or greater\n");
        Exit(EXIT_FAILURE);
    }

    // Get and set IVirtualBoxClient global instance
    HRESULT rc = g_pVBoxFuncs->pfnClientInitialize(NULL, &vboxclient);
    // pfnClientInitialize does all the necessary startup action and should
    // provide above requested IVirtualBoxClient instance. When all is done
    // is should be matched by a call to g_pVBoxFuncs->pfnClientUninitialize().
    ExitIfFailure(rc, "g_pVBoxFuncs->pfnClientInitialize", __FILE__, __LINE__);

    // Get and set IVirtualBox global instance
    rc = IVirtualBoxClient_GetVirtualBox(vboxclient, &vbox);
    ExitIfFailure(rc, "IVirtualBoxClient_GetVirtualBox", __FILE__, __LINE__);

    // Get and set ISystemProperties global instance
    rc = IVirtualBox_GetSystemProperties(vbox, &sysprop);
    ExitIfFailure(rc, "IVirtualBox_GetSystemProperties", __FILE__, __LINE__);

    // Get and set IHost global instance
    rc = IVirtualBox_GetHost(vbox, &ihost);
    ExitIfFailure(rc, "IVirtualBox_GetHost", __FILE__, __LINE__);

    // Get and set default MachineFolder
    BSTR vbhome_16 = NULL;
    // Unfortunately, the API defaults to using UTF16 char encoding
    rc = ISystemProperties_GetDefaultMachineFolder(sysprop, &vbhome_16);
    ExitIfFailure(rc, "ISystemProperties_GetDefaultMachineFolder", __FILE__, __LINE__);
    Convert16to8(vbhome_16, &vbhome);   // Transfer to UTF8 global variable
    FreeBSTR(vbhome_16);

    // Ensure config directory exists.
    sprintf(vmhome, "%s%s%s", getenv("HOME"), PATHSEPSTR, vmdir);
    // Note that vmhome and vmdir are GLOBAL variables
    if (!isDir(vmhome)) {
        if (mkdir(vmhome, 0755)) {   // Try creating it, and confirm success
            fprintf(stderr, "Error creating dir '%s'\n", vmhome);
            Exit(EXIT_FAILURE);
        }
    }
}


// Print last VirtualBox exception errors if they are available
void PrintVBoxException(void)
{
    // Return immediately if the API function handler isn't even defined
    if (!g_pVBoxFuncs) { return; }

    // Parts borrowed from SDK tstCAPIGlue.c
    IErrorInfo *ex;
    HRESULT rc = g_pVBoxFuncs->pfnGetException(&ex);
    if (FAILED(rc) || !ex) { return; }

    IVirtualBoxErrorInfo *ei;
    rc = IErrorInfo_QueryInterface(ex, &IID_IVirtualBoxErrorInfo, (void **)&ei);
    if (FAILED(rc) || !ei) { return; }

    do {
        // Get and print extended error info, maybe multiple entries
        BSTR text_16 = NULL;
        IVirtualBoxErrorInfo_GetText(ei, &text_16);
        char *text;
        Convert16to8(text_16, &text);
        FreeBSTR(text_16);
        fprintf(stderr, "%s\n", text);
        free(text);

        IVirtualBoxErrorInfo *ei_next = NULL;
        rc = IVirtualBoxErrorInfo_GetNext(ei, &ei_next);
        if (FAILED(rc)) { ei_next = NULL; }
        IVirtualBoxErrorInfo_Release(ei);
        ei = ei_next;
    } while (ei);

    if (ex) { IErrorInfo_Release(ex); }
    g_pVBoxFuncs->pfnClearException();
}


// Get failed IProgress error message
char * GetProgressError(IProgress *progress)
{
    IVirtualBoxErrorInfo *ei;
    IProgress_GetErrorInfo(progress, &ei);
    BSTR text_16;
    IVirtualBoxErrorInfo_GetText(ei, &text_16);

    char *text = NULL;
    // REMINDER: Caller must free allocated memory
    Convert16to8(text_16, &text);

    FreeBSTR(text_16);
    IVirtualBoxErrorInfo_Release(ei);
    return text;
}


// Get IProgress object id
char * GetProgressId(IProgress *progress)
{
    BSTR id_16 = NULL;
    IProgress_GetId(progress, &id_16);

    char *id = NULL;
    // REMINDER: Caller must free allocated memory
    Convert16to8(id_16, &id);

    FreeBSTR(id_16);
    return id;
}


// Handle IProgress object
void HandleProgress(IProgress *progress, HRESULT rc, PRInt32 Timeout)
{
    // Using VBOX API call return code
    if (FAILED(rc)) {
        PrintVBoxException();   // Print exceptions if any
        return;
    }

    IProgress_WaitForCompletion(progress, Timeout);

    // Wait for about a minute, in .1 sec intervals, until it completes
    BOOL completed = FALSE;
    int delay = 600;
    while (!completed && delay > 0) {
        usleep(100000);  // Do nothing for .1 second
        --delay;
        rc = IProgress_GetCompleted(progress, &completed);
        ExitIfFailure(rc, "IProgress_GetCompleted", __FILE__, __LINE__);
    }

    // By this time it should have completed
    LONG resultCode;
    IProgress_GetResultCode(progress, &resultCode);
    if (FAILED(resultCode)) {
        char *text = GetProgressError(progress);
        fprintf(stderr, "%s:%d IProgress_GetResultCode error\n", __FILE__, __LINE__);
        fprintf(stderr, "%s\n", text);
        free(text);
    }

    // // DEBUG
    // char *now = TimeNow();
    // char *id = GetProgressId(progress);
    // printf("%s Before releasing IProgress id=%s\n", now, id);
    // if (now) { free(now); }

    if (progress) { IProgress_Release(progress); }
    // NOTE: Not sure I like releasing the IProgress object pointer here,
    // instead of in the calling function where it was created.

    // // DEBUG
    // now = timenow();
    // printf("%s After releasing IProgress id=%s\n", now, id);
    // if (now) { free(now); }
    // if (id) { free(id); }        
}


// Set up and return a new session
ISession * GetSession(IMachine *vm, PRUint32 lockType, IMachine **vmMuta)
{
    // There are TWO different session types:
    // Type1 GENERIC
    //   Returns just a session object for doing IMachine_LaunchVMProcess
    //   and other types of call that do NOT involved a VM. The call format
    //   is: ISession *session = GetSession(NULL, LockType_Null, NULL);
    // Type2 VM
    //   Returns a session object, PLUS a VM object that can be written to
    //   (mutable) if lockType is Write, or just ReadOnly if lockType is
    //   Shared. Share lock are for remote-controlling the machine, to change
    //   certain VM settings which can be safely modified for a running VM.
    //   The call format is:
    //     vm = IMachine object
    //     ISession *session = GetSession(vm, LockType_Shared|LockType_Write, &vmMuta);
    //   NOTE: Given vm is just a copy pointer, but vmMuta pointer updates the
    //   the actual, usable handle 

    // Allocate memory for a new Session pointer that will survive this function
    ISession *session = malloc(sizeof(ISession*));
    ExitIfNull(session, __FILE__, __LINE__);
    // REMINDER: Caller must free allocated memory
    
    // Get a new ISession object and store it in above pointer
    HRESULT rc = IVirtualBoxClient_GetSession(vboxclient, &session);
    ExitIfFailure(rc, "IVirtualBoxClient_GetSession", __FILE__, __LINE__);

    if (lockType == LockType_Null) { return session; }  // Type1 request, we're done

    // Write lock the given VM
    rc = IMachine_LockMachine(vm, session, lockType);
    ExitIfFailure(rc, "IMachine_LockMachine", __FILE__, __LINE__);

    // Get handle to the mutable copy
    rc = ISession_GetMachine(session, vmMuta);
    ExitIfFailure(rc, "ISession_GetMachine", __FILE__, __LINE__);

    return session;
}


// Save all settings and unlock session
void CloseSession(ISession *session)
{
    // If there's a VM associated with this session, save all settings
    IMachine *vmMuta = NULL;
    ISession_GetMachine(session, &vmMuta);
    if (vmMuta) {
        IMachine_SaveSettings(vmMuta);
        // DISSABLED: Causes "Segmentation fault: 11"
        //IMachine_Release(vmMuta);
        //vmMuta = NULL;
    }

    ISession_UnlockMachine(session);
    if (session) {
        ISession_Release(session);
        session = NULL;
    }
}


// Graceful exit, releasing any global VBOX API object
void Exit(int rc)
{
    // First, release any object dependant on the API
    if (vbhome) { Free8(vbhome); }
    FreeVMList();

    // Finally, release all major API objects
    if (ihost) { IHost_Release(ihost); }
    if (sysprop) { ISystemProperties_Release(sysprop); }
    if (vbox) { IVirtualBox_Release(vbox); }
    if (vboxclient) { IVirtualBoxClient_Release(vboxclient); }
    if (g_pVBoxFuncs) {
        // Uninitialize client and terminate VirtualBox C API Glue
        g_pVBoxFuncs->pfnClientUninitialize();
        VBoxCGlueTerm();
    }
    exit(rc);
}
