// vmc.h

// // These may be a requirement to compile in non-macOS systems
// #ifndef _POSIX_C_SOURCE
// #define _POSIX_C_SOURCE
// #endif
// #ifndef _BSD_SOURCE
// #define _BSD_SOURCE
// #endif
// #ifndef _GNU_SOURCE
// #define _GNU_SOURCE
// #endif

#include "VBoxCAPIGlue.h"

#ifndef WIN32
# include <signal.h>
# include <unistd.h>
# include <sys/poll.h>
#endif
#ifdef IPRT_INCLUDED_cdefs_h
# error "not supposed to involve any IPRT or VBox headers here."
#endif

#include "ini.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>


// DEFINES
#if defined(WIN32) || defined(_WIN32) 
#define PATHSEPSTR  "\\"
#define PATHSEPCHAR '\\'
#else 
#define PATHSEPSTR  "/"
#define PATHSEPCHAR '/'
#endif
// Syntactic sugar for common API functions
#define Convert16to8(u16,u8)     (g_pVBoxFuncs->pfnUtf16ToUtf8(u16,u8))
#define Free8(a)                 (g_pVBoxFuncs->pfnUtf8Free(a))
#define Convert8to16(u8,u16)     (g_pVBoxFuncs->pfnUtf8ToUtf16(u8,u16))
#define Free16(a)                (g_pVBoxFuncs->pfnUtf16Free(a))
#define FreeBSTR(a)              (g_pVBoxFuncs->pfnComUnallocString(a))
#define SAOutParamAlloc()                  (g_pVBoxFuncs->pfnSafeArrayOutParamAlloc())
#define SACopyOutIfaceParamHelper(a,b,c)   (g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper(a,b,c))
#define SACopyOutParamHelper(a,b,c,d)      (g_pVBoxFuncs->pfnSafeArrayCopyOutParamHelper(a,b,c,d))
#define ArrayOutFree(a)                    (g_pVBoxFuncs->pfnArrayOutFree(a))
#define SACopyInParamHelper(a,b,c)         (g_pVBoxFuncs->pfnSafeArrayCopyInParamHelper(a,b,c))
#define SACreateVector(a,b,c)              (g_pVBoxFuncs->pfnSafeArrayCreateVector(a,b,c))
#define SADestroy(sa)                      (g_pVBoxFuncs->pfnSafeArrayDestroy(sa))


// TYPES


// GLOBAL CONSTANTS AND VARIABLES DECLARATION
extern const char prgver[];
extern const char prgname[];
extern const char vmconf[];
extern const char vmdefip[];
extern const char vmuser[];
extern const char vmdir[];
extern const char sshpri[];
extern const char sshpub[];
extern IVirtualBoxClient *vboxclient;
extern IVirtualBox *vbox;
extern ISystemProperties *sysprop;
extern IHost *ihost;
extern IMachine **VMList;
extern ULONG VMListCount;
extern char *vbhome;
extern char vmsshpri[];
extern char vmhome[];
extern const char *VMStateStr[];
extern const char *NICifType[];
extern const char *NICStatus[];
extern const char *SessState[];
extern const char *NICType[];
extern const char *NICattType[];
extern const char *PromiscMode[];
extern const char *BoolStr[]; 
extern const char *BusStr[];
extern const char *CtrlType[];
extern const char *DevType[];
extern const char *VSDType[];


// FUNCTIONS PROTOTYPES
// vbapi.c
void InitGlobalObjects(void);
void PrintVBoxException(void);
char * GetProgressError(IProgress *progress);
char * GetProgressId(IProgress *progress);
void HandleProgress(IProgress *progress, HRESULT rc, PRInt32 Timeout);
ISession * GetSession(IMachine *vm, PRUint32 lockType, IMachine **vmMuta);
void CloseSession(ISession *session);
void Exit(int rc);

