/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2019 M/Gateway Developments Ltd,                      |
   | Surrey UK.                                                               |
   | All rights reserved.                                                     |
   |                                                                          |
   | http://www.mgateway.com                                                  |
   |                                                                          |
   | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
   | not use this file except in compliance with the License.                 |
   | You may obtain a copy of the License at                                  |
   |                                                                          |
   | http://www.apache.org/licenses/LICENSE-2.0                               |
   |                                                                          |
   | Unless required by applicable law or agreed to in writing, software      |
   | distributed under the License is distributed on an "AS IS" BASIS,        |
   | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
   | See the License for the specific language governing permissions and      |
   | limitations under the License.                                           |      
   |                                                                          |
   ----------------------------------------------------------------------------
*/

#ifndef MG_DBX_H
#define MG_DBX_H

#include <node_version.h>

#define DBX_NODE_VERSION         (NODE_MAJOR_VERSION * 10000) + (NODE_MINOR_VERSION * 100) + NODE_PATCH_VERSION

#define DBX_VERSION_MAJOR        "1"
#define DBX_VERSION_MINOR        "1"
#define DBX_VERSION_BUILD        "5"

#define DBX_VERSION              DBX_VERSION_MAJOR "." DBX_VERSION_MINOR "." DBX_VERSION_BUILD

#define DBX_STR_HELPER(x)        #x
#define DBX_STR(x)               DBX_STR_HELPER(x)


/*
#if defined(_WIN32)
#define DBX_USE_SECURE_CRT       1
#endif
*/

/*
#if !defined(SOLARIS)
#define DBX_CHK_SECURE_CRT       1
#endif
*/

#define DBX_DBNAME               dbx
#define DBX_DBNAME_STR           "dbx"

#if defined(_WIN32)

#define BUILDING_NODE_EXTENSION     1
#if defined(_MSC_VER)
/* Check for MS compiler later than VC6 */
#if (_MSC_VER >= 1400)
#if !defined(DBX_USE_SECURE_CRT)
#define _CRT_SECURE_NO_DEPRECATE    1
#define _CRT_NONSTDC_NO_DEPRECATE   1
#endif
#endif
#endif

#elif defined(__linux__) || defined(__linux) || defined(linux)

#if !defined(LINUX)
#define LINUX                       1
#endif

#elif defined(__APPLE__)

#if !defined(MACOSX)
#define MACOSX                      1
#endif

#endif

#if defined(SOLARIS)
#ifndef __GNUC__
#  define  __attribute__(x)
#endif
#endif


#if defined(_WIN32)
#include <string>
#include <time.h>

#if defined(DBX_WINSOCK2)
#define INCL_WINSOCK_API_TYPEDEFS 1
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#endif

#if !defined(_WIN32)
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#endif

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

#include <uv.h>
#include <node_object_wrap.h>

#if !defined(_WIN32)

#include <pthread.h>
#include <dlfcn.h>
#endif

#define DBX_MAXARGS              64
#define DBX_DEFAULT_TIMEOUT      30

#define DBX_THREADPOOL_MAX       8

#define DBX_ERROR_SIZE           512

#define DBX_THREAD_STACK_SIZE    0xf0000

#define DBX_TEXT_E_ASYNC         "Unable to process task asynchronously"

#define DBX_TYPE_NONE            -1
#define DBX_TYPE_STR             -2
#define DBX_TYPE_STR8            0
#define DBX_TYPE_STR16           1
#define DBX_TYPE_STROBJ          2
#define DBX_TYPE_INT             3
#define DBX_TYPE_INT64           4
#define DBX_TYPE_DOUBLE          5
#define DBX_TYPE_OREF            6
#define DBX_TYPE_BUF             7
#define DBX_TYPE_NULL            10

#define DBX_XTYPE                21

#if defined(MAX_PATH) && (MAX_PATH>511)
#define DBX_MAX_PATH             MAX_PATH
#else
#define DBX_MAX_PATH             512
#endif

#if defined(_WIN32)
#define DBX_NULL_DEVICE          "//./nul"
#else
#define DBX_NULL_DEVICE          "/dev/null/"
#endif


#if defined(_WIN32)
#define DBX_CACHE_DLL            "cache.dll"
#define DBX_IRIS_DLL             "irisdb.dll"
#define DBX_YDB_DLL              "yottadb.dll"
#else
#define DBX_CACHE_SO             "libcache.so"
#define DBX_CACHE_DYLIB          "libcache.dylib"
#define DBX_IRIS_SO              "libirisdb.so"
#define DBX_IRIS_DYLIB           "libirisdb.dylib"
#define DBX_YDB_SO               "libyottadb.so"
#define DBX_YDB_DYLIB            "libyottadb.dylib"
#endif


#if defined(_WIN32)
#define _dbxso(a)                _countof(a)
#else
#define _dbxso(a)                sizeof(a)
#endif

