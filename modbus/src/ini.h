#ifndef MININI_H
#define MININI_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#if defined NDEBUG
  #define assert(e)
#else
  #include <assert.h>
#endif

#define mTCHAR char

#define INI_BUFFERSIZE  512

#define TCHAR     char
#define __T(s)    s
#define _tcscat   strcat
#define _tcschr   strchr
#define _tcscmp   strcmp
#define _tcscpy   strcpy
#define _tcsicmp  stricmp
#define _tcslen   strlen
#define _tcsncmp  strncmp
#define _tcsnicmp strnicmp
#define _tcsrchr  strrchr
#define _tcstol   strtol
#define _tcstod   strtod
#define _totupper toupper
#define _stprintf sprintf
#define _tfgets   fgets
#define _tfputs   fputs
#define _tfopen   fopen
#define _tremove  remove
#define _trename  rename

#define INI_LINETERM    __T("\n")

#define sizearray(a)    (sizeof(a) / sizeof((a)[0]))

#define INI_FILETYPE                    FILE*
#define ini_openread(filename,file)     ((*(file) = fopen((filename),"rb")) != NULL)
#define ini_openwrite(filename,file)    ((*(file) = fopen((filename),"wb")) != NULL)
#define ini_openrewrite(filename,file)  ((*(file) = fopen((filename),"r+b")) != NULL)
#define ini_close(file)                 (fclose(*(file)) == 0)
#define ini_read(buffer,size,file)      (fgets((buffer),(size),*(file)) != NULL)
#define ini_write(buffer,file)          (fputs((buffer),*(file)) >= 0)
#define ini_rename(source,dest)         (rename((source), (dest)) == 0)
#define ini_remove(filename)            (remove(filename) == 0)

#define INI_FILEPOS                     long int
#define ini_tell(file,pos)              (*(pos) = ftell(*(file)))
#define ini_seek(file,pos)              (fseek(*(file), *(pos), SEEK_SET) == 0)

#define INI_REAL                        float
#define ini_ftoa(string,value)          sprintf((string),"%f",(value))
#define ini_atof(string)                (INI_REAL)strtod((string),NULL)

enum quote_option {
  QUOTE_NONE,
  QUOTE_ENQUOTE,
  QUOTE_DEQUOTE,
};

int   ini_getbool(const mTCHAR *Section, const mTCHAR *Key, int DefValue, const mTCHAR *Filename);
long  ini_getl(const mTCHAR *Section, const mTCHAR *Key, long DefValue, const mTCHAR *Filename);
int   ini_gets(const mTCHAR *Section, const mTCHAR *Key, const mTCHAR *DefValue, mTCHAR *Buffer, int BufferSize, const mTCHAR *Filename);
int   ini_getsection(int idx, mTCHAR *Buffer, int BufferSize, const mTCHAR *Filename);
int   ini_getkey(const mTCHAR *Section, int idx, mTCHAR *Buffer, int BufferSize, const mTCHAR *Filename);
INI_REAL ini_getf(const mTCHAR *Section, const mTCHAR *Key, INI_REAL DefValue, const mTCHAR *Filename);


typedef int (*INI_CALLBACK)(const mTCHAR *Section, const mTCHAR *Key, const mTCHAR *Value, const void *UserData);
int  	ini_browse(INI_CALLBACK Callback, const void *UserData, const mTCHAR *Filename);

#endif /* MININI_H */