// helper.c
int isDir(const char *path);
int isFile(const char *path);
BOOL Equal(const char *str1, const char *str2);
void Lower(char *str);
bool endsWith(const char *str1, const char *str2);
void argCopy(char *name, int length, char *arg);
char ** strSplit(char *String, char DELIMITER, int *Count);
long int fileSize(char *path);
bool isTarFile(const char *path);
char * baseName(char *path);
int copyFile(const char *path1, const char *path2);
void ExitIfNull(void *ptr, const char *path, int line);
void ExitIfFailure(HRESULT rc, const char *msg, const char *path, int line);
char * TimeNow(void);
void DEBUG(char *msg);
char * NewString(int size);

// vmlist.c
void PrintVMList(void);
void UpdateVMList(void);
void FreeVMList(void);
IMachine * GetVM(char *name);
PRUint32 VMState(IMachine *vm);
char * GetVMName(IMachine *vm);
char * GetVMProp(IMachine *vm, char *path);
void SetVMProp(IMachine *vm, char *path, const char *value);

// iplib.c
char * NextUniqueIP(const char *ip);
int ValidIpStr(const char *ipString);
char * GetIPNet(const char *ipStr);
BOOL UsedIP(char *ip, char *vmName);
BOOL ActiveIP(char *ip);

// netlist.c
void netList(void);
void GetNICList(IHostNetworkInterface ***List, ULONG *Count);
char * getHostMainNIC(void);

// netadd.c
void netAdd(int argc, char *argv[]);
char * CreateHONIC(char *ip);

// vmssh.c
int vmSSH(int argc, char *argv[]);
int SSHVM(IMachine *vm, char *cmd, bool verbose);
bool sshAccess(char *ip);
int SCPVM(char *srcPath, char *vmName, char *dstPath, bool verbose);

// vmstop.c
void vmStop(int argc, char *argv[]);
bool StopVM(IMachine *vm);

// vminfo.c
int vmInfo(int argc, char *argv[]);
int printStorageControllers(IMachine *vm);
IStorageController ** getCtlrList(IMachine *vm, ULONG *Count);

// vminfo-media.c
int printMediaAttachments(IMachine *vm);
IMediumAttachment ** getMAList(IMachine *vm, ULONG *Count);

// vminfo-folders.c
int printSharedFolders(IMachine *vm);
ISharedFolder ** getSFList(IMachine *vm, ULONG *Count);

// nettype.c
void netType(int argc, char *argv[]);
bool SetVMNetType(IMachine *vm, char *nettype);

// vmcreate.c
void vmCreate(int argc, char *argv[]);
IMachine * CreateVM(char *vmName, char *imgFile);
BSTR * getWarningsList(IAppliance *appliance, ULONG *Count);
IVirtualSystemDescription ** GetSysVSDList(IAppliance *appliance, ULONG *Count);
void GetVSDArrays(IVirtualSystemDescription *sysDesc, ULONG *Count, ULONG **List1,
    BSTR **List2, BSTR **List3, BSTR **List4, BSTR **List5);
void UpdateVSDArrays(IVirtualSystemDescription *sysDesc, ULONG Count,
    BOOL *List1, BSTR *List2, BSTR *List3);

// vmprov.c
void vmProv(int argc, char *argv[]);
void CreateVMConf(void);
void ProvisionConfig(char *provFile);

// vmstart.c
void vmStart(int argc, char *argv[]);
bool StartVM(IMachine *vm, char *option);

// vmmod.c
void vmMod(int argc, char *argv[]);
bool ModVM(IMachine *vm, char *cpuCount, char *memSize);

// vmip.c
void vmIP(int argc, char *argv[]);
bool SetVMIP(IMachine *vm, char *ip);

// Own .c file
void PrintUsage(void);
void CreateSSHKeys(void);
void imgCreate(int argc, char *argv[]);
void vmDelete(int argc, char *argv[]);
void imgList(void);
void imgPack(void);
void imgDelete(int argc, char *argv[]);
void imgImport(int argc, char *argv[]);
void netDel(int argc, char *argv[]);
int printNetworkAdapters(IMachine *vm);
int printGuestAdditionProps(IMachine *vm);