#if defined(DBX_USE_SECURE_CRT)
/* #define T_STRCPY(a, b, c)        _tcscpy_s(a, b, c) */
/* #define T_STRNCPY(a, b, c, d)    _tcsncpy_s(a, b, c, d) */
#define T_STRCPY(a, b, c)        strcpy_s(a, b, c)
#define T_STRNCPY(a, b, c, d)    memcpy_s(a, b, c, d)

#define T_STRCAT(a, b, c)        strcat_s(a, b, c)
#define T_STRNCAT(a, b, c, d)    strncat_s(a, b, c, d)
#define T_SPRINTF(a, b, c, ...)  sprintf_s(a, b, c, __VA_ARGS__)
#if defined(LINUX)
#define T_MEMCPY(a,b,c)          memmove(a,b,c)
#else
#define T_MEMCPY(a,b,c)          memcpy(a,b,c)
#endif
#elif defined(DBX_CHK_SECURE_CRT)
#define T_STRCPY(a, b, c)        dbx_strcpy_s(a, b, c, __FILE__, __FUNCTION__, __LINE__)
#define T_STRNCPY(a, b, c, d)    dbx_strncpy_s(a, b, c, d, __FILE__, __FUNCTION__, __LINE__)
#define T_STRCAT(a, b, c)        dbx_strcat_s(a, b, c, __FILE__, __FUNCTION__, __LINE__)
#define T_STRNCAT(a, b, c, d)    dbx_strncat_s(a, b, c, d, __FILE__, __FUNCTION__, __LINE__)
#define T_SPRINTF(a, b, c, ...)  dbx_sprintf_s(a, b, c,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#if defined(LINUX)
#define T_MEMCPY(a,b,c)          memmove(a,b,c)
#else
#define T_MEMCPY(a,b,c)          memcpy(a,b,c)
#endif
#else
#define T_STRCPY(a, b, c)        strcpy(a, c)
#define T_STRNCPY(a, b, c, d)    strncpy(a, c, d)

#define T_STRCAT(a, b, c)        strcat(a, c)
#define T_STRNCAT(a, b, c, d)    strncat(a, c, d)
#define T_SPRINTF(a, b, c, ...)  sprintf(a, c, __VA_ARGS__)
#if defined(LINUX)
#define T_MEMCPY(a,b,c)          memmove(a,b,c)
#else
#define T_MEMCPY(a,b,c)          memcpy(a,b,c)
#endif
#endif

#define DBX_INTEGER_NEW(a)          Integer::New(isolate, a)
#define DBX_INT32_NEW(a)            Int32::New(isolate, a)
#define DBX_UINT32_NEW(a)           Uint32::New(isolate, a)
#define DBX_NUMBER_NEW(a)           Number::New(isolate, a)
#define DBX_BOOLEAN_NEW(a)          Boolean::New(isolate, a)
#define DBX_NULL()                  Null(isolate)
#define DBX_DATE(a)                 Date::New(isolate, a)

#define DBX_OBJECT_NEW()            Object::New(isolate)
#define DBX_ARRAY_NEW(a)            Array::New(isolate, a)

#define DBX_NODE_SET_PROTOTYPE_METHOD(a, b)    dbx_set_prototype_method(t, b, a, a);
#define DBX_NODE_SET_PROTOTYPE_METHODC(a, b)   dbx_set_prototype_method(tpl, b, a, a);

#if DBX_NODE_VERSION >= 100000

#define DBX_GET_ISOLATE \
   if (c->isolate == NULL) { \
      c->isolate = args.GetIsolate(); \
      HandleScope scope(c->isolate); \
   } \
   Isolate* isolate = c->isolate; \

#define DBX_GET_ICONTEXT \
   if (c->isolate == NULL) { \
      c->isolate = args.GetIsolate(); \
      HandleScope scope(c->isolate); \
   } \
   if (c->got_icontext == 0) { \
      c->icontext = c->isolate->GetCurrentContext(); \
      c->got_icontext = 1; \
   } \
   Isolate* isolate = c->isolate; \
   Local<Context> icontext = c->icontext; \

#else

#define DBX_GET_ISOLATE \
   if (c->isolate == NULL) { \
      c->isolate = args.GetIsolate(); \
      HandleScope scope(c->isolate); \
   } \
   Isolate* isolate = c->isolate; \

#define DBX_GET_ICONTEXT \
   if (c->isolate == NULL) { \
      c->isolate = args.GetIsolate(); \
      HandleScope scope(c->isolate); \
   } \
   Isolate* isolate = c->isolate; \

#endif


#if DBX_NODE_VERSION >= 120000

