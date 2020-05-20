// helper.c

#include "vmc.h"

// ===== For copyFile ======
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <copyfile.h>
#else
#include <sys/sendfile.h>
#endif

#include <sys/time.h>
#include <math.h>

// Returns 1 if given path is a directory and exists, 0 otherwise 
int isDir(const char *path)
{
    if (!path) { return FALSE; }
    struct stat stats;
    stat(path, &stats);
    if (S_ISDIR(stats.st_mode)) { return TRUE; }  // Check for file existence
    return FALSE;
}


// Returns 1 if file exists, 0 otherwise 
int isFile(const char *path)
{
    if (!path) { return FALSE; }
    if (access(path, F_OK) == -1) { return FALSE; }  // Check if file exists
    return TRUE;
}


// Returns 1 IIF the two strings are exactly the same 
BOOL Equal(const char *str1, const char *str2)
{
    if (!str1 || !str2) { return FALSE; }
    // Syntactic sugar reduces useless cognitive load
    if (strcmp(str1, str2) == 0) { return TRUE; }
    else { return FALSE; }
}


// Converts string to lowercase
void Lower(char *str)
{
    if (!str) { return; }
    for (int i = 0; str[i]; i++) { str[i] = tolower(str[i]); }
}


// Return true if both strings have same ending
bool endsWith(const char *str1, const char *str2)
{
    if (!str1 || !str2) { return false; }

    int l1 = strlen(str1);
    int l2 = strlen(str2);

    if (l2 > l1) { return false; }

    while (l2 >= 0) {
       if (str1[l1] != str2[l2]) { return false; }
       --l1; --l2;
    }
    return true;
}


// Check arguments boundary and copy them 
void argCopy(char *name, int length, char *arg)
{
    if (strlen(arg) > length) {
        fprintf(stderr, "Argument '%s' exceeds %u characters.\n", arg, length);
        Exit(1);
    }
    strcpy(name, arg);
}


// Split string per given delimiter and return array of strings
char ** strSplit(char *String, char DELIMITER, int *Count)  // Option1
//int strsplit(char *String, char DELIMITER, int *Count, char ***StrList)  // Option2
{
    // TWO IMPLEMENTATION OPTION
    // 1. This method, that returns the list directly, when called with:
    //    char **List = strSplit(String, LiteralDelimiter, &Count);
    //    or
    // 2. Alternate method, that returns the list in StrList when called w/:
    //    char **List = NULL;
    //    strSplit(String, LiteralDelimiter, &Count, &List);
    // See corresponding 'return' comments below also.
    // 
    // This is documented because it's interesting how C's passing of pointer
    // variables by value forces us to use "***StrList" in the interface

    *Count = 1;
    char *tmp = String;
    while (*tmp) { if (*tmp == DELIMITER) { ++*Count; } ++tmp; }

    // Allocate memory for the base array of strings, type (char **)
    char **List = malloc(*Count * sizeof(char **));
    ExitIfNull(List, __FILE__, __LINE__);

    // Build each individual string element in the base array list
    char Element[32] = "";   // Assume elements will never exceed 32 chars width
    int e = 0, i = 0;
    while (*String) {           // Walk each char in string
        if (i > 32) {
            fprintf(stderr, "%s:%d index boundary error", __FILE__, __LINE__);
            exit(1);
        }
        Element[i] = *String;       // Build current element
        if (*String == DELIMITER) {
            Element[i] = '\0';	 // Terminate this element

            // Allocate memory for this element substring, type (char *)
            List[e] = NewString(i+1);  // Note that i = strlen(Element)
            strcpy(List[e], Element);  // Add element to base array
            ++e;                 // Let's do next element
            i = -1;              // Restart index for next element
	    }
	    ++String;     // Next character in string
        ++i;       // Next character in current element
    }
  
    // Build remaining element
    Element[i] = '\0';
    List[e] = NewString(i+1);
    strcpy(List[e], Element);  // Add element to base array

    // Option2 return:
    //   *StrList = List;
    //   return 0;     // Means function ran fine
    // Note how dereferencing with one (1) asterisk does a return-by-address!

    // Option1 return (return list directly)
    // REMINDER: Caller must free allocated memory
    return List;
}


