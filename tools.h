#ifndef TOOLS_H                                                                                                    
#define TOOLS_H 1

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define SYSTEM_DIR_DELIMITER '\\'
#include <windows.h>
#else
#define SYSTEM_DIR_DELIMITER '/'
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#endif

#include "comdef.h"

#ifdef COM_INLINE
#define TOOLS_INLINE COM_INLINE
#else
#define TOOLS_INLINE inline
#endif

#define MAX_PATH_LENGTH 2048

#ifdef __cplusplus
extern "C" {
#endif

  typedef int (*listDirCallback_t)(int i, char *dirEntry, int isDirectory, void *data);

int isLogOpen(void);

/* NULL or "" to close log:*/
int openLog (char *logFileName);
void errorMsg(char *fmt, ...);
void message(char *fmt, ...);
void halt(int retval,char *fmt, ...);

void blockMessage(void);
void unblockMessage(void);
void unblockedMessage(char *fmt, ...);

/* to be independent on locale.
   Converts ONLY ASCII low-case one-byte chars!:*/
static TOOLS_INLINE
int myToupper (int ch)
{
   if ( (ch >='a')&&(ch <= 'z') )
   {
      return ch - ('a' - 'A');
   }
   return ch;
}/*s_toupper*/

int listDir(char *providedPath, listDirCallback_t theCallback, void *data);

static TOOLS_INLINE
int getFileSize(FILE *f)
{
   long int fSize = 0;
   fseek(f,0,SEEK_END);
   fSize=ftell(f);
   fseek(f,0,SEEK_SET);
   return fSize;
}/*getFileSize*/

static TOOLS_INLINE
int getCpuNumber(void)
{
#ifdef _WIN32
  SYSTEM_INFO siSysInfo;
  GetSystemInfo(&siSysInfo);
  return siSysInfo.dwNumberOfProcessors;
#else
   return sysconf( _SC_NPROCESSORS_ONLN );
#endif
}/*getCpuNumber*/

#ifdef __cplusplus
}
#endif

#endif