#define DBX_DBFUN_START(C, PCON) \
   if (!C->open) { \
      if (PCON && PCON->error[0]) { \
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, PCON->error, NewStringType::kNormal).ToLocalChecked())); \
         return; \
      } \
      else { \
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Database not open", NewStringType::kNormal).ToLocalChecked())); \
         return; \
      } \
   } \

#else

#define DBX_DBFUN_START(C, PCON) \
   if (!C->open) { \
      if (PCON && PCON->error[0]) { \
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, PCON->error))); \
         return; \
      } \
      else { \
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Database not open"))); \
         return; \
      } \
   } \

#endif

#if DBX_NODE_VERSION >= 120000
#define DBX_GET(a,b)                a->Get(icontext,b).ToLocalChecked()
#define DBX_SET(a,b,c)              a->Set(icontext,b,c).FromJust()
#define DBX_TO_OBJECT(a)            a->ToObject(icontext).ToLocalChecked()
#define DBX_TO_STRING(a)            a->ToString(icontext).ToLocalChecked()
#define DBX_TO_BOOLEAN(a)           a->ToBoolean(isolate)
#define DBX_NUMBER_VALUE(a)         a->NumberValue(icontext).ToChecked()
#define DBX_INT32_VALUE(a)          a->Int32Value(icontext).FromJust()
#define DBX_WRITE_UTF8(a,b)         a->WriteUtf8(isolate, b)
#define DBX_WRITE(a,b)              a->Write(isolate, (uint16_t *) b)
#define DBX_WRITE_ONE_BYTE(a,b)     a->WriteOneByte(isolate, b)
#define DBX_UTF8_LENGTH(a)          a->Utf8Length(isolate)
#define DBX_LENGTH(a)               a->Length()
#elif DBX_NODE_VERSION >= 100000
#define DBX_GET(a,b)                a->Get(icontext,b).ToLocalChecked()
#define DBX_SET(a,b,c)              a->Set(icontext,b,c).FromJust()
#define DBX_TO_OBJECT(a)            a->ToObject(icontext).ToLocalChecked()
#define DBX_TO_STRING(a)            a->ToString(icontext).ToLocalChecked()
#define DBX_TO_BOOLEAN(a)           a->ToBoolean(icontext).ToLocalChecked()
#define DBX_NUMBER_VALUE(a)         a->NumberValue(icontext).ToChecked()
#define DBX_INT32_VALUE(a)          a->Int32Value(icontext).FromJust()
#define DBX_WRITE_UTF8(a,b)         a->WriteUtf8(b)
#define DBX_WRITE(a,b)              a->Write((uint16_t *) b)
#define DBX_WRITE_ONE_BYTE(a,b)     a->WriteOneByte(b)
#define DBX_UTF8_LENGTH(a)          a->Utf8Length()
#define DBX_LENGTH(a)               a->Length()
#else
#define DBX_GET(a,b)                a->Get(b)
#define DBX_SET(a,b,c)              a->Set(b,c)
#define DBX_TO_OBJECT(a)            a->ToObject()
#define DBX_TO_STRING(a)            a->ToString()
#define DBX_TO_BOOLEAN(a)           a->ToBoolean()
#define DBX_NUMBER_VALUE(a)         a->NumberValue()
#define DBX_INT32_VALUE(a)          a->Int32Value()
#define DBX_WRITE_UTF8(a,b)         a->WriteUtf8(b)
#define DBX_WRITE(a,b)              a->Write((uint16_t *) b)
#define DBX_WRITE_ONE_BYTE(a,b)     a->WriteOneByte(b)
#define DBX_UTF8_LENGTH(a)          a->Utf8Length()
#define DBX_LENGTH(a)               a->Length()
#endif

#define DBX_INTEGER_NEW(a)          Integer::New(isolate, a)
#define DBX_OBJECT_NEW()            Object::New(isolate)
#define DBX_ARRAY_NEW(a)            Array::New(isolate, a)
#define DBX_ARRAY_NEW(a)            Array::New(isolate, a)
#define DBX_NUMBER_NEW(a)           Number::New(isolate, a)
#define DBX_BOOLEAN_NEW(a)          Boolean::New(isolate, a)
#define DBX_NULL()                  Null(isolate)

#define DBX_CALLBACK_FUN(JSNARG, CB, ASYNC) \
   JSNARG = args.Length(); \
   if (JSNARG > 0 && args[JSNARG - 1]->IsFunction()) { \
      ASYNC = 1; \
      JSNARG --; \
   } \
   else { \
      ASYNC = 0; \
   } \

#define DBX_DBFUN_END(C) \
   if (C->debug.debug == 1) { \
      fprintf(C->debug.p_fdebug, "\r\n"); \
      fflush(C->debug.p_fdebug); \
   } \


#define DBX_DB_LOCK(RC, TIMEOUT) \
   if (pcon->use_mutex) { \
      RC = dbx_mutex_lock(pcon->p_mutex, TIMEOUT); \
   } \

#define DBX_DB_UNLOCK(RC) \
   if (pcon->use_mutex) { \
      RC = dbx_mutex_unlock(pcon->p_mutex); \
   } \


typedef void      async_rtn;

#if defined(_WIN32)

typedef DWORD           DBXTHID;
typedef HINSTANCE       DBXPLIB;
typedef FARPROC         DBXPROC;

#else /* #if defined(_WIN32) */
   
typedef pthread_t       DBXTHID;
typedef void            *DBXPLIB;
typedef void            *DBXPROC;
#if !defined(NODE_ENGINE_CHAKRACORE)
typedef unsigned long   DWORD;
#endif
typedef unsigned long   WORD;

#endif /* #if defined(_WIN32) */


typedef struct tagDBXDEBUG {
   unsigned char  debug;
   FILE *         p_fdebug;
} DBXDEBUG, *PDBXDEBUG;


typedef struct tagDBXZV {
   unsigned char  dbtype;
   double         dbx_version;
   int            majorversion;
   int            minorversion;
   int            dbx_build;
   unsigned long  vnumber; /* yymbbbb */
   char           version[64]; /* v1.0.17 */
} DBXZV, *PDBXZV;


typedef struct tagDBXMUTEX {
   unsigned char     created;
   int               stack;
#if defined(_WIN32)
   HANDLE            h_mutex;
#else
   pthread_mutex_t   h_mutex;
#endif /* #if defined(_WIN32) */
   DBXTHID           thid;
} DBXMUTEX, *PDBXMUTEX;

typedef struct tagDBXTID {
   int         thread_id;
   DBXMUTEX    *p_mutex;
   DBXZV       *p_zv;
} DBXTID, *PDBXTID;


/* Cache/IRIS */

#define CACHE_MAXSTRLEN	32767
#define CACHE_MAXLOSTSZ	3641144

typedef char		Callin_char_t;
#define CACHE_INT64 long long
#define CACHESTR	CACHE_ASTR

typedef struct {
   unsigned short len;
   Callin_char_t  str[CACHE_MAXSTRLEN];
} CACHE_ASTR, *CACHE_ASTRP;

typedef struct {
   unsigned int	len;
   union {
      Callin_char_t * ch;
      unsigned short *wch;
      unsigned short *lch;
   } str;
} CACHE_EXSTR, *CACHE_EXSTRP;

#define CACHE_TTALL     1
#define CACHE_TTNEVER   8
#define CACHE_PROGMODE  32

#define CACHE_INCREMENTAL_LOCK   1
#define CACHE_SHARED_LOCK        2
#define CACHE_IMMEDIATE_RELEASE  4

#define CACHE_INT	      1
#define CACHE_DOUBLE	   2
#define CACHE_ASTRING   3

#define CACHE_CHAR      4
#define CACHE_INT2      5
#define CACHE_INT4      6
#define CACHE_INT8      7
#define CACHE_UCHAR     8
#define CACHE_UINT2     9
#define CACHE_UINT4     10
#define CACHE_UINT8     11
#define CACHE_FLOAT     12
#define CACHE_HFLOAT    13
#define CACHE_UINT      14
#define CACHE_WSTRING   15
#define CACHE_OREF      16
#define CACHE_LASTRING  17
#define CACHE_LWSTRING  18
#define CACHE_IEEE_DBL  19
#define CACHE_HSTRING   20
#define CACHE_UNDEF     21

#define CACHE_CHANGEPASSWORD  -16
#define CACHE_ACCESSDENIED    -15
#define CACHE_EXSTR_INUSE     -14
#define CACHE_NORES	         -13
#define CACHE_BADARG	         -12
#define CACHE_NOTINCACHE      -11
#define CACHE_RETTRUNC 	      -10
#define CACHE_ERUNKNOWN	      -9	
#define CACHE_RETTOOSMALL     -8	
#define CACHE_NOCON 	         -7
#define CACHE_INTERRUPT       -6
#define CACHE_CONBROKEN       -4
#define CACHE_STRTOOLONG      -3
#define CACHE_ALREADYCON      -2
#define CACHE_FAILURE	      -1
#define CACHE_SUCCESS 	      0

#define CACHE_ERMXSTR         5
#define CACHE_ERNOLINE        8
#define CACHE_ERUNDEF         9
#define CACHE_ERSYSTEM        10
#define CACHE_ERSUBSCR        16
#define CACHE_ERNOROUTINE     17
#define CACHE_ERSTRINGSTACK   20
#define CACHE_ERUNIMPLEMENTED 22
#define CACHE_ERARGSTACK      25
#define CACHE_ERPROTECT       27
#define CACHE_ERPARAMETER     40
#define CACHE_ERNAMSP         83
#define CACHE_ERWIDECHAR      89
#define CACHE_ERNOCLASS       122
#define CACHE_ERBADOREF       119
#define CACHE_ERNOMETHOD      120
#define CACHE_ERNOPROPERTY    121

#define CACHE_ETIMEOUT        -100
#define CACHE_BAD_STRING      -101
#define CACHE_BAD_NAMESPACE   -102
#define CACHE_BAD_GLOBAL      -103
#define CACHE_BAD_FUNCTION    -104

/* End of Cache/IRIS */


/* YottaDB */

#define YDB_OK       0
#define YDB_DEL_TREE 1

#define YDB_INT_MAX        ((int)0x7fffffff)
#define YDB_TP_RESTART     (YDB_INT_MAX - 1)
#define YDB_TP_ROLLBACK    (YDB_INT_MAX - 2)
#define YDB_NODE_END       (YDB_INT_MAX - 3)
#define YDB_LOCK_TIMEOUT   (YDB_INT_MAX - 4)
#define YDB_NOTOK          (YDB_INT_MAX - 5)

typedef struct {
   unsigned int   len_alloc;
   unsigned int   len_used;
   char		      *buf_addr;
} ydb_buffer_t;

typedef struct {
   unsigned long  length;
   char		      *address;
} ydb_string_t;

typedef struct {
   ydb_string_t   rtn_name;
   void		      *handle;
} ci_name_descriptor;

typedef ydb_buffer_t DBXSTR;

/* End of YottaDB */


typedef struct tagDBXCVAL {
   void           *pstr;
   CACHE_EXSTR    zstr;
} DBXCVAL, *PDBXCVAL;


typedef struct tagDBXVAL {
   short          type;
   union {
      int            int32;
      long long      int64;
      double         real;
      unsigned int   oref;
   } num;
   unsigned long  offs;
   ydb_buffer_t svalue;
   DBXCVAL cvalue;
   struct tagDBXVAL *pnext;
} DBXVAL, *PDBXVAL;


typedef struct tagDBXFUN {
   unsigned int   rflag;
   int            label_len;
   char *         label;
   int            routine_len;
   char *         routine;
   char           buffer[128];
} DBXFUN, *PDBXFUN;


typedef struct tagDBXGREF {
   char *         global;
   DBXVAL *       pkey;
} DBXGREF, *PDBXGREF;

typedef struct tagDBXFREF {
   char *         function;
} DBXFREF, *PDBXFREF;


#define DBX_DBTYPE_CACHE     1
#define DBX_DBTYPE_IRIS      2
#define DBX_DBTYPE_YOTTADB   5

typedef struct tagDBXISCSO {
   short             loaded;
   short             iris;
   short             merge_enabled;
   short             functions_enabled;
   short             objects_enabled;
   short             multithread_enabled;
   char              funprfx[8];
   char              libdir[256];
   char              libnam[256];
   char              dbname[32];
   DBXPLIB           p_library;

   int               (* p_CacheSetDir)                   (char * dir);
   int               (* p_CacheSecureStartA)             (CACHE_ASTRP username, CACHE_ASTRP password, CACHE_ASTRP exename, unsigned long flags, int tout, CACHE_ASTRP prinp, CACHE_ASTRP prout);
   int               (* p_CacheEnd)                      (void);
   unsigned char *   (* p_CacheExStrNew)                 (CACHE_EXSTRP zstr, int size);
   unsigned short *  (* p_CacheExStrNewW)                (CACHE_EXSTRP zstr, int size);
   wchar_t *         (* p_CacheExStrNewH)                (CACHE_EXSTRP zstr, int size);
   int               (* p_CachePushExStr)                (CACHE_EXSTRP sptr);
   int               (* p_CachePushExStrW)               (CACHE_EXSTRP sptr);
   int               (* p_CachePushExStrH)               (CACHE_EXSTRP sptr);
   int               (* p_CachePopExStr)                 (CACHE_EXSTRP sstrp);
   int               (* p_CachePopExStrW)                (CACHE_EXSTRP sstrp);
   int               (* p_CachePopExStrH)                (CACHE_EXSTRP sstrp);
   int               (* p_CacheExStrKill)                (CACHE_EXSTRP obj);
   int               (* p_CachePushStr)                  (int len, Callin_char_t * ptr);
   int               (* p_CachePushStrW)                 (int len, short * ptr);
   int               (* p_CachePushStrH)                 (int len, wchar_t * ptr);
   int               (* p_CachePopStr)                   (int * lenp, Callin_char_t ** strp);
   int               (* p_CachePopStrW)                  (int * lenp, short ** strp);
   int               (* p_CachePopStrH)                  (int * lenp, wchar_t ** strp);
   int               (* p_CachePushDbl)                  (double num);
   int               (* p_CachePushIEEEDbl)              (double num);
   int               (* p_CachePopDbl)                   (double * nump);
   int               (* p_CachePushInt)                  (int num);
   int               (* p_CachePopInt)                   (int * nump);
   int               (* p_CachePushInt64)                (CACHE_INT64 num);
   int               (* p_CachePopInt64)                 (CACHE_INT64 * nump);
   int               (* p_CachePushGlobal)               (int nlen, const Callin_char_t * nptr);
   int               (* p_CachePushGlobalX)              (int nlen, const Callin_char_t * nptr, int elen, const Callin_char_t * eptr);
   int               (* p_CacheGlobalGet)                (int narg, int flag);
   int               (* p_CacheGlobalSet)                (int narg);
   int               (* p_CacheGlobalData)               (int narg, int valueflag);
   int               (* p_CacheGlobalKill)               (int narg, int nodeonly);
   int               (* p_CacheGlobalOrder)              (int narg, int dir, int valueflag);
   int               (* p_CacheGlobalQuery)              (int narg, int dir, int valueflag);
   int               (* p_CacheGlobalIncrement)          (int narg);
   int               (* p_CacheGlobalRelease)            (void);
   int               (* p_CacheAcquireLock)              (int nsub, int flg, int tout, int * rval);
   int               (* p_CacheReleaseAllLocks)          (void);
   int               (* p_CacheReleaseLock)              (int nsub, int flg);
   int               (* p_CachePushLock)                 (int nlen, const Callin_char_t * nptr);
   int               (* p_CacheAddGlobal)                (int num, const Callin_char_t * nptr);
   int               (* p_CacheAddGlobalDescriptor)      (int num);
   int               (* p_CacheAddSSVN)                  (int num, const Callin_char_t * nptr);
   int               (* p_CacheAddSSVNDescriptor)        (int num);
   int               (* p_CacheMerge)                    (void);
   int               (* p_CachePushFunc)                 (unsigned int * rflag, int tlen, const Callin_char_t * tptr, int nlen, const Callin_char_t * nptr);
   int               (* p_CacheExtFun)                   (unsigned int flags, int narg);
   int               (* p_CachePushRtn)                  (unsigned int * rflag, int tlen, const Callin_char_t * tptr, int nlen, const Callin_char_t * nptr);
   int               (* p_CacheDoFun)                    (unsigned int flags, int narg);
   int               (* p_CacheDoRtn)                    (unsigned int flags, int narg);
   int               (* p_CacheCloseOref)                (unsigned int oref);
   int               (* p_CacheIncrementCountOref)       (unsigned int oref);
   int               (* p_CachePopOref)                  (unsigned int * orefp);
   int               (* p_CachePushOref)                 (unsigned int oref);
   int               (* p_CacheInvokeMethod)             (int narg);
   int               (* p_CachePushMethod)               (unsigned int oref, int mlen, const Callin_char_t * mptr, int flg);
   int               (* p_CacheInvokeClassMethod)        (int narg);
   int               (* p_CachePushClassMethod)          (int clen, const Callin_char_t * cptr, int mlen, const Callin_char_t * mptr, int flg);
   int               (* p_CacheGetProperty)              (void);
   int               (* p_CacheSetProperty)              (void);
   int               (* p_CachePushProperty)             (unsigned int oref, int plen, const Callin_char_t * pptr);
   int               (* p_CacheType)                     (void);
   int               (* p_CacheEvalA)                    (CACHE_ASTRP volatile expr);
   int               (* p_CacheExecuteA)                 (CACHE_ASTRP volatile cmd);
   int               (* p_CacheConvert)                  (unsigned long type, void * rbuf);
   int               (* p_CacheErrorA)                   (CACHE_ASTRP, CACHE_ASTRP, int *);
   int               (* p_CacheErrxlateA)                (int, CACHE_ASTRP);
   int               (* p_CacheEnableMultiThread)        (void);

} DBXISCSO, *PDBXISCSO;


typedef struct tagDBXYDBSO {
   short             loaded;
   char              libdir[256];
   char              libnam[256];
   char              funprfx[8];
   char              dbname[32];
   DBXPLIB           p_library;

   int               (* p_ydb_init)                      (void);
   int               (* p_ydb_exit)                      (void);
   int               (* p_ydb_malloc)                    (size_t size);
   int               (* p_ydb_free)                      (void *ptr);
   int               (* p_ydb_data_s)                    (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, unsigned int *ret_value);
   int               (* p_ydb_delete_s)                  (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int deltype);
   int               (* p_ydb_set_s)                     (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *value);
   int               (* p_ydb_get_s)                     (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_subscript_next_s)          (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_subscript_previous_s)      (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_node_next_s)               (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
   int               (* p_ydb_node_previous_s)           (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
   int               (* p_ydb_incr_s)                    (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *increment, ydb_buffer_t *ret_value);
   int               (* p_ydb_ci)                        (const char *c_rtn_name, ...);
   int               (* p_ydb_cip)                       (ci_name_descriptor *ci_info, ...);
   int               (* p_ydb_lock_incr_s)               (unsigned long long timeout_nsec, ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray);
   int               (* p_ydb_lock_decr_s)               (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray);

} DBXYDBSO, *PDBXYDBSO;


typedef struct tagDBXCON {
   short          dbtype;
   short          utf8;
   short          use_mutex;
   short          done;
   short          lock;
   short          increment;
   char           type[64];
   char           path[256];
   char           username[64];
   char           password[64];
   char           nspace[64];
   char           input_device[64];
   char           output_device[64];
   int            error_code;
   char           error[DBX_ERROR_SIZE];
   unsigned int   ibuffer_size;
   unsigned int   ibuffer_used;
   unsigned char *ibuffer;
   int            argc;
   int            cargc;
   DBXVAL         args[DBX_MAXARGS];
   ydb_buffer_t   yargs[DBX_MAXARGS];
   DBXVAL         output_val;
   DBXMUTEX       *p_mutex;
   DBXZV          *p_zv;
   DBXDEBUG       *p_debug;
   DBXISCSO       *p_isc_so;
   DBXYDBSO       *p_ydb_so;

   int            (* p_dbxfun) (struct tagDBXCON * pcon);

} DBXCON, *PDBXCON;


typedef struct tagDBXQR {
   ydb_buffer_t   global_name;
   unsigned int   kbuffer_size;
   unsigned int   kbuffer_used;
   unsigned char *kbuffer;
   int            keyn;
   ydb_buffer_t   key[DBX_MAXARGS];
   DBXVAL         data;
} DBXQR, *PDBXQR;


struct dbx_pool_task {
#if !defined(_WIN32)
   pthread_t   parent_tid;
#endif
   int         task_id;
   DBXCON      *pcon;
   struct dbx_pool_task *next;
};


class DBX_DBNAME : public node::ObjectWrap
{

private:

   unsigned char  csize;
   int            m_count;

public:

   int            counter;
   short          open;
   short          use_mutex;
   short          handle_sigint;
   short          handle_sigterm;
   int            utf8;
   unsigned long  pid;
   DBXMUTEX *     p_mutex;
   DBXCON *       pcon;
   DBXCON         con;
   DBXZV          zv;
   DBXDEBUG       debug;

   v8::Isolate             *isolate;
   v8::Local<v8::Context>  icontext;
   short                   got_icontext;

   static v8::Persistent<v8::Function> s_ct;

   struct dbx_baton_t {
      DBX_DBNAME *                  c;
      int                           increment_by;
      int                           sleep_for;
      v8::Local<v8::String>         result;
      v8::Persistent<v8::Function>  cb;
      DBXCON *                      pcon;
   };

#if DBX_NODE_VERSION >= 100000
   static void                   Init(v8::Local<v8::Object> target);
#else
   static void                   Init(v8::Handle<v8::Object> target);
#endif

   explicit                      DBX_DBNAME(int value = 0);
                                 ~DBX_DBNAME();
   static void                   New(const v8::FunctionCallbackInfo<v8::Value>& args);
#if DBX_NODE_VERSION >= 100000
   static void                   dbx_set_prototype_method(v8::Local<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#else
   static void                   dbx_set_prototype_method(v8::Handle<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#endif
   static v8::Local<v8::Object>  dbx_is_object(v8::Local<v8::Value> value, int *otype);
   static int                    dbx_string8_length(v8::Isolate * isolate, v8::Local<v8::String> str, int utf8);
   static v8::Local<v8::String>  dbx_new_string8(v8::Isolate * isolate, char * buffer, int utf8);
   static v8::Local<v8::String>  dbx_new_string8n(v8::Isolate * isolate, char * buffer, unsigned long len, int utf8);
   static int                    dbx_write_char8(v8::Isolate * isolate, v8::Local<v8::String> str, char * buffer, int utf8);
   static int                    dbx_ibuffer_add(DBXCON *pcon, v8::Isolate * isolate, int argn, v8::Local<v8::String> str, char * buffer, int buffer_len, short context);
   static dbx_baton_t *          dbx_make_baton(DBX_DBNAME *c, DBXCON *pcon);
   static int                    dbx_destroy_baton(dbx_baton_t *baton, DBXCON *pcon);
   static int                    dbx_queue_task(void * work_cb, void * after_work_cb, dbx_baton_t *baton, short context);
   static async_rtn              dbx_process_task(uv_work_t *req);
   static async_rtn              dbx_uv_close_callback(uv_work_t *req);
   static async_rtn              dbx_invoke_callback(uv_work_t *req);


   static void                   About(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Version(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Open(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Close(const v8::FunctionCallbackInfo<v8::Value>& args);
   static int                    GlobalReference(DBX_DBNAME *c, const v8::FunctionCallbackInfo<v8::Value>& args, DBXCON *pcon, DBXGREF *pgref, short context);
   static void                   Get(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Set(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Defined(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Delete(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Next(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Previous(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Increment(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Lock(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Unlock(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   Sleep(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   MGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   MGlobal_Close(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   MGlobalQuery(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void                   MGlobalQuery_Close(const v8::FunctionCallbackInfo<v8::Value>& args);
   static int                    ExtFunctionReference(DBX_DBNAME *c, const v8::FunctionCallbackInfo<v8::Value>& args, DBXCON *pcon, DBXFREF *pfref, DBXFUN *pfun, short context);
   static void                   ExtFunction(const v8::FunctionCallbackInfo<v8::Value>& args);

   static void                   Benchmark(const v8::FunctionCallbackInfo<v8::Value>& args);

};


int                     isc_load_library           (DBXCON *pcon);
int                     isc_authenticate           (DBXCON *pcon);
int                     isc_open                   (DBXCON *pcon);

int                     isc_parse_zv               (char *zv, DBXZV * p_sv);
int                     isc_change_namespace       (DBXCON *pcon, char *nspace);
int                     isc_cleanup                (DBXCON *pcon);
int                     isc_pop_value              (DBXCON *pcon, DBXVAL *value, int required_type);
char *                  isc_callin_message         (DBXCON *pcon, int error_code);
int                     isc_error_message          (DBXCON *pcon, int error_code);
int                     isc_error                  (DBXCON *pcon, int error_code);


int                     ydb_load_library           (DBXCON *pcon);
int                     ydb_open                   (DBXCON *pcon);
int                     ydb_parse_zv               (char *zv, DBXZV * p_isc_sv);
int                     ydb_error_message          (DBXCON *pcon, int error_code);
int                     ydb_function               (DBXCON *pcon, DBXFUN *pfun);

int                     dbx_version                (DBXCON *pcon);
int                     dbx_open                   (DBXCON *pcon);
int                     dbx_do_nothing             (DBXCON *pcon);
int                     dbx_close                  (DBXCON *pcon);
int                     dbx_reference              (DBXCON *pcon, int n);
int                     dbx_global_reference       (DBXCON *pcon);

int                     dbx_get                    (DBXCON *pcon);
int                     dbx_set                    (DBXCON *pcon);
int                     dbx_defined                (DBXCON *pcon);
int                     dbx_delete                 (DBXCON *pcon);
int                     dbx_next                   (DBXCON *pcon);
int                     dbx_previous               (DBXCON *pcon);
int                     dbx_increment              (DBXCON *pcon);
int                     dbx_lock                   (DBXCON *pcon);
int                     dbx_unlock                 (DBXCON *pcon);
int                     dbx_function_reference     (DBXCON *pcon, DBXFUN *pfun);
int                     dbx_function               (DBXCON *pcon);
int                     dbx_global_order           (DBXCON *pcon, DBXQR *pqr_prev, short dir, short getdata);
int                     dbx_global_query           (DBXCON *pcon, DBXQR *pqr_next, DBXQR *pqr_prev, short dir, short getdata);
int                     dbx_parse_global_reference (DBXCON *pcon, DBXQR *pqr, char *global_ref, int global_ref_len);

int                     dbx_launch_thread          (DBXCON *pcon);
#if defined(_WIN32)
LPTHREAD_START_ROUTINE  dbx_thread_main            (LPVOID pargs);
#else
void *                  dbx_thread_main            (void *pargs);
#endif

struct dbx_pool_task *  dbx_pool_add_task          (DBXCON *pcon);
struct dbx_pool_task *  dbx_pool_get_task          (void);
void                    dbx_pool_execute_task      (struct dbx_pool_task *task, int thread_id);
void *                  dbx_pool_requests_loop     (void *data);
int                     dbx_pool_thread_init       (DBXCON *pcon, int num_threads);
int                     dbx_pool_submit_task       (DBXCON *pcon);
void *                  dbx_realloc                (void *p, int curr_size, int new_size, short id);
void *                  dbx_malloc                 (int size, short id);
int                     dbx_free                   (void *p, short id);
DBXQR *                 dbx_alloc_dbxqr            (DBXQR *pqr, int dsize, short context);
int                     dbx_free_dbxqr             (DBXQR *pqr);
int                     dbx_ucase                  (char *string);
int                     dbx_lcase                  (char *string);

int                     dbx_create_string          (DBXSTR *pstr, void *data, short type);

int                     dbx_buffer_dump            (DBXCON *pcon, void *buffer, unsigned int len, char *title, unsigned char csize, short mode);
int                     dbx_log_event              (DBXDEBUG *p_debug, char *message, char *title, int level);
int                     dbx_sleep                  (int msecs);
int                     dbx_test_file_access       (char *file, int mode);
DBXPLIB                 dbx_dso_load               (char * library);
DBXPROC                 dbx_dso_sym                (DBXPLIB p_library, char * symbol);
int                     dbx_dso_unload             (DBXPLIB p_library);
DBXTHID                 dbx_current_thread_id      (void);
unsigned long           dbx_current_process_id     (void);
int                     dbx_error_message          (DBXCON *pcon, int error_code);

int                     dbx_mutex_create           (DBXMUTEX *p_mutex);
int                     dbx_mutex_lock             (DBXMUTEX *p_mutex, int timeout);
int                     dbx_mutex_unlock           (DBXMUTEX *p_mutex);
int                     dbx_mutex_destroy          (DBXMUTEX *p_mutex);
int                     dbx_enter_critical_section (void *p_crit);
int                     dbx_leave_critical_section (void *p_crit);
int                     dbx_sleep                  (unsigned long msecs);

int                     dbx_fopen                  (FILE **pfp, const char *file, const char *mode);
int                     dbx_strcpy_s               (char *to, size_t size, const char *from, const char *file, const char *fun, const unsigned int line);
int                     dbx_strncpy_s              (char *to, size_t size, const char *from, size_t count, const char *file, const char *fun, const unsigned int line);
int                     dbx_strcat_s               (char *to, size_t size, const char *from, const char *file, const char *fun, const unsigned int line);
int                     dbx_strncat_s              (char *to, size_t size, const char *from, size_t count, const char *file, const char *fun, const unsigned int line);
int                     dbx_sprintf_               (char *to, size_t size, const char *format, const char *file, const char *fun, const unsigned int line, ...);

#endif