// Returns file size or -1 if file not found
long int fileSize(char *path)
{
    if (!path) { return -1; }

    FILE *fp = fopen(path, "r");
    if (!fp) { return -1; }

    fseek(fp, 0L, SEEK_END);
    long int res = ftell(fp);
    fclose(fp);
    return res;
}


// Returns true (1) if it's a TAR file
bool isTarFile(const char *path)
{
    if (!path) { return false; }

    if (strlen(path) > 1024) {
        fprintf(stderr, "%s:%d path exceeds 1024 chars", __FILE__, __LINE__);
        Exit(EXIT_FAILURE);
    }
    char cmd[1100];
    sprintf(cmd, "tar tf %s > /dev/null 2>&1", path);
    int rc = system(cmd);
    if (rc == 0) { return true; }
    else { return false; }
}


// Return pointer to basename of filePath
char * baseName(char *path)
{
    if (!path) { return NULL; }

    int length = strlen(path) - 1;
    char *basename = &path[length];
    while (*basename != PATHSEPCHAR && length >= 0) {
        --basename;
        --length;
    }
    return ++basename;
}


// Copy file using kernel-space for better performance
int copyFile(const char *path1, const char *path2)
{    
    // From https://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c

    if (!path1 || !path2) { return 1; }

    int source, target, oflag = O_RDONLY;
    if ((source = open(path1, oflag)) < 0) {
        return -1;
    }

    mode_t mode = 0600;
    if ((target = creat(path2, mode)) < 0) {
        close(target);
        return -1;
    }

    // For BSD unices use fcopyfile
#if defined(__APPLE__) || defined(__FreeBSD__)
    unsigned int flag = COPYFILE_DATA;
    //uint32_t flag = COPYFILE_DATA;
    // For other flags such as COPYFILE_ALL see:
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/fcopyfile.3.html
    int result = fcopyfile(source, target, 0, flag);
#else
    // For Linux use sendfile
    off_t bytesCopied = 0;
    struct stat fileinfo = {0};
    fstat(source, &fileinfo);
    int result = sendfile(target, source, &bytesCopied, fileinfo.st_size);
#endif

    close(source);
    close(target);

    return result;
}


// Exit if malloc returns NULL
void ExitIfNull(void *ptr, const char *path, int line)
{
    if (ptr) { return; }   // Return if it's good
    if (path && line) {
        fprintf(stderr, "%s:%d malloc error\n", path, line);
    }
    PrintVBoxException();  // Print API errors if any
    Exit(EXIT_FAILURE);
}


// Exit if return code shows failure
void ExitIfFailure(HRESULT rc, const char *msg, const char *path, int line)
{
    if (SUCCEEDED(rc)) { return; }   // Return if it's good
    if (msg && path && line) {
        fprintf(stderr, "%s:%d %s error\n", path, line, msg);
    }
    PrintVBoxException();  // Print API errors if any
    Exit(EXIT_FAILURE);
}


// Return current time in H:M:S.ms string format
char * TimeNow(void)
{
    // Do millisec part    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int millisec = lrint(tv.tv_usec/1000.0);  // Round to nearest millisec
    if (millisec >= 1000) {     // Allow for rounding up to nearest second
        millisec -= 1000;
        tv.tv_sec++;
    }

    // Do H:M:S part
    struct tm *tm_info;
    tm_info = localtime(&tv.tv_sec);

    char hms[9];
    strftime(hms, 9, "%H:%M:%S", tm_info);
    char *timeStr = NewString(14);
    sprintf(timeStr, "%s.%03d", hms, millisec);

    // REMINDER: Caller must free allocated memory
    return timeStr;
}


// Print debug message with current time
void DEBUG(char *msg)
{
    char *now = TimeNow();
    printf("%s %s\n", now, msg);
    free(now);
}


// Allocate memory for new character string 
char * NewString(int size)
{
    if (size < 1) {
        fprintf(stderr, "%s:%d size < 1 not allowed", __FILE__, __LINE__);
        Exit(EXIT_FAILURE);
    }
    char *string = malloc(sizeof(char) * size);
    ExitIfNull(string, __FILE__, __LINE__);
    // REMINDER: Caller must free allocated memory
    return string;
}
