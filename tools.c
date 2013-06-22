#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <pthread.h>

#include "tools.h"

static pthread_mutex_t g_mIO = PTHREAD_MUTEX_INITIALIZER;
static FILE *l_logFile = NULL;

int isLogOpen(void)
{
   return (NULL != l_logFile);
}/*isLogOpen*/

int openLog (char *logFileName)
{
   if (NULL != l_logFile)
   {
      fclose (l_logFile);
      l_logFile = NULL;
   }
   if ( (NULL == logFileName) || ('\0' ==  *logFileName) )
   {
      return 0;
   }
   l_logFile = fopen (logFileName, "w");
   if (NULL != l_logFile)
   {
#ifndef _WIN32
      setlinebuf(l_logFile);
#endif
   }
   return (NULL == l_logFile);
}/*openLog*/

void errorMsg(char *fmt, ...)
{
  va_list arg_ptr;
  va_start (arg_ptr, fmt);
  pthread_mutex_lock( &g_mIO);
  vfprintf(stderr,fmt,arg_ptr);
  if (NULL != l_logFile)
  {
     va_start (arg_ptr, fmt);
     vfprintf(l_logFile,fmt,arg_ptr);
  }
  pthread_mutex_unlock( &g_mIO);
  va_end (arg_ptr);
}/*errorMsg*/

void unblockedMessage(char *fmt, ...)
{
  va_list arg_ptr;
  va_start (arg_ptr, fmt);

  vfprintf(stdout,fmt,arg_ptr);

  if (NULL != l_logFile)
  {
     va_start (arg_ptr, fmt);
     vfprintf(l_logFile,fmt,arg_ptr);
  }

  va_end (arg_ptr);
}/*unblockedMessage*/

void message(char *fmt, ...)
{
  va_list arg_ptr;
  va_start (arg_ptr, fmt);
  pthread_mutex_lock( &g_mIO);
  vfprintf(stdout,fmt,arg_ptr);

  if (NULL != l_logFile)
  {
     va_start (arg_ptr, fmt);
     vfprintf(l_logFile,fmt,arg_ptr);
  }

  pthread_mutex_unlock( &g_mIO);
  va_end (arg_ptr);
}/*message*/

void blockMessage(void)
{
  pthread_mutex_lock (&g_mIO);
}/*blockMessage*/

void unblockMessage(void)
{
  pthread_mutex_unlock (&g_mIO);
}/*unblockMessage*/

void halt(int retval,char *fmt, ...)
{
  va_list arg_ptr;
  va_start (arg_ptr, fmt);
  pthread_mutex_lock( &g_mIO);
  vfprintf(stderr,fmt,arg_ptr);
 if (NULL != l_logFile)
  {
     va_start (arg_ptr, fmt);
     vfprintf(l_logFile,fmt,arg_ptr);
  } 
  va_end (arg_ptr);
  pthread_mutex_unlock( &g_mIO);
  exit(retval);
}/*halt*/

/*Calls 'theCallback' for each entry of a directory 'providedPath'.
 'data' is the external data for the callback 'theCallback'. Returns
 the number of processed enties, or <0 on error. If the callback 
 returns !0, stops processing:*/
int listDir(char *providedPath, listDirCallback_t theCallback, void *data)
{
#ifdef _WIN32

   WIN32_FIND_DATA FindFileData;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   char dirPath[MAX_PATH_LENGTH+2];
#else
   DIR *dp = NULL;
   struct dirent *dptr = NULL;

   char dirPath[MAX_PATH_LENGTH];
#endif
   char *ch;
   int i;

   if ( NULL == providedPath )
   {
      return -1;/*NULL path*/
   }

   if ( '\0' == *providedPath )
   {

#ifdef _WIN32
      providedPath = ".\\*";
#else
      providedPath = ".";
#endif
   }

   for(i = 0,ch = providedPath; '\0' != *ch ; ++i, ++ch)
   {
      if ( i >= MAX_PATH_LENGTH)
      {
         return -2;/*Path too long*/
      }
      dirPath[i] = *ch;
   }
   if (  (i + 2 ) > MAX_PATH_LENGTH )
   {
      return -2;/*Path too long*/
   }

   ch = dirPath + i;
#ifdef _WIN32
   /*FindFile requires "\\*" */
   if ( '\\' == ch[-1] )
   {
      *ch++ = '*';
   }
   else if ( '*' != ch[-1] )
   {
      *ch++ = '\\';
      *ch++ = '*';
   }
   *ch = '\0';
   i = 0;
   hFind = FindFirstFile(dirPath, &FindFileData);
   if (INVALID_HANDLE_VALUE == hFind)
   {
      return -3;/*Cannot open Input directory*/
   }

   if ( 0 != (*theCallback)(i, 
                  FindFileData.cFileName, 
                  (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), data) 
      )
   {
      return i;/*callback says "Enough!"*/
   }
   ++i;
   while (FindNextFile(hFind, &FindFileData) != 0)
   {
      if ( 0 != (*theCallback)(i, 
                  FindFileData.cFileName, 
                  (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), data) 
         )
      {
         break;/*callback says "Enough!"*/
      }
      ++i;
   }
   FindClose(hFind);
#else
   if ( SYSTEM_DIR_DELIMITER != ch[-1])
   {
      *ch++ = SYSTEM_DIR_DELIMITER;
   }

   *ch = '\0';

   if(NULL == (dp = opendir(dirPath)) )
   {
      return -3;/*Cannot open Input directory*/
   }
   i = 0;
   while(NULL != (dptr = readdir(dp)) )
   {
      if ( 0 != (*theCallback)(i, dptr->d_name, (dptr->d_type & DT_DIR), data) )
      {
         break;/*callback says "Enough!"*/
      }
      ++i;
   }
   closedir(dp);
#endif
   return i;
}/*listDir*/
