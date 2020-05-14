/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2020 M/Gateway Developments Ltd,                      |
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

/*

Change Log:

Version 1.0.3 28 June 2019:
   First release.

Version 1.0.4 7 September 2019:
   Performance improvements.
   Allow partial (constant key) to be defined in the mglobal() method.
      myglobal = db.mglobal("myglobal", "str");
      myglobal.set(1,"data");
      -> ^myglobal("str",1)="data"
      And: myglobal.reset("myglobal", "new str");

Version 1.1.5 4 October 2019:
   Performance improvements.
   New cursor based retrieval operations:
      query = db.mglobalquery({global: <global_name>, key: [<partial_key>]}, {multilevel: true|false, getdata: true|false, format: "url"});
      while ((key = query.next()) !== null) {}

Version 1.2.6 10 October 2019:
   Introduce support for direct access to InterSystems IRIS/Cache classes (db.classmethod() et al).
   Extend cursor based data retrieval to include an option for generating a global directory listing (globaldirectory: true).
   Introduce a method to report and (optionally change) the current working global directory (or Namespace) (db.namespace()).
   Correct a fault that led to the timeout occasionally not being honoured in the **lock()** method.
   Correct a fault that led to Node.js exceptions not being processed correctly.

Version 1.3.7 1 November 2019:
   Introduce support for direct access to InterSystems SQL and MGSQL.
   Correct a fault in the InterSystems Cache/IRIS API to globals that resulted in failures - notably in cases where there was a mix of string and numeric data in the global records.

Version 1.3.8 14 November 2019:
   Correct a fault in the Global Increment method.
   Correct a fault that resulted in query.next() and query.previous() loops not terminating properly (with null) under YottaDB.
   - This fault affected YottaDB releases after 1.22
   Modify the version() method so that it returns the version of YottaDB rather than the version of the underlying GT.M engine.

Version 1.3.9 26 February 2020:
   Verify that mg-dbx will build and work with Node.js v13.x.x
   Suppress a number of benign 'cast-function-type' compiler warnings when building on the Raspberry Pi.

Version 1.3.9a 21 April 2020:
   Verify that mg-dbx will build and work with Node.js v14.x.x.

Version 1.4.10 6 May 2020:
   Introduce support for Node.js/V8 worker threads (for Node.js v12.x.x. and later).
   Introduce support for the M Merge command.
   Correct a fault in the Cursor Reset method.

Version 1.4.11 14 May 2020:
   Introduce a scheme for transmitting binary data between Node.js and the database.
   Correct a fault that led to some calls failing with incorrect data types after calls to the mglobal::increment method.
   Pass arguments to YottaDB functions as ydb_string_t types and not ydb_char_t.

*/

#include "mg-dbx.h"
#include "mg-global.h"
#include "mg-cursor.h"
#include "mg-class.h"

#if !defined(_WIN32)
extern int errno;
#endif

static int     dbx_counter             = 0;
static int     dbx_sql_counter         = 0;
int            dbx_total_tasks         = 0;
int            dbx_request_errors      = 0;

#if defined(_WIN32)
CRITICAL_SECTION  dbx_async_mutex;
#else
pthread_mutex_t   dbx_async_mutex        = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t   dbx_pool_mutex         = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t   dbx_result_mutex       = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t   dbx_task_queue_mutex   = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t    dbx_pool_cond           = PTHREAD_COND_INITIALIZER;
pthread_cond_t    dbx_result_cond         = PTHREAD_COND_INITIALIZER;

DBXTID            dbx_thr_id[DBX_THREADPOOL_MAX];
pthread_t         dbx_p_threads[DBX_THREADPOOL_MAX];
#endif

struct dbx_pool_task * tasks           = NULL;
struct dbx_pool_task * bottom_task     = NULL;

DBXCON * pcon_api = NULL;

DBXISCSO *  p_isc_so_global;
DBXYDBSO *  p_ydb_so_global;
DBXMUTEX    mutex_global;

using namespace node;
using namespace v8;


static void dbxGoingDownSignal(int sig)
{
   /* printf("\r\nNode.js process interrupted (signal=%d)", sig); */
   exit(0);
}

#if defined(_WIN32) && defined(DBX_USE_SECURE_CRT)
/* Compile with: /Zi /MTd */
void dbxInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
{
   printf("\r\nWindows Secure Function Error\r\nInvalid parameter detected in function at line %d\r\n", line);
   /* abort(); */
   return;
}
#endif


#if defined(_WIN32)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
#if defined(_WIN32) && defined(DBX_USE_SECURE_CRT)
   _invalid_parameter_handler oldHandler, newHandler;
#endif

   switch (fdwReason)
   { 
      case DLL_PROCESS_ATTACH:
#if defined(_WIN32) && defined(DBX_USE_SECURE_CRT)
         newHandler = dbxInvalidParameterHandler;
         oldHandler = _set_invalid_parameter_handler(newHandler);
#endif
         InitializeCriticalSection(&dbx_async_mutex);
         break;
      case DLL_THREAD_ATTACH:
         break;
      case DLL_THREAD_DETACH:
         break;
      case DLL_PROCESS_DETACH:
         DeleteCriticalSection(&dbx_async_mutex);
         break;
   }
   return TRUE;
}
#endif

/* Start of DBX class methods */
#if DBX_NODE_VERSION >= 100000
void DBX_DBNAME::Init(Local<Object> exports)
#else
void DBX_DBNAME::Init(Handle<Object> exports)
#endif
{

#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = exports->GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, DBX_DBNAME_STR, NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* v1.4.10 */
#else
   Isolate* isolate = Isolate::GetCurrent();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
   tpl->SetClassName(String::NewFromUtf8(isolate, DBX_DBNAME_STR));
#endif

   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "version", Version);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "charset", Charset);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "open", Open);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "namespace", Namespace);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "get", Get);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "get_bx", Get_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "set", Set);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "defined", Defined);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "delete", Delete);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "next", Next);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "previous", Previous);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "increment", Increment);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "lock", Lock);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "unlock", Unlock);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "mglobal", MGlobal);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "mglobal_close", MGlobal_Close);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "mglobalquery", MGlobalQuery);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "mglobalquery_close", MGlobalQuery_Close);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "function", ExtFunction);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "function_bx", ExtFunction_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "classmethod", ClassMethod);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "classmethod_bx", ClassMethod_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "classmethod_close", ClassMethod_Close);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "sql", SQL);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "sql_close", SQL_Close);

   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "sleep", Sleep);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "benchmark", Benchmark);

#if DBX_NODE_VERSION >= 120000
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
   exports->Set(icontext, String::NewFromUtf8(isolate, DBX_DBNAME_STR, NewStringType::kNormal).ToLocalChecked(), tpl->GetFunction(icontext).ToLocalChecked()).FromJust();
#else
   constructor.Reset(isolate, tpl->GetFunction());
   exports->Set(String::NewFromUtf8(isolate, DBX_DBNAME_STR), tpl->GetFunction());
#endif

}


DBX_DBNAME::DBX_DBNAME(int value) : dbx_count(value)
{
}

DBX_DBNAME::~DBX_DBNAME()
{
}


void DBX_DBNAME::New(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
   HandleScope scope(isolate);
   DBX_DBNAME *c = new DBX_DBNAME();
   c->Wrap(args.This());

   /* V8 will set SetAlignedPointerInInternalField */
   /* args.This()->SetAlignedPointerInInternalField(0, c); */
   args.This()->SetInternalField(2, DBX_INTEGER_NEW(DBX_MAGIC_NUMBER));

   dbx_enter_critical_section((void *) &dbx_async_mutex);
   if (dbx_counter == 0) {
      mutex_global.created = 0;
      dbx_mutex_create(&(mutex_global));
   }
   c->counter = dbx_counter;
   dbx_counter ++;
   dbx_leave_critical_section((void *) &dbx_async_mutex);

   c->open = 0;

   c->csize = 8;

   c->pcon = NULL;

   c->use_mutex = 0;
   c->p_mutex = &mutex_global;
   c->handle_sigint = 0;
   c->handle_sigterm = 0;

   c->isolate = NULL;
   c->got_icontext = 0;

   c->pcon = (DBXCON *) dbx_malloc(sizeof(DBXCON), 0);
   memset((void *) c->pcon, 0, sizeof(DBXCON));

   c->pcon->p_mutex = &mutex_global;
   c->pcon->p_zv = NULL;

   c->pcon->output_val.svalue.buf_addr = (char *) dbx_malloc(32000, 0);
   memset((void *) c->pcon->output_val.svalue.buf_addr, 0, 32000);
   c->pcon->output_val.svalue.len_alloc = 32000;
   c->pcon->output_val.svalue.len_used = 0;

   c->pcon->ibuffer = (unsigned char *) dbx_malloc(CACHE_MAXSTRLEN + DBX_IBUFFER_OFFSET, 0);
   memset((void *) c->pcon->ibuffer, 0, DBX_IBUFFER_OFFSET);
   dbx_add_block_size(c->pcon->ibuffer + 5, 0, CACHE_MAXSTRLEN, 0, 0);
   c->pcon->ibuffer += DBX_IBUFFER_OFFSET; /* v1.4.10 */
   c->pcon->ibuffer_used = 0;
   c->pcon->ibuffer_size = CACHE_MAXSTRLEN;

   c->pcon->utf8 = 1; /* seems to be faster with UTF8 on! */
   c->pcon->use_mutex = 0;
   c->pcon->binary = 0;
   c->pcon->lock = 0;
   c->pcon->increment = 0;

   c->pcon->psql = NULL;
   c->pcon->p_isc_so = NULL;
   c->pcon->p_ydb_so = NULL;

   args.GetReturnValue().Set(args.This());

   return;

}


DBX_DBNAME::dbx_baton_t * DBX_DBNAME::dbx_make_baton(DBX_DBNAME *c, DBXCON *pcon)
{
   dbx_baton_t *baton;

   baton = new dbx_baton_t();

   if (!baton) {
      return NULL;
   }
   baton->c = c;
   baton->pcon = pcon;
   return baton;
}


int DBX_DBNAME::dbx_destroy_baton(dbx_baton_t *baton, DBXCON *pcon)
{

   if (baton) {
      delete baton;
   }

   return 0;
}


int DBX_DBNAME::dbx_queue_task(void * work_cb, void * after_work_cb, DBX_DBNAME::dbx_baton_t *baton, short context)
{
   uv_work_t *_req = new uv_work_t;
   _req->data = baton;
   uv_queue_work(uv_default_loop(), _req, (uv_work_cb) work_cb, (uv_after_work_cb) after_work_cb);

   return 0;
}

/* ASYNC THREAD */
async_rtn DBX_DBNAME::dbx_process_task(uv_work_t *req)
{
   DBX_DBNAME::dbx_baton_t *baton = static_cast<DBX_DBNAME::dbx_baton_t *>(req->data);

   dbx_launch_thread(baton->pcon);

   baton->c->dbx_count += 1;

   return;
}


/* PRIMARY THREAD : Standard callback wrapper */
async_rtn DBX_DBNAME::dbx_uv_close_callback(uv_work_t *req)
{
/*
   printf("\r\n*** %lu cache_uv_close_callback() (%d, req=%p) ...", (unsigned long) dbx_current_thread_id(), dbx_queue_size, req);
*/
   delete req;

   return;
}


async_rtn DBX_DBNAME::dbx_invoke_callback(uv_work_t *req)
{
   Isolate* isolate = Isolate::GetCurrent();
   HandleScope scope(isolate);

   dbx_baton_t *baton = static_cast<dbx_baton_t *>(req->data);

   baton->c->Unref();

   Local<Value> argv[2];

   if (baton->pcon->error[0])
      argv[0] = DBX_INTEGER_NEW(true);
   else
      argv[0] = DBX_INTEGER_NEW(false);

   if (baton->c->pcon->binary) {
      baton->result_obj = node::Buffer::New(isolate, (char *) baton->c->pcon->output_val.svalue.buf_addr, (size_t) baton->c->pcon->output_val.svalue.len_used).ToLocalChecked();
      argv[1] = baton->result_obj;
   }
   else {
      baton->result_str = dbx_new_string8n(isolate, baton->pcon->output_val.svalue.buf_addr, baton->pcon->output_val.svalue.len_used, baton->c->pcon->utf8);
      argv[1] = baton->result_str;
   }

#if DBX_NODE_VERSION < 80000
   TryCatch try_catch;
#endif

   Local<Function> cb = Local<Function>::New(isolate, baton->cb);

#if DBX_NODE_VERSION >= 120000
   /* cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv); */
   cb->Call(isolate->GetCurrentContext(), Null(isolate), 2, argv).ToLocalChecked();
#else
   cb->Call(isolate->GetCurrentContext()->Global(), 2, argv);
#endif

#if DBX_NODE_VERSION < 80000
   if (try_catch.HasCaught()) {
      node::FatalException(isolate, try_catch);
   }
#endif

   baton->cb.Reset();

	DBX_DBFUN_END(baton->c);

   dbx_destroy_baton(baton, baton->pcon);

   delete req;
   return;
}


async_rtn DBX_DBNAME::dbx_invoke_callback_sql_execute(uv_work_t *req)
{
   Isolate* isolate = Isolate::GetCurrent();
#if DBX_NODE_VERSION >= 100000
   Local<Context> icontext = isolate->GetCurrentContext();
#endif
   HandleScope scope(isolate);
   Local<String> key;

   dbx_baton_t *baton = static_cast<dbx_baton_t *>(req->data);

   baton->c->Unref();

   Local<Value> argv[2];

   if (baton->pcon->error[0])
      argv[0] = DBX_INTEGER_NEW(true);
   else
      argv[0] = DBX_INTEGER_NEW(false);

   baton->result_obj = DBX_OBJECT_NEW();

   key = dbx_new_string8(isolate, (char *) "sqlcode", 0);
   DBX_SET(baton->result_obj, key, DBX_INTEGER_NEW(baton->pcon->psql->sqlcode));
   key = dbx_new_string8(isolate, (char *) "sqlstate", 0);
   DBX_SET(baton->result_obj, key, dbx_new_string8(isolate, baton->pcon->psql->sqlstate, 0));
   if (baton->pcon->error[0]) {
      key = dbx_new_string8(isolate, (char *) "error", 0);
      DBX_SET(baton->result_obj, key, dbx_new_string8(isolate, baton->pcon->error, 0));
   }

   argv[1] =  baton->result_obj;

#if DBX_NODE_VERSION < 80000
   TryCatch try_catch;
#endif

   Local<Function> cb = Local<Function>::New(isolate, baton->cb);

#if DBX_NODE_VERSION >= 120000
   /* cb->Call(isolate->GetCurrentContext(), isolate->GetCurrentContext()->Global(), 2, argv); */
   cb->Call(isolate->GetCurrentContext(), Null(isolate), 2, argv).ToLocalChecked();
#else
   cb->Call(isolate->GetCurrentContext()->Global(), 2, argv);
#endif

#if DBX_NODE_VERSION < 80000
   if (try_catch.HasCaught()) {
      node::FatalException(isolate, try_catch);
   }
#endif

   baton->cb.Reset();

	DBX_DBFUN_END(baton->c);

   dbx_destroy_baton(baton, baton->pcon);

   delete req;
   return;
}


void DBX_DBNAME::Version(const FunctionCallbackInfo<Value>& args)
{
   int js_narg;
   DBXCON *pcon;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;

   js_narg = args.Length();

   if (js_narg >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Version", 1)));
      return;
   }

   dbx_version(pcon);

   Local<String> result;
   result = dbx_new_string8(isolate, (char *) pcon->output_val.svalue.buf_addr, pcon->utf8);

   args.GetReturnValue().Set(result);
}


/* 1.4.11 */
void DBX_DBNAME::Charset(const FunctionCallbackInfo<Value>& args)
{
   int js_narg, str_len;
   char buffer[32];
   DBXCON *pcon;
   Local<String> str;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   js_narg = args.Length();

   if (js_narg == 0) {
      if (pcon->utf8 == 0) {
         strcpy(buffer, "ascii");
      }
      else {
         strcpy(buffer, "utf-8");
      }
      result = dbx_new_string8(isolate, (char *) buffer, pcon->utf8);
      args.GetReturnValue().Set(result);
      return;
   }

   result = dbx_new_string8(isolate, (char *) buffer, pcon->utf8);
   args.GetReturnValue().Set(result);

   if (js_narg != 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Charset method takes one argument", 1)));
      return;
   }
   str = DBX_TO_STRING(args[0]);
   str_len = dbx_string8_length(isolate, str, 0);
   if (str_len > 30) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Invalid 'character set' argument supplied to the SetCharset method", 1)));
      return;
   }

   DBX_WRITE_UTF8(str, buffer);
   dbx_lcase(buffer);

   if (strstr(buffer, "ansi") || strstr(buffer, "ascii") || strstr(buffer, "8859") || strstr(buffer, "1252")) {
      pcon->utf8 = 0;
      strcpy(buffer, "ascii");
   }
   else if (strstr(buffer, "utf8") || strstr(buffer, "utf-8")) {
      pcon->utf8 = 1;
      strcpy(buffer, "utf-8");
   }
   else {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Invalid 'character set' argument supplied to the SetCharset method", 1)));
      return;
   }

   result = dbx_new_string8(isolate, (char *) buffer, pcon->utf8);
   args.GetReturnValue().Set(result);
   return;
}


void DBX_DBNAME::Open(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n, js_narg, error_code, key_len;
   char name[256];
   DBXCON *pcon;
#if !defined(_WIN32)
   struct sigaction action;
#endif
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;

   DBX_CALLBACK_FUN(js_narg, cb, async);

   if (js_narg >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Open", 1)));
      return;
   }

   if (c->open) {
      Local<String> result = dbx_new_string8(isolate, (char *) "", 0);
      args.GetReturnValue().Set(result);
   }

   Local<Object> obj, objn;
   Local<String> key;
   Local<String> value;

   obj = DBX_TO_OBJECT(args[0]);

#if DBX_NODE_VERSION >= 120000
   Local<Array> a = obj->GetPropertyNames(icontext).ToLocalChecked();
#else
   Local<Array> a = obj->GetPropertyNames();
#endif

   error_code = 0;
   for (n = 0; n < (int) a->Length(); n ++) {
      key = DBX_TO_STRING(DBX_GET(a, n));

      key_len = dbx_string8_length(isolate, key, 0);
      if (key_len > 60) {
         error_code = 1;
         break;
      }
      DBX_WRITE_UTF8(key, (char *) name);

      if (!strcmp(name, (char *) "type")) {
         value = DBX_TO_STRING(DBX_GET(obj ,key));
         DBX_WRITE_UTF8(value, pcon->type);
         dbx_lcase(pcon->type);

         if (!strcmp(pcon->type, "cache"))
            pcon->dbtype = DBX_DBTYPE_CACHE;
         else if (!strcmp(pcon->type, "iris"))
            pcon->dbtype = DBX_DBTYPE_IRIS;
         else if (!strcmp(pcon->type, "yottadb"))
            pcon->dbtype = DBX_DBTYPE_YOTTADB;
      }
      else if (!strcmp(name, (char *) "path")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, pcon->path);
      }
      else if (!strcmp(name, (char *) "username")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, pcon->username);
      }
      else if (!strcmp(name, (char *) "password")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, pcon->password);
      }
      else if (!strcmp(name, (char *) "namespace")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
       DBX_WRITE_UTF8(value, pcon->nspace);
      }
      else if (!strcmp(name, (char *) "input_device")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, pcon->input_device);
      }
      else if (!strcmp(name, (char *) "output_device")) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, pcon->output_device);
      }
      else if (!strcmp(name, (char *) "env_vars")) {
         char *p, *p1, *p2;
         value = DBX_TO_STRING(DBX_GET(obj, key));
         DBX_WRITE_UTF8(value, (char *) c->pcon->ibuffer);

         p = (char *) c->pcon->ibuffer;
         p2 = p;
         while ((p2 = strstr(p, "\n"))) {
            *p2 = '\0';
            p1 = strstr(p, "=");
            if (p1) {
               *p1 = '\0';
               p1 ++;
#if defined(_WIN32)
               SetEnvironmentVariable((LPCTSTR) p, (LPCTSTR) p1);
#else
               /* printf("\nLinux : environment variable p=%s p1=%s;", p, p1); */
               setenv(p, p1, 1);
#endif
            }
            else {
               break;
            }
            p = p2 + 1;
         }
      }
      else if (!strcmp(name, (char *) "multithreaded")) {
        if (DBX_GET(obj, key)->IsBoolean()) {
            if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
               c->use_mutex = 1;
               c->pcon->use_mutex = 1;
            }
         }
      }
      else if (!strcmp(name, (char *) "debug")) {
         ; /* TODO */
      }
      else {
         error_code = 2;
         break;
      }
   }

   if (error_code == 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Oversize parameter supplied to the Open method", 1)));
      return;
   }
   else if (error_code == 2) {
      strcat(name, " - Invalid parameter name in the Open method");
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) name, 1)));
      return;
   }

#if defined(_WIN32)

   if (c->handle_sigint) {
      signal(SIGINT, dbxGoingDownSignal);
      /* printf("\r\nSet Signal Handler for SIGINT"); */
   }
   if (c->handle_sigterm) {
      signal(SIGTERM, dbxGoingDownSignal);
      /* printf("\r\nSet Signal Handler for SIGTERM"); */
   }

#else

   action.sa_flags = 0;
   sigemptyset(&(action.sa_mask));
   if (c->handle_sigint) {
      action.sa_handler = dbxGoingDownSignal;
      sigaction(SIGTERM, &action, NULL);
      /* printf("\r\nSet Signal Handler for SIGINT"); */
   }
   if (c->handle_sigterm) {
      action.sa_handler = dbxGoingDownSignal;
      sigaction(SIGINT, &action, NULL);
      /* printf("\r\nSet Signal Handler for SIGTERM"); */
   }

#endif

    if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_do_nothing;

      rc = dbx_open(pcon);
      if (rc == CACHE_SUCCESS) {
         c->open = 1;
      }

      Local<Function> cb = Local<Function>::Cast(args[js_narg]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }

      return;
   }

   rc = dbx_open(pcon);

   if (rc == CACHE_SUCCESS) {
      c->open = 1; 
   }

   Local<String> result = dbx_new_string8(isolate, pcon->error, 0);
   args.GetReturnValue().Set(result);
 }


void DBX_DBNAME::Close(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int js_narg;
   DBXCON *pcon;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;

   DBX_DBFUN_START(c, pcon);

   c->open = 0;

   DBX_CALLBACK_FUN(js_narg, cb, async);

   pcon->error[0] = '\0';
   if (pcon && pcon->error[0]) {
      goto Close_Exit;
   }

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_do_nothing;

      dbx_close(baton->pcon);

      Local<Function> cb = Local<Function>::Cast(args[js_narg]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }

      return;
   }

   dbx_close(pcon);

Close_Exit:

   Local<String> result = dbx_new_string8(isolate, pcon->error, 0);

   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Namespace(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on NameSpace", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   if (pcon->argc > 0) {
      result = DBX_TO_STRING(args[0]);
      dbx_ibuffer_add(pcon, isolate, 0, result, NULL, 0, 0);
   }

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_namespace;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = dbx_namespace(pcon);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}



int DBX_DBNAME::GlobalReference(DBX_DBNAME *c, const FunctionCallbackInfo<Value>& args, DBXCON *pcon, DBXGREF *pgref, short context)
{
   int n, nx, rc, otype, len;
   char *p;
   char buffer[64];
   DBXVAL *pval;
   Local<Object> obj;
   Local<String> str;
   DBX_GET_ICONTEXT;

   pcon->ibuffer_used = 0;
   pcon->cargc = 0;
   rc = 0;

   if (!context) {
      DBX_DB_LOCK(rc, 0);
   }

   pcon->output_val.svalue.len_used = 0;
   nx = 0;
   n = 0;
   pcon->args[nx].cvalue.pstr = 0;
   if (pgref) {
      dbx_ibuffer_add(pcon, isolate, nx, str, pgref->global, (int) strlen(pgref->global), 0);
   }
   else {
      str = DBX_TO_STRING(args[n]);
      dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 0);
      n ++;
   }

   if (pcon->dbtype != DBX_DBTYPE_YOTTADB && context == 0) {
      if (pcon->lock) {
         rc = pcon->p_isc_so->p_CachePushLock((int) pcon->args[nx].svalue.len_used, (Callin_char_t *) pcon->args[nx].svalue.buf_addr);
      }
      else {
         if (pcon->args[nx].svalue.buf_addr[nx] == '^')
            rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[nx].svalue.len_used - 1, (Callin_char_t *) pcon->args[nx].svalue.buf_addr + 1);
         else
            rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[nx].svalue.len_used, (Callin_char_t *) pcon->args[nx].svalue.buf_addr);
      }
   }

   nx ++;

   if (pgref && (pval = pgref->pkey)) {
      while (pval) {
         pcon->args[nx].cvalue.pstr = 0;
         if (pval->type == DBX_TYPE_INT) {
            pcon->args[nx].type = DBX_TYPE_INT;
            pcon->args[nx].num.int32 = (int) pval->num.int32;
            T_SPRINTF(buffer, _dbxso(buffer), "%d", pval->num.int32);
            dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 0);
         }
         else {
            dbx_ibuffer_add(pcon, isolate, nx, str, pval->svalue.buf_addr, (int) pval->svalue.len_used, 0);
         }
/*
{
   char buffer[256];
   strcpy(buffer, "");
   if (pval->svalue.len_used > 0) {
      strncpy(buffer,  pval->svalue.buf_addr,  pval->svalue.len_used);
      buffer[ pval->svalue.len_used] = '\0';
   }
   printf("\r\n pval->type=%d; pval->num.int32=%d; pval->svalue.buf_addr=%s", pval->type, pval->num.int32, buffer);
}
*/

         if (pcon->dbtype != DBX_DBTYPE_YOTTADB && context == 0) {
            rc = dbx_reference(pcon, nx);
         }
         nx ++;
         pval = pval->pnext;
      }
   }

   for (; n < pcon->argc; n ++, nx ++) {

      pcon->args[nx].cvalue.pstr = 0;

      if (args[n]->IsInt32()) {
         pcon->args[nx].num.int32 = (int) DBX_INT32_VALUE(args[n]);
         T_SPRINTF(buffer, _dbxso(buffer), "%d", pcon->args[nx].num.int32);
         dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 0);
         pcon->args[nx].type = DBX_TYPE_INT;
      }
      else {
         pcon->args[nx].type = DBX_TYPE_STR;
         obj = dbx_is_object(args[n], &otype);

         if (otype == 2) {
            p = node::Buffer::Data(obj);
            len = (int) node::Buffer::Length(obj);
            dbx_ibuffer_add(pcon, isolate, nx, str, p, (int) len, 0);
         }
         else {
            str = DBX_TO_STRING(args[n]);
            dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 0);
         }
      }

      if (pcon->increment && n == (pcon->argc - 1)) { /* v1.3.8 */
         if (pcon->args[nx].svalue.len_used < 32) {
            T_STRNCPY(buffer, _dbxso(buffer), pcon->args[nx].svalue.buf_addr, pcon->args[nx].svalue.len_used);
         }
         else {
            buffer[0] = '1';
            buffer[1] = '\0';
         }
         pcon->args[nx].type = DBX_TYPE_DOUBLE;
         pcon->args[nx].num.real = (double) strtod(buffer, NULL);
      }

      if (pcon->dbtype != DBX_DBTYPE_YOTTADB && context == 0) {

         if (pcon->lock == 1) {
            if (n < (pcon->argc - 1)) { /* don't push lock timeout */
               rc = dbx_reference(pcon, nx);
            }
         }
         else {
            rc = dbx_reference(pcon, nx);
         }
      }
   }

   pcon->cargc = nx;

   return rc;
}


void DBX_DBNAME::Get(const FunctionCallbackInfo<Value>& args)
{
   return GetEx(args, 0);
}


void DBX_DBNAME::Get_bx(const FunctionCallbackInfo<Value>& args)
{
   return GetEx(args, 1);
}


void DBX_DBNAME::GetEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = binary;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Get", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Get", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_get;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_get_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalGet(pcon->cargc - 1, 0);
   }

   if (rc == CACHE_ERUNDEF) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) "", DBX_TYPE_STR8);
   }
   else if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (binary) {
      Local<Object> bx = node::Buffer::New(isolate, (char *) pcon->output_val.svalue.buf_addr, (size_t) pcon->output_val.svalue.len_used).ToLocalChecked();
      args.GetReturnValue().Set(bx);
   }
   else {
      result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
      args.GetReturnValue().Set(result);
   }
}


void DBX_DBNAME::Set(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Set", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Set", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_set;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_set_s(&(pcon->args[0].svalue), pcon->cargc - 2, &pcon->yargs[0], &(pcon->args[pcon->cargc - 1].svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalSet(pcon->cargc - 2);
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Defined(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Defined", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Defined", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_defined;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_data_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0], (unsigned int *) &n);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalData(pcon->cargc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->p_zv->dbtype != DBX_DBTYPE_YOTTADB) {
         pcon->p_isc_so->p_CachePopInt(&n);
      }
      dbx_create_string(&(pcon->output_val.svalue), (void *) &n, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Delete(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Delete", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Delete", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_delete_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0], YDB_DEL_TREE);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalKill(pcon->cargc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      n = 0;
      dbx_create_string(&(pcon->output_val.svalue), (void *) &n, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Next", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->cargc - 1, 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }


   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Previous", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->cargc - 1, -1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }


   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);

   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Increment(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Increment", 1)));
      return;
   }
   if (pcon->argc < 2) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Increment", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 1;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_increment;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_incr_s(&(pcon->args[0].svalue), pcon->cargc - 2, &pcon->yargs[0], &(pcon->args[pcon->cargc - 1].svalue), &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalIncrement(pcon->cargc - 2);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Lock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval, timeout, locktype;
   unsigned long long timeout_nsec;
   char buffer[32];
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Lock", 1)));
      return;
   }
   if (pcon->argc < 2) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Lock", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 1;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_lock;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      timeout = -1;
      if (pcon->args[pcon->cargc - 1].svalue.len_used < 16) {
         strncpy(buffer, pcon->args[pcon->cargc - 1].svalue.buf_addr, pcon->args[pcon->cargc - 1].svalue.len_used);
         buffer[pcon->args[pcon->cargc - 1].svalue.len_used] = '\0';
         timeout = (int) strtol(buffer, NULL, 10);
      }
      timeout_nsec = 1000000000;

      if (timeout < 0)
         timeout_nsec *= 3600;
      else
         timeout_nsec *= timeout;
      rc = pcon->p_ydb_so->p_ydb_lock_incr_s(timeout_nsec, &(pcon->args[0].svalue), pcon->cargc - 2, &pcon->yargs[0]);
      if (rc == YDB_OK) {
         retval = 1;
         rc = YDB_OK;
      }
      else if (rc == YDB_LOCK_TIMEOUT) {
         retval = 0;
         rc = YDB_OK;
      }
      else {
         retval = 0;
         rc = CACHE_FAILURE;
      }
   }
   else {
      strncpy(buffer, pcon->args[pcon->cargc - 1].svalue.buf_addr, pcon->args[pcon->cargc - 1].svalue.len_used);
      buffer[pcon->args[pcon->cargc - 1].svalue.len_used] = '\0';
      timeout = (int) strtol(buffer, NULL, 10);
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheAcquireLock(pcon->cargc - 2, locktype, timeout, &retval);
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &retval, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Unlock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval;
   DBXCON *pcon;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Unlock", 1)));
      return;
   }
   if (pcon->argc < 2) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Unlock", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 2;
   pcon->increment = 0;
   rc = GlobalReference(c, args, pcon, NULL, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_unlock;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      c->Ref();
      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_lock_decr_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0]);
      if (rc == YDB_OK)
         retval = 1;
      else
         retval = 0;
   }
   else {
      int locktype;
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheReleaseLock(pcon->cargc - 1, locktype);
      if (rc == CACHE_SUCCESS)
         retval = 1;
      else
         retval = 0;
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &retval, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::MGlobal(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->argc = args.Length();

   /* 1.4.10 */

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobal method takes at least one argument (the global name)", 1)));
      return;
   }

   mglobal *gx = mglobal::NewInstance(args);

   gx->c = c;
   gx->pkey = NULL;

   /* 1.4.10 */
   rc = dbx_global_reset(args, isolate, (void *) gx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobal method takes at least one argument (the global name)", 1)));
      return;
   }

/*
{
   int n;
   char buffer[256];

   printf("\r\n mglobal START");
   n = 0;
   pval = gx->pkey;
   while (pval) {
      strcpy(buffer, "");
      if (pcon->args[n].svalue.len_used > 0) {
         strncpy(buffer,   pval->svalue.buf_addr,   pval->svalue.len_used);
         buffer[ pval->svalue.len_used] = '\0';
      }
      printf("\r\n >>>>>>> n=%d; type=%d; int32=%d; str=%s", ++ n, pval->type, pval->num.int32, buffer);
      pval = pval->pnext;
   }
    printf("\r\n mglobal END");
}
*/

   return;
}


void DBX_DBNAME::MGlobal_Close(const FunctionCallbackInfo<Value>& args)
{
   int js_narg;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   js_narg = args.Length();


   if (js_narg != 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The MGlobal_Close method takes one argument (the MGlobal reference)", 1)));
      return;
   }

#if DBX_NODE_VERSION >= 100000
   mglobal * cx = node::ObjectWrap::Unwrap<mglobal>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
#else
   mglobal * cx = node::ObjectWrap::Unwrap<mglobal>(args[0]->ToObject());
#endif

   cx->delete_mglobal_template(cx);

   return;
}


void DBX_DBNAME::MGlobalQuery(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery method takes at least one argument (the global reference to start with)", 1)));
      return;
   }

   mcursor *cx = mcursor::NewInstance(args);

   dbx_cursor_init((void *) cx);

   cx->c = c;

   /* 1.4.10 */
   rc = dbx_cursor_reset(args, isolate, (void *) cx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery method takes at least one argument (the global reference to start with)", 1)));
      return;
   }

   return;
}


void DBX_DBNAME::MGlobalQuery_Close(const FunctionCallbackInfo<Value>& args)
{
   int js_narg;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   js_narg = args.Length();

   if (js_narg != 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The MGlobalQuery_Close method takes one argument (the MGlobalQuery reference)", 1)));
      return;
   }

#if DBX_NODE_VERSION >= 100000
   mcursor * cx = node::ObjectWrap::Unwrap<mcursor>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
#else
   mcursor * cx = node::ObjectWrap::Unwrap<mcursor>(args[0]->ToObject());
#endif

   cx->delete_mcursor_template(cx);

   return;
}


int DBX_DBNAME::ExtFunctionReference(DBX_DBNAME *c, const FunctionCallbackInfo<Value>& args, DBXCON *pcon, DBXFREF *pfref, DBXFUN *pfun, short context)
{
   int n, nx, rc, otype, len;
   char *p;
   char buffer[64];
   Local<Object> obj;
   Local<String> str;
   DBX_GET_ICONTEXT;

   pcon->ibuffer_used = 0;
   rc = 0;

   if (!context) {
      DBX_DB_LOCK(rc, 0);
   }

   pcon->output_val.svalue.len_used = 0;
   nx = 0;
   n = 0;
   pcon->args[nx].cvalue.pstr = 0;
   if (pfref) {
      dbx_ibuffer_add(pcon, isolate, nx, str, pfref->function, (int) strlen(pfref->function), 1);
   }
   else {
      str = DBX_TO_STRING(args[n]);
      dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 1);
      n ++;
   }

   if (context == 0) {
      T_STRNCPY(pfun->buffer, _dbxso(pfun->buffer), pcon->args[nx].svalue.buf_addr, pcon->args[nx].svalue.len_used);
      pfun->buffer[pcon->args[nx].svalue.len_used] = '\0';
      pfun->label = pfun->buffer;
      pfun->routine = strstr(pfun->buffer, "^");
      *pfun->routine = '\0';
      pfun->routine ++;
      pfun->label_len = (int) strlen(pfun->label);
      pfun->routine_len = (int) strlen(pfun->routine);

      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         rc = pcon->p_isc_so->p_CachePushFunc(&(pfun->rflag), (int) pfun->label_len, (const Callin_char_t *) pfun->label, (int) pfun->routine_len, (const Callin_char_t *) pfun->routine);
      }
   }

   nx ++;

   for (; n < pcon->argc; n ++, nx ++) {

      pcon->args[nx].cvalue.pstr = 0;

      if (args[n]->IsInt32()) {
         pcon->args[nx].type = DBX_TYPE_INT;
         pcon->args[nx].num.int32 = (int) DBX_INT32_VALUE(args[n]);
         T_SPRINTF(buffer, _dbxso(buffer), "%d", pcon->args[nx].num.int32);
         dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 1);
      }
      else {
         pcon->args[nx].type = DBX_TYPE_STR;
         obj = dbx_is_object(args[n], &otype);

         if (otype == 2) {
            p = node::Buffer::Data(obj);
            len = (int) node::Buffer::Length(obj);
            dbx_ibuffer_add(pcon, isolate, nx, str, p, (int) len, 1);
         }
         else {
            str = DBX_TO_STRING(args[n]);
            dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 1);
         }
      }
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB && context == 0) {

         rc = dbx_reference(pcon, nx);

      }
   }

   return rc;
}


void DBX_DBNAME::ExtFunction(const FunctionCallbackInfo<Value>& args)
{
   return ExtFunctionEx(args, 0);
}


void DBX_DBNAME::ExtFunction_bx(const FunctionCallbackInfo<Value>& args)
{
   return ExtFunctionEx(args, 1);
}


void DBX_DBNAME::ExtFunctionEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXFUN fun;
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = binary;
   pcon->lock = 0;
   pcon->increment = 0;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Get", 1)));
      return;
   }
   if (pcon->argc == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Get", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = ExtFunctionReference(c, args, pcon, NULL, &fun, async);

   if (async) {
      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_get;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_function(pcon, &fun);
   }
   else {
      rc = pcon->p_isc_so->p_CacheExtFun(fun.rflag, pcon->argc - 1);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (binary) {
      Local<Object> bx = node::Buffer::New(isolate, (char *) pcon->output_val.svalue.buf_addr, (size_t) pcon->output_val.svalue.len_used).ToLocalChecked();
      args.GetReturnValue().Set(bx);
   }
   else {
      result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
      args.GetReturnValue().Set(result);
   }
}


int DBX_DBNAME::ClassReference(DBX_DBNAME *c, const FunctionCallbackInfo<Value>& args, DBXCON *pcon, DBXCREF *pcref, int argc_offset, short context)
{
   int n, nx, rc, otype, len;
   char *p;
   char buffer[64];
   Local<Object> obj;
   Local<String> str;
   DBX_GET_ICONTEXT;

   pcon->ibuffer_used = 0;
   pcon->cargc = 0;
   rc = 0;

   if (!context) {
      DBX_DB_LOCK(rc, 0);
   }

   pcon->output_val.svalue.len_used = 0;
   nx = 0;
   n = argc_offset;
   pcon->args[nx].cvalue.pstr = 0;
   if (pcref) {
      if (pcref->optype == 0) {
         dbx_ibuffer_add(pcon, isolate, nx, str, pcref->class_name, (int) strlen(pcref->class_name), 0);
      }
      else {
         sprintf(buffer, "%d", pcref->oref);
         pcon->args[nx].num.oref = pcref->oref;
         dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 0);
      }
   }
   else {
      str = DBX_TO_STRING(args[n]);
      dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 0);
      n ++;
   }
   nx ++;

   str = DBX_TO_STRING(args[n]);
   dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 0);
   n ++;
   nx ++;

   if (pcref) {
      if (pcref->optype == 0) { /* classmethod from mclass */
         rc = pcon->p_isc_so->p_CachePushClassMethod((int) pcon->args[0].svalue.len_used, (Callin_char_t *) pcon->args[0].svalue.buf_addr, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr, 1);
      }
      else if (pcref->optype == 1) { /* method from mclass */
         rc = pcon->p_isc_so->p_CachePushMethod(pcref->oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr, 1);
      }
      else if (pcref->optype == 2) { /* set from mclass */
         rc = pcon->p_isc_so->p_CachePushProperty(pcref->oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr);
      }
     else if (pcref->optype == 3) { /* get from mclass */
         rc = pcon->p_isc_so->p_CachePushProperty(pcref->oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr);
      }
   }
   else { /* classmethod from dbx */
      rc = pcon->p_isc_so->p_CachePushClassMethod((int) pcon->args[0].svalue.len_used, (Callin_char_t *) pcon->args[0].svalue.buf_addr, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr, 1);
   }

   for (; n < pcon->argc; n ++, nx ++) {

      pcon->args[nx].cvalue.pstr = 0;

      if (args[n]->IsInt32()) {
         pcon->args[nx].num.int32 = (int) DBX_INT32_VALUE(args[n]);
         T_SPRINTF(buffer, _dbxso(buffer), "%d", pcon->args[nx].num.int32);
         dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 0);
         pcon->args[nx].type = DBX_TYPE_INT;
      }
      else {
         pcon->args[nx].type = DBX_TYPE_STR;
         obj = dbx_is_object(args[n], &otype);

         if (otype == 2) {
            p = node::Buffer::Data(obj);
            len = (int) node::Buffer::Length(obj);
            dbx_ibuffer_add(pcon, isolate, nx, str, p, (int) len, 0);
         }
         else {
            str = DBX_TO_STRING(args[n]);
            dbx_ibuffer_add(pcon, isolate, nx, str, NULL, 0, 0);
         }
      }

      if (pcon->increment && n == (pcon->argc - 1)) { /* v1.3.8 */
         if (pcon->args[nx].svalue.len_used < 32) {
            T_STRNCPY(buffer, _dbxso(buffer), pcon->args[nx].svalue.buf_addr, pcon->args[nx].svalue.len_used);
         }
         else {
            buffer[0] = '1';
            buffer[1] = '\0';
         }
         pcon->args[nx].type = DBX_TYPE_DOUBLE;
         pcon->args[nx].num.real = (double) strtod(buffer, NULL);
      }

      rc = dbx_reference(pcon, nx);
   }

   pcon->cargc = nx;

   return rc;
}


void DBX_DBNAME::ClassMethod(const FunctionCallbackInfo<Value>& args)
{
   return ClassMethodEx(args, 0);
}


void DBX_DBNAME::ClassMethod_bx(const FunctionCallbackInfo<Value>& args)
{
   return ClassMethodEx(args, 1);
}


void DBX_DBNAME::ClassMethodEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   char class_name[256];
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> str;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = binary;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "InterSystems IRIS/Cache classes are not available with YottaDB!", 1)));
      return;
   }

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The classmethod method takes at least one argument (the class name)", 1)));
      return;
   }

   dbx_write_char8(isolate, DBX_TO_STRING(args[0]), class_name, pcon->utf8);
   if (class_name[0] == '\0') {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The classmethod method takes at least one argument (the class name)", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = ClassReference(c, args, pcon, NULL, 0, async);

   if (async) {

      dbx_baton_t *baton = dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_classmethod;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      c->Ref();

      if (dbx_queue_task((void *) dbx_process_task, (void *) dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pcon->cargc - 2);

   /* printf("\r\npcon->cargc=%d; rc=%d", pcon->cargc, rc); */

   if (rc == CACHE_SUCCESS) {
      rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (pcon->output_val.type != DBX_TYPE_OREF) {
      if (binary) {
         Local<Object> bx = node::Buffer::New(isolate, (char *) pcon->output_val.svalue.buf_addr, (size_t) pcon->output_val.svalue.len_used).ToLocalChecked();
         args.GetReturnValue().Set(bx);
      }
      else {
         str = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
         args.GetReturnValue().Set(str);
      }
      return;
   }

   mclass *clx = mclass::NewInstance(args);

   clx->c = c;
   clx->oref =  pcon->output_val.num.oref;
   strcpy(clx->class_name, class_name);

   return;
}


void DBX_DBNAME::ClassMethod_Close(const FunctionCallbackInfo<Value>& args)
{
#if 0
   int js_narg;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   js_narg = args.Length();

   if (js_narg != 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The ClassMethod_Close method takes one argument (the ClassMethod reference)", 1)));
      return;
   }

#if DBX_NODE_VERSION >= 100000
   mclass * clx = node::ObjectWrap::Unwrap<mclass>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
#else
   mclass * clx = node::ObjectWrap::Unwrap<mclass>(args[0]->ToObject());
#endif

   clx->delete_mclass_template(clx);
#endif

   return;
}


void DBX_DBNAME::SQL(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The sql method takes at least one argument (the sql script)", 1)));
      return;
   }

   mcursor *cx = mcursor::NewInstance(args);

   dbx_cursor_init((void *) cx);

   cx->c = c;

   /* 1.4.10 */
   rc = dbx_cursor_reset(args, isolate, (void *) cx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The sql method takes at least one argument (the sql script)", 1)));
      return;
   }

   return;
}


void DBX_DBNAME::SQL_Close(const FunctionCallbackInfo<Value>& args)
{
   int js_narg;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ISOLATE;
   c->dbx_count ++;

   js_narg = args.Length();

   if (js_narg != 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The SQL_Close method takes one argument (the SQL reference)", 1)));
      return;
   }

#if DBX_NODE_VERSION >= 100000
   mcursor * cx = node::ObjectWrap::Unwrap<mcursor>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
#else
   mcursor * cx = node::ObjectWrap::Unwrap<mcursor>(args[0]->ToObject());
#endif

   cx->delete_mcursor_template(cx);

   return;
}


void DBX_DBNAME::Sleep(const FunctionCallbackInfo<Value>& args)
{
   int timeout;
   Local<Integer> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;


   timeout = 0;
   if (args[0]->IsInt32()) {
      timeout = (int) DBX_INT32_VALUE(args[0]);
   }

   dbx_sleep((unsigned long) timeout);
 
   result = DBX_INTEGER_NEW(0);
   args.GetReturnValue().Set(result);
}


void DBX_DBNAME::Benchmark(const FunctionCallbackInfo<Value>& args)
{
   char ibuffer[256];
   char obuffer[256];
   Local<String> result;
   DBX_DBNAME *c = ObjectWrap::Unwrap<DBX_DBNAME>(args.This());
   DBX_GET_ICONTEXT;
   c->dbx_count ++;

   dbx_write_char8(isolate, DBX_TO_STRING(args[0]), ibuffer, 1);

   strcpy(obuffer, "output string");
   result = dbx_new_string8(isolate, obuffer, 1);
   args.GetReturnValue().Set(result);
}


#if DBX_NODE_VERSION >= 120000
class dbxAddonData
{

public:

   dbxAddonData(Isolate* isolate, Local<Object> exports):
      call_count(0) {
         /* Link the existence of this object instance to the existence of exports. */
         exports_.Reset(isolate, exports);
         exports_.SetWeak(this, DeleteMe, WeakCallbackType::kParameter);
      }

   ~dbxAddonData() {
      if (!exports_.IsEmpty()) {
         /* Reset the reference to avoid leaking data. */
         exports_.ClearWeak();
         exports_.Reset();
      }
   }

   /* Per-addon data. */
   int call_count;

private:

   /* Method to call when "exports" is about to be garbage-collected. */
   static void DeleteMe(const WeakCallbackInfo<dbxAddonData>& info) {
      delete info.GetParameter();
   }

   /*
   Weak handle to the "exports" object. An instance of this class will be
   destroyed along with the exports object to which it is weakly bound.
   */
   v8::Persistent<v8::Object> exports_;
};

#endif


/* End of dbx-node class methods */

Persistent<Function> DBX_DBNAME::constructor;

extern "C" {
#if defined(_WIN32)
#if DBX_NODE_VERSION >= 100000
void __declspec(dllexport) init (Local<Object> exports)
#else
void __declspec(dllexport) init (Handle<Object> exports)
#endif
#else
#if DBX_NODE_VERSION >= 100000
static void init (Local<Object> exports)
#else
static void init (Handle<Object> exports)
#endif
#endif
{

   DBX_DBNAME::Init(exports);
   mglobal::Init(exports);
   mcursor::Init(exports);
   mclass::Init(exports);
}

#if DBX_NODE_VERSION >= 120000

/* exports, module, context */
extern "C" NODE_MODULE_EXPORT void
NODE_MODULE_INITIALIZER(Local<Object> exports,
                        Local<Value> module,
                        Local<Context> context) {
   Isolate* isolate = context->GetIsolate();

   /* Create a new instance of dbxAddonData for this instance of the addon. */
   dbxAddonData* data = new dbxAddonData(isolate, exports);
   /* Wrap the data in a v8::External so we can pass it to the method we expose. */
   /* Local<External> external = External::New(isolate, data); */
   External::New(isolate, data);

   init(exports);

   /*
   Expose the method "Method" to JavaScript, and make sure it receives the
   per-addon-instance data we created above by passing `external` as the
   third parameter to the FunctionTemplate constructor.
   exports->Set(context, String::NewFromUtf8(isolate, "method", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, Method, external)->GetFunction(context).ToLocalChecked()).FromJust();
   */

}

#else

  NODE_MODULE(dbx, init)

#endif
}


#if DBX_NODE_VERSION >= 100000
void dbx_set_prototype_method(v8::Local<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data)
#else
void dbx_set_prototype_method(v8::Handle<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data)
#endif
{
#if DBX_NODE_VERSION >= 100000

   v8::Isolate* isolate = v8::Isolate::GetCurrent();
   v8::HandleScope handle_scope(isolate);
   v8::Local<v8::Signature> s = v8::Signature::New(isolate, t);

#if DBX_NODE_VERSION >= 120000
   v8::Local<v8::String> data_str = v8::String::NewFromUtf8(isolate, data, NewStringType::kNormal).ToLocalChecked();
#else
   v8::Local<v8::String> data_str = v8::String::NewFromUtf8(isolate, data);
#endif

#if 0
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), s);
#else
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, data_str, s);
#endif

#if DBX_NODE_VERSION >= 120000
   v8::Local<v8::String> fn_name = String::NewFromUtf8(isolate, name, NewStringType::kNormal).ToLocalChecked();;

#else
   v8::Local<v8::String> fn_name = String::NewFromUtf8(isolate, name);
#endif

   tx->SetClassName(fn_name);
   t->PrototypeTemplate()->Set(fn_name, tx);
#else
   NODE_SET_PROTOTYPE_METHOD(t, name, callback);
#endif

   return;
}


v8::Local<v8::Object> dbx_is_object(v8::Local<v8::Value> value, int *otype)
{
#if DBX_NODE_VERSION >= 100000
   v8::Isolate* isolate = v8::Isolate::GetCurrent();
   v8::Local<v8::Context> icontext = isolate->GetCurrentContext();
#endif
   *otype = 0;

   if (value->IsObject()) {
      *otype = 1;
      v8::Local<v8::Object> value_obj = DBX_TO_OBJECT(value);
      if (node::Buffer::HasInstance(value_obj)) {
         *otype = 2;
      }
      return value_obj;
   }
   else {
      return v8::Local<v8::Object>::Cast(value);
   }
}


int dbx_string8_length(v8::Isolate * isolate, v8::Local<v8::String> str, int utf8)
{
   if (utf8) {
      return DBX_UTF8_LENGTH(str);
   }
   else {
      return DBX_LENGTH(str);
   }
}

v8::Local<v8::String> dbx_new_string8(v8::Isolate * isolate, char * buffer, int utf8)
{
   if (utf8) {
#if DBX_NODE_VERSION >= 120000
      return v8::String::NewFromUtf8(isolate, buffer, NewStringType::kNormal).ToLocalChecked();
#else
      return v8::String::NewFromUtf8(isolate, buffer);
#endif
   }
   else {
#if DBX_NODE_VERSION >= 100000
      return v8::String::NewFromOneByte(isolate, (uint8_t *) buffer, NewStringType::kInternalized).ToLocalChecked();
#else
      /* return String::NewFromOneByte(isolate, (uint8_t *) buffer, String::kNormalString); */
      return v8::String::NewFromOneByte(isolate, (uint8_t *) buffer, v8::NewStringType::kInternalized).ToLocalChecked();
#endif
   }
}


v8::Local<v8::String> dbx_new_string8n(v8::Isolate * isolate, char * buffer, unsigned long len, int utf8)
{
   if (utf8) {
#if DBX_NODE_VERSION >= 120000
      return v8::String::NewFromUtf8(isolate, buffer, NewStringType::kNormal, len).ToLocalChecked();
#else
      return v8::String::NewFromUtf8(isolate, buffer, String::kNormalString, len);
#endif
   }
   else {
#if DBX_NODE_VERSION >= 100000
      return v8::String::NewFromOneByte(isolate, (uint8_t *) buffer, NewStringType::kInternalized, len).ToLocalChecked();
#else
      /* return v8::String::NewFromOneByte(isolate, (uint8_t *) buffer, String::kNormalString, len); */
      return v8::String::NewFromOneByte(isolate, (uint8_t *) buffer, v8::NewStringType::kInternalized, len).ToLocalChecked();
#endif

   }
}


int dbx_write_char8(v8::Isolate * isolate, v8::Local<v8::String> str, char * buffer, int utf8)
{
   if (utf8) {
      return DBX_WRITE_UTF8(str, buffer);
   }
   else {
      return DBX_WRITE_ONE_BYTE(str, (uint8_t *) buffer);
   }
}


int dbx_ibuffer_add(DBXCON *pcon, v8::Isolate * isolate, int argn, v8::Local<v8::String> str, char * buffer, int buffer_len, short context)
{
   int len;
   unsigned char *p, *phead;

   if (buffer)
      len = buffer_len;
   else
      len = dbx_string8_length(isolate, str, pcon->utf8);

   /* 1.4.11 resize input buffer if necessary */

   if ((pcon->ibuffer_used + len + 32) > pcon->ibuffer_size) {
      p = (unsigned char *) dbx_malloc(sizeof(char) * (pcon->ibuffer_used + len + CACHE_MAXSTRLEN + DBX_IBUFFER_OFFSET), 301);
      if (p) {
         memcpy((void *) p, (void *) (pcon->ibuffer - DBX_IBUFFER_OFFSET), (size_t) (pcon->ibuffer_used + DBX_IBUFFER_OFFSET));
         dbx_free((void *) (pcon->ibuffer - DBX_IBUFFER_OFFSET), 301);
         pcon->ibuffer = (p + DBX_IBUFFER_OFFSET);
         pcon->ibuffer_size = (pcon->ibuffer_used + len + CACHE_MAXSTRLEN);
      }
      else {
         return 0;
      }
   }

   phead = (pcon->ibuffer + pcon->ibuffer_used);
   pcon->ibuffer_used += 5;
   p = (pcon->ibuffer + pcon->ibuffer_used);

   if (buffer) {
      T_MEMCPY((void *) p, (void *) buffer, (size_t) len); /* 1.4.11 */
   }
   else {
      dbx_write_char8(isolate, str, (char *) p, pcon->utf8);
   }
   pcon->ibuffer_used += len;

   pcon->args[argn].type = DBX_TYPE_STR; /* Bug fix v1.3.7 */

   if (context == 2) { /* v1.4.10 */
      dbx_add_block_size(phead, 0, len, pcon->args[argn].sort, DBX_TYPE_STR8);
   }

   pcon->args[argn].svalue.buf_addr = (char *) p;
   pcon->args[argn].svalue.len_alloc = len;
   pcon->args[argn].svalue.len_used = len;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (argn > 0) {
         pcon->yargs[argn - 1].len_used = pcon->args[argn].svalue.len_used;
         pcon->yargs[argn - 1].len_alloc = pcon->args[argn].svalue.len_alloc;
         pcon->yargs[argn - 1].buf_addr = (char *) pcon->args[argn].svalue.buf_addr;
      }

      if (argn == 0 && context == 0 && pcon->args[argn].svalue.buf_addr[0] != '^') {
         pcon->args[argn].svalue.buf_addr = (char *) (p - 1);
         pcon->args[argn].svalue.len_used ++;
         pcon->args[argn].svalue.len_alloc ++;
         pcon->args[argn].svalue.buf_addr[0] = '^';
      }
   }
   else {
      if (pcon->lock && argn == 0 && context == 0 && pcon->args[argn].svalue.buf_addr[0] != '^') {
         pcon->args[argn].svalue.buf_addr = (char *) (p - 1);
         pcon->args[argn].svalue.len_used ++;
         pcon->args[argn].svalue.len_alloc ++;
         pcon->args[argn].svalue.buf_addr[0] = '^';
      }
   }

   return len;
}


int dbx_global_reset(const v8::FunctionCallbackInfo<v8::Value>& args, v8::Isolate * isolate, void *pgx, int argc_offset, short context)
{
   v8::Local<v8::Context> icontext = isolate->GetCurrentContext();
   int n, len, otype;
   char global_name[256], *p;
   DBXCON *pcon;
   DBXVAL *pval, *pvalp;
   v8::Local<v8::Object> obj;
   v8::Local<v8::String> str;
   mglobal *gx = (mglobal *) pgx;
   DBX_DBNAME *c = gx->c;

   pcon = c->pcon;

   dbx_write_char8(isolate, DBX_TO_STRING(args[argc_offset]), global_name, pcon->utf8);
   if (global_name[0] == '\0') {
      return -1;
   }

   pval = gx->pkey;
   while (pval) {
      pvalp = pval;
      pval = pval->pnext;
      dbx_free((void *) pvalp, 0);
   }
   gx->pkey = NULL;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (global_name[0] == '^') {
         T_STRCPY(gx->global_name, _dbxso(gx->global_name), global_name);
      }
      else {
         gx->global_name[0] = '^';
         T_STRCPY(gx->global_name + 1, _dbxso(gx->global_name), global_name);
      }
   }
   else {
      if (global_name[0] == '^') {
         T_STRCPY(gx->global_name, _dbxso(gx->global_name), global_name + 1);
      }
      else {
         T_STRCPY(gx->global_name, _dbxso(gx->global_name), global_name);
      }
   }

   pvalp = NULL;
   for (n = (argc_offset + 1); n < pcon->argc; n ++) {
      if (args[n]->IsInt32()) {
         pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL), 0);
         pval->type = DBX_TYPE_INT;
         pval->num.int32 = (int) DBX_INT32_VALUE(args[n]);
      }
      else {
         obj = dbx_is_object(args[n], &otype);
         if (otype == 2) {
            p = node::Buffer::Data(obj);
            len = (int) node::Buffer::Length(obj);
            pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL) + len + 32, 0);
            pval->type = DBX_TYPE_STR;
            pval->svalue.buf_addr = ((char *) pval) + sizeof(DBXVAL);
            memcpy((void *) pval->svalue.buf_addr, (void *) p, (size_t) len);
            pval->svalue.len_alloc = len + 32;
            pval->svalue.len_used = len;
         }
         else {
            str = DBX_TO_STRING(args[n]);
            len = (int) dbx_string8_length(isolate, str, 0);
            pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL) + len + 32, 0);
            pval->type = DBX_TYPE_STR;
            pval->svalue.buf_addr = ((char *) pval) + sizeof(DBXVAL);
            dbx_write_char8(isolate, str, pval->svalue.buf_addr, 1);
            pval->svalue.len_alloc = len + 32;
            pval->svalue.len_used = len;
         }
      }
      if (pvalp) {
         pvalp->pnext = pval;
      }
      else {
         gx->pkey = pval;
      }
      pvalp = pval;
      pvalp->pnext = NULL;
   }
   return 0;
}


int dbx_cursor_init(void *pcx)
{
   mcursor *cx = (mcursor *) pcx;

   cx->context = 0;
   cx->getdata = 0;
   cx->multilevel = 0;
   cx->format = 0;
   cx->counter = 0;
   cx->global_name[0] = '\0';
   cx->pqr_prev = NULL;
   cx->pqr_next = NULL;
   cx->data.buf_addr = NULL;
   cx->data.len_alloc = 0;
   cx->data.len_used = 0;
   cx->psql = NULL;
   cx->c = NULL;

   return 0;
}


int dbx_cursor_reset(const v8::FunctionCallbackInfo<v8::Value>& args, v8::Isolate * isolate, void *pcx, int argc_offset, short context)
{
   v8::Local<v8::Context> icontext = isolate->GetCurrentContext();
   int n, len;
   char global_name[256], buffer[256];
   DBXSQL *psql;
   DBXCON *pcon;
   v8::Local<v8::Object> obj;
   v8::Local<v8::String> key;
   v8::Local<v8::String> value;
   mcursor *cx = (mcursor *) pcx;
   DBX_DBNAME *c = cx->c;

   pcon = c->pcon;

   if (pcon->argc < 1) {
      return -1;
   }

   obj = DBX_TO_OBJECT(args[argc_offset]);
   key = dbx_new_string8(isolate, (char *) "sql", 1);
   if (DBX_GET(obj, key)->IsString()) {
      value = DBX_TO_STRING(DBX_GET(obj, key));
      len = (int) dbx_string8_length(isolate, value, 0);
      psql = (DBXSQL *) dbx_malloc(sizeof(DBXSQL) + (len + 4), 0);
      for (n = 0; n < DBX_SQL_MAXCOL; n ++) {
         psql->cols[n] = NULL;
      }
      psql->no_cols = 0;
      psql->sql_script = ((char *) psql) + sizeof(DBXSQL);
      psql->sql_script_len = len;
      dbx_write_char8(isolate, value, psql->sql_script, pcon->utf8);

      psql->sql_type = DBX_SQL_MGSQL;
      key = dbx_new_string8(isolate, (char *) "type", 1);
      if (DBX_GET(obj, key)->IsString()) {
         value = DBX_TO_STRING(DBX_GET(obj, key));
         dbx_write_char8(isolate, value, buffer, pcon->utf8);
         dbx_lcase(buffer);
         if (strstr(buffer, "intersys") || strstr(buffer, "cach") || strstr(buffer, "iris")) {
            psql->sql_type = DBX_SQL_ISCSQL;
         }
      }

      cx->psql = psql;
      DBX_DB_LOCK(n, 0);
      psql->sql_no = ++ dbx_sql_counter;
      DBX_DB_UNLOCK(n);
   
      cx->context = 11;
      cx->counter = 0;
      cx->getdata = 0;
      cx->multilevel = 0;
      cx->format = 0;

      if (pcon->argc > (argc_offset + 1)) {
         dbx_is_object(args[argc_offset + 1], &n);
         if (n) {
            obj = DBX_TO_OBJECT(args[argc_offset + 1]);
            key = dbx_new_string8(isolate, (char *) "format", 1);
            if (DBX_GET(obj, key)->IsString()) {
               char buffer[64];
               value = DBX_TO_STRING(DBX_GET(obj, key));
               dbx_write_char8(isolate, value, buffer, 1);
               dbx_lcase(buffer);
               if (!strcmp(buffer, "url")) {
                  cx->format = 1;
               }
            }
         }
      }
      return 0;
   }


   if (!cx->pqr_prev) {
      cx->pqr_prev = dbx_alloc_dbxqr(NULL, 0, 0);
   }
   if (!cx->pqr_next) {
      cx->pqr_next = dbx_alloc_dbxqr(NULL, 0, 0);
   }
   if (!cx->data.buf_addr) {
      cx->data.buf_addr = (char *) dbx_malloc(CACHE_MAXSTRLEN, 0);
      cx->data.len_alloc = CACHE_MAXSTRLEN;
      cx->data.len_used = 0;
   }

   key = dbx_new_string8(isolate, (char *) "global", 1);
   if (DBX_GET(obj, key)->IsString()) {
      dbx_write_char8(isolate, DBX_TO_STRING(DBX_GET(obj, key)), global_name, pcon->utf8);
   }
   else {
      return -1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (global_name[0] == '^') {
         T_STRCPY(cx->global_name, _dbxso(cx->global_name), global_name);
      }
      else {
         cx->global_name[0] = '^';
         T_STRCPY(cx->global_name + 1, _dbxso(cx->global_name), global_name);
      }
   }
   else {
      if (global_name[0] == '^') {
         T_STRCPY(cx->global_name, _dbxso(cx->global_name), global_name + 1);
      }
      else {
         T_STRCPY(cx->global_name, _dbxso(cx->global_name), global_name);
      }
   }

/*

   strcpy(cx->pqr_next->global_name.buf_addr, cx->global_name);
   strcpy(cx->pqr_prev->global_name.buf_addr, cx->global_name);
   cx->pqr_prev->global_name.len_used = (int) strlen((char *) cx->pqr_prev->global_name.buf_addr);
   cx->pqr_next->global_name.len_used = cx->pqr_prev->global_name.len_used;

   cx->pqr_prev->keyn = 0;
   key = dbx_new_string8(isolate, (char *) "key", 1);
   if (DBX_GET(obj, key)->IsArray()) {
      Local<Array> a = Local<Array>::Cast(DBX_GET(obj, key));
      cx->pqr_prev->keyn = (int) a->Length();
      for (n = 0; n < cx->pqr_prev->keyn; n ++) {
         value = DBX_TO_STRING(DBX_GET(a, n));
         len = (int) dbx_string8_length(isolate, value, 0);
         dbx_write_char8(isolate, value, cx->pqr_prev->key[n].buf_addr, 1);
         cx->pqr_prev->key[n].len_used = len;
      }
   }

   cx->context = 1;
   cx->counter = 0;
   cx->getdata = 0;
   cx->multilevel = 0;
   cx->format = 0;
*/
   strcpy(cx->pqr_next->global_name.buf_addr, cx->global_name);
   strcpy(cx->pqr_prev->global_name.buf_addr, cx->global_name);
   cx->pqr_prev->global_name.len_used = (int) strlen((char *) cx->pqr_prev->global_name.buf_addr);
   cx->pqr_next->global_name.len_used =  cx->pqr_prev->global_name.len_used;
   cx->pqr_prev->keyn = 0;
   key = dbx_new_string8(isolate, (char *) "key", 1);
   if (DBX_GET(obj, key)->IsArray()) {
      Local<Array> a = Local<Array>::Cast(DBX_GET(obj, key));
      cx->pqr_prev->keyn = (int) a->Length();
      for (n = 0; n < cx->pqr_prev->keyn; n ++) {
         value = DBX_TO_STRING(DBX_GET(a, n));
         len = (int) dbx_string8_length(isolate, value, 0);
         dbx_write_char8(isolate, value, cx->pqr_prev->key[n].buf_addr, 1);
         cx->pqr_prev->key[n].len_used = len;
      }
   }

   cx->context = 1;
   cx->counter = 0;
   cx->getdata = 0;
   cx->multilevel = 0;
   cx->format = 0;

   if (pcon->argc > (argc_offset + 1)) {
      obj = DBX_TO_OBJECT(args[argc_offset + 1]);
      key = dbx_new_string8(isolate, (char *) "getdata", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->getdata = 1;
         }
      }
      key = dbx_new_string8(isolate, (char *) "multilevel", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->context = 2;
         }
      }
      key = dbx_new_string8(isolate, (char *) "globaldirectory", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->context = 9;
         }
      }
      key = dbx_new_string8(isolate, (char *) "format", 1);
      if (DBX_GET(obj, key)->IsString()) {
         char buffer[64];
         value = DBX_TO_STRING(DBX_GET(obj, key));
         dbx_write_char8(isolate, value, buffer, 1);
         dbx_lcase(buffer);
         if (!strcmp(buffer, "url")) {
            cx->format = 1;
         }
      }
   }

   if (cx->context != 9 && cx->global_name[0] == '\0') { /* not a global directory so global name cannot be empty */
      return -1;
   }

   return 0;
}


int isc_load_library(DBXCON *pcon)
{
   int n, len, result;
   char primlib[DBX_ERROR_SIZE], primerr[DBX_ERROR_SIZE];
   char verfile[256], fun[64];
   char *libnam[16];

   result = CACHE_SUCCESS;

   strcpy(pcon->p_isc_so->libdir, pcon->path);
   strcpy(pcon->p_isc_so->funprfx, "Cache");
   strcpy(pcon->p_isc_so->dbname, "Cache");

   strcpy(verfile, pcon->path);
   len = (int) strlen(pcon->p_isc_so->libdir);
   if (pcon->p_isc_so->libdir[len - 1] == '/' || pcon->p_isc_so->libdir[len - 1] == '\\') {
      pcon->p_isc_so->libdir[len - 1] = '\0';
      len --;
   }
   for (n = len - 1; n > 0; n --) {
      if (pcon->p_isc_so->libdir[n] == '/') {
         strcpy((pcon->p_isc_so->libdir + (n + 1)), "bin/");
         break;
      }
      else if (pcon->p_isc_so->libdir[n] == '\\') {
         strcpy((pcon->p_isc_so->libdir + (n + 1)), "bin\\");
         break;
      }
   }

   n = 0;
   if (pcon->dbtype == DBX_DBTYPE_IRIS) {
#if defined(_WIN32)
      libnam[n ++] = (char *) DBX_IRIS_DLL;
      libnam[n ++] = (char *) DBX_CACHE_DLL;
#else
#if defined(MACOSX)
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
#else
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
#endif
#endif
   }
   else {
#if defined(_WIN32)
      libnam[n ++] = (char *) DBX_CACHE_DLL;
      libnam[n ++] = (char *) DBX_IRIS_DLL;
#else
#if defined(MACOSX)
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
#else
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
#endif
#endif
   }

   libnam[n ++] = NULL;
   strcpy(pcon->p_isc_so->libnam, pcon->p_isc_so->libdir);
   len = (int) strlen(pcon->p_isc_so->libnam);

   for (n = 0; libnam[n]; n ++) {
      strcpy(pcon->p_isc_so->libnam + len, libnam[n]);
      if (!n) {
         strcpy(primlib, pcon->p_isc_so->libnam);
      }

      pcon->p_isc_so->p_library = dbx_dso_load(pcon->p_isc_so->libnam);
      if (pcon->p_isc_so->p_library) {
         if (strstr(libnam[n], "iris")) {
            pcon->p_isc_so->iris = 1;
            strcpy(pcon->p_isc_so->funprfx, "Iris");
            strcpy(pcon->p_isc_so->dbname, "IRIS");
         }
         T_STRCPY(pcon->error, _dbxso(pcon->error), "");
         pcon->error_code = 0;
         break;
      }

      if (!n) {
         int len1, len2;
         char *p;
#if defined(_WIN32)
         DWORD errorcode;
         LPVOID lpMsgBuf;

         lpMsgBuf = NULL;
         errorcode = GetLastError();
         sprintf(pcon->error, "Error loading %s Library: %s; Error Code : %ld", pcon->p_isc_so->dbname, primlib, errorcode);
         len2 = (int) strlen(pcon->error);
         len1 = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorcode,
                        /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPTSTR) &lpMsgBuf,
                        0,
                        NULL 
                        );
         if (lpMsgBuf && len1 > 0 && (DBX_ERROR_SIZE - len2) > 30) {
            strncpy(primerr, (const char *) lpMsgBuf, DBX_ERROR_SIZE - 1);
            p = strstr(primerr, "\r\n");
            if (p)
               *p = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            p = strstr(primerr, "%1");
            if (p) {
               *p = 'I';
               *(p + 1) = 't';
            }
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
         if (lpMsgBuf)
            LocalFree(lpMsgBuf);
#else
         p = (char *) dlerror();
         sprintf(primerr, "Cannot load %s library: Error Code: %d", pcon->p_isc_so->dbname, errno);
         len2 = strlen(pcon->error);
         if (p) {
            strncpy(primerr, p, DBX_ERROR_SIZE - 1);
            primerr[DBX_ERROR_SIZE - 1] = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
#endif
      }
   }

   if (!pcon->p_isc_so->p_library) {
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sSetDir", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSetDir = (int (*) (char *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheSetDir) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sSecureStartA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSecureStartA = (int (*) (CACHE_ASTRP, CACHE_ASTRP, CACHE_ASTRP, unsigned long, int, CACHE_ASTRP, CACHE_ASTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheSecureStartA) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sEnd", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEnd = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheEnd) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sExStrNew", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNew = (unsigned char * (*) (CACHE_EXSTRP, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrNew) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sExStrNewW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNewW = (unsigned short * (*) (CACHE_EXSTRP, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrNewW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sExStrNewH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNewH = (wchar_t * (*) (CACHE_EXSTRP, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPushExStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStr = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushExStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushExStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStrW = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushExStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushExStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStrH = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPopExStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStr = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopExStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopExStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStrW = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopExStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopExStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStrH = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sExStrKill", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrKill = (int (*) (CACHE_EXSTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrKill) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStr = (int (*) (int, Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStrW = (int (*) (int, short *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStrH = (int (*) (int, wchar_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPopStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStr = (int (*) (int *, Callin_char_t **)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStrW = (int (*) (int *, short **)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStrH = (int (*) (int *, wchar_t **)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPushDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushDbl = (int (*) (double)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushIEEEDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushIEEEDbl = (int (*) (double)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushIEEEDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopDbl = (int (*) (double *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushInt", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushInt = (int (*) (int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushInt) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopInt", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopInt = (int (*) (int *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopInt) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sPushInt64", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushInt64 = (int (*) (CACHE_INT64)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
/*
   if (!pcon->p_isc_so->p_CachePushInt64) {
      pcon->p_isc_so->p_CachePushInt64 = (int (*) (CACHE_INT64)) pcon->p_isc_so->p_CachePushInt;
   }

   sprintf(fun, "%sPushInt64", pcon->p_isc_so->funprfx);
   if (!pcon->p_isc_so->p_CachePushInt64) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
*/
   sprintf(fun, "%sPopInt64", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopInt64 = (int (*) (CACHE_INT64 *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
/*
   if (!pcon->p_isc_so->p_CachePopInt64) {
      pcon->p_isc_so->p_CachePopInt64 = (int (*) (CACHE_INT64 *)) pcon->p_isc_so->p_CachePopInt;
   }
   if (!pcon->p_isc_so->p_CachePopInt64) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
*/

   sprintf(fun, "%sPushGlobal", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushGlobal = (int (*) (int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushGlobal) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushGlobalX", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushGlobalX = (int (*) (int, const Callin_char_t *, int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushGlobalX) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalGet", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalGet = (int (*) (int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalGet) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalSet", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalSet = (int (*) (int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalSet) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalData", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalData = (int (*) (int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalData) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalKill", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalKill = (int (*) (int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalKill) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalOrder", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalOrder = (int (*) (int, int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalOrder) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalQuery", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalQuery = (int (*) (int, int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalQuery) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalIncrement", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalIncrement = (int (*) (int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalIncrement) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalRelease", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalRelease = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalRelease) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sAcquireLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAcquireLock = (int (*) (int, int, int, int *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheAcquireLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sReleaseAllLocks", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheReleaseAllLocks = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheReleaseAllLocks) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sReleaseLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheReleaseLock = (int (*) (int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheReleaseLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushLock = (int (*) (int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sAddGlobal", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddGlobal = (int (*) (int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddGlobalDescriptor", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddGlobalDescriptor = (int (*) (int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddSSVN", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddSSVN = (int (*) (int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddSSVNDescriptor", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddSSVNDescriptor = (int (*) (int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sMerge", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheMerge = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   if (pcon->p_isc_so->p_CacheAddGlobal && pcon->p_isc_so->p_CacheAddGlobalDescriptor && pcon->p_isc_so->p_CacheAddSSVN && pcon->p_isc_so->p_CacheAddSSVNDescriptor && pcon->p_isc_so->p_CacheMerge) {
      pcon->p_isc_so->merge_enabled = 1;
   }

   sprintf(fun, "%sPushFunc", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushFunc = (int (*) (unsigned int *, int, const Callin_char_t *, int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sExtFun", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExtFun = (int (*) (unsigned int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushRtn", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushRtn = (int (*) (unsigned int *, int, const Callin_char_t *, int, const Callin_char_t *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sDoFun", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheDoFun = (int (*) (unsigned int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sDoRtn", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheDoRtn = (int (*) (unsigned int, int)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   if (pcon->p_isc_so->p_CachePushFunc && pcon->p_isc_so->p_CacheExtFun && pcon->p_isc_so->p_CachePushRtn && pcon->p_isc_so->p_CacheDoFun && pcon->p_isc_so->p_CacheDoRtn) {
      pcon->p_isc_so->functions_enabled = 1;
   }

   sprintf(fun, "%sCloseOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheCloseOref = (int (*) (unsigned int oref)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sIncrementCountOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheIncrementCountOref = (int (*) (unsigned int oref)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPopOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopOref = (int (*) (unsigned int * orefp)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushOref = (int (*) (unsigned int oref)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sInvokeMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheInvokeMethod = (int (*) (int narg)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushMethod = (int (*) (unsigned int oref, int mlen, const Callin_char_t * mptr, int flg)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sInvokeClassMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheInvokeClassMethod = (int (*) (int narg)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushClassMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushClassMethod = (int (*) (int clen, const Callin_char_t * cptr, int mlen, const Callin_char_t * mptr, int flg)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sGetProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGetProperty = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sSetProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSetProperty = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushProperty = (int (*) (unsigned int oref, int plen, const Callin_char_t * pptr)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   if (pcon->p_isc_so->p_CacheCloseOref && pcon->p_isc_so->p_CacheIncrementCountOref && pcon->p_isc_so->p_CachePopOref && pcon->p_isc_so->p_CachePushOref
         && pcon->p_isc_so->p_CacheInvokeMethod && pcon->p_isc_so->p_CachePushMethod && pcon->p_isc_so->p_CacheInvokeClassMethod && pcon->p_isc_so->p_CachePushClassMethod
         && pcon->p_isc_so->p_CacheGetProperty && pcon->p_isc_so->p_CacheSetProperty && pcon->p_isc_so->p_CachePushProperty) {
      pcon->p_isc_so->objects_enabled = 1;
   }

   sprintf(fun, "%sType", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheType = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sEvalA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEvalA = (int (*) (CACHE_ASTRP volatile)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sExecuteA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExecuteA = (int (*) (CACHE_ASTRP volatile)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sConvert", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheConvert = (int (*) (unsigned long, void *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sErrorA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheErrorA = (int (*) (CACHE_ASTRP, CACHE_ASTRP, int *)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sErrxlateA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheErrxlateA = (int (*) (int, CACHE_ASTRP)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sEnableMultiThread", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEnableMultiThread = (int (*) (void)) dbx_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (pcon->p_isc_so->p_CacheEnableMultiThread) {
      pcon->p_isc_so->multithread_enabled = 1;
   }
   else {
      pcon->p_isc_so->multithread_enabled = 0;
   }

   pcon->p_isc_so->loaded = 1;

isc_load_library_exit:

   if (pcon->error[0]) {
      pcon->p_isc_so->loaded = 0;
      pcon->error_code = 1009;
      strcpy((char *) pcon->output_val.svalue.buf_addr, "0");
      result = CACHE_NOCON;

      return result;
   }

   result = pcon->p_isc_so->p_CacheSetDir(pcon->path);

   T_STRCPY((char *) pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_alloc, "1");
   pcon->p_isc_so->loaded = 2;
   result = CACHE_SUCCESS;

   return result;
}



int isc_authenticate(DBXCON *pcon)
{
   unsigned char reopen;
   int termflag, timeout;
   char buffer[256];

   termflag = 0;
	timeout = 15;
	CACHESTR pin, *ppin;
	CACHESTR pout, *ppout;
	CACHESTR pusername;
	CACHESTR ppassword;
	CACHESTR pexename;
	int rc;

   reopen = 0;

isc_authenticate_reopen:

   pcon->error_code = 0;
   *pcon->error = '\0';
	T_STRCPY((char *) pexename.str, _dbxso(pexename.str), "Node.JS");
	pexename.len = (unsigned short) strlen((char *) pexename.str);
/*
	T_STRCPY((char *) pin.str, "//./nul");
	T_STRCPY((char *) pout.str, "//./nul");
	pin.len = (unsigned short) strlen((char *) pin.str);
	pout.len = (unsigned short) strlen((char *) pout.str);
*/
   ppin = NULL;

   if (pcon->input_device[0]) {
	   T_STRCPY(buffer, _dbxso(buffer), pcon->input_device);
      dbx_lcase(buffer);
      if (!strcmp(buffer, "stdin")) {
	      T_STRCPY((char *) pin.str, _dbxso(pin.str), "");
         ppin = &pin;
      }
      else if (strcmp(pcon->input_device, DBX_NULL_DEVICE)) {
	      T_STRCPY((char *) pin.str, _dbxso(pin.str), pcon->input_device);
         ppin = &pin;
      }
      if (ppin)
	      ppin->len = (unsigned short) strlen((char *) ppin->str);
   }
   ppout = NULL;
   if (pcon->output_device[0]) {
	   T_STRCPY(buffer, _dbxso(buffer), pcon->output_device);
      dbx_lcase(buffer);
      if (!strcmp(buffer, "stdout")) {
	      T_STRCPY((char *) pout.str, _dbxso(pout.str), "");
         ppout = &pout;
      }
      else if (strcmp(pcon->output_device, DBX_NULL_DEVICE)) {
	      T_STRCPY((char *) pout.str, _dbxso(pout.str), pcon->output_device);
         ppout = &pout;
      }
      if (ppout)
	      ppout->len = (unsigned short) strlen((char *) ppout->str);
   }

   if (ppin && ppout) { 
      termflag = CACHE_TTALL|CACHE_PROGMODE;
   }
   else {
      termflag = CACHE_TTNEVER|CACHE_PROGMODE;
   }

	T_STRCPY((char *) pusername.str, _dbxso(pusername.str), pcon->username);
	T_STRCPY((char *) ppassword.str, _dbxso(ppassword.str), pcon->password);

	pusername.len = (unsigned short) strlen((char *) pusername.str);
	ppassword.len = (unsigned short) strlen((char *) ppassword.str);

	/* Authenticate using a real username */

/*
#if !defined(_WIN32)
{
   int ret;
   pthread_attr_t tattr;
   size_t size;
   ret = pthread_getattr_np(pthread_self(), &tattr);
   ret = pthread_attr_getstacksize(&tattr, &size);   printf("\r\n*** pthread_attr_getstacksize (main) %x *** ret=%d; size=%u\r\n", (unsigned long) pthread_self(), ret, size);
}
#endif
*/

#if !defined(_WIN32)
   signal(SIGUSR1, SIG_IGN);
#endif

	rc = pcon->p_isc_so->p_CacheSecureStartA(&pusername, &ppassword, &pexename, termflag, timeout, ppin, ppout);

	if (rc != CACHE_SUCCESS) {
      pcon->error_code = rc;
	   if (rc == CACHE_ACCESSDENIED) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: Access Denied : Check the audit log for the real authentication error (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_CHANGEPASSWORD) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: Password Change Required (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_ALREADYCON) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: Already Connected (%d)\n", rc);
	      return 1;
	   }
	   if (rc == CACHE_CONBROKEN) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: Connection was formed and then broken by the server. (%d)\n", rc);
	      return 0;
	   }

	   if (rc == CACHE_FAILURE) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: An unexpected error has occurred. (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_STRTOOLONG) {
	      T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: prinp or prout is too long. (%d)\n", rc);
	      return 0;
	   }
	   T_SPRINTF(pcon->error, _dbxso(pcon->error), "Authentication: Failed (%d)\n", rc);
	   return 0;
   }


   {
      CACHE_ASTR retval;
      CACHE_ASTR expr;

      T_STRCPY((char *) expr.str, _dbxso(expr.str), "$ZVersion");
      expr.len = (unsigned short) strlen((char *) expr.str);
      rc = pcon->p_isc_so->p_CacheEvalA(&expr);

      if (rc == CACHE_CONBROKEN)
         reopen = 1;
      if (rc == CACHE_SUCCESS) {
         retval.len = 256;
         rc = pcon->p_isc_so->p_CacheConvert(CACHE_ASTRING, &retval);

         if (rc == CACHE_CONBROKEN)
            reopen = 1;
         if (rc == CACHE_SUCCESS) {
            isc_parse_zv((char *) retval.str, pcon->p_zv);
            T_SPRINTF(pcon->p_zv->version, _dbxso(pcon->p_zv->version), "%d.%d build %d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->dbx_build);
         }
      }
   }

   if (reopen) {
      goto isc_authenticate_reopen;
   }

   rc = isc_change_namespace(pcon, pcon->nspace);

   if (pcon->p_isc_so->p_CacheEnableMultiThread) {
      rc = pcon->p_isc_so->p_CacheEnableMultiThread();
   }
   else {
      rc = -1;
   }

   return 1;
}


int isc_open(DBXCON *pcon)
{
   int rc, error_code, result;

   error_code = 0;

   if (!pcon->p_isc_so) {
      pcon->p_isc_so = (DBXISCSO *) dbx_malloc(sizeof(DBXISCSO), 0);
      if (!pcon->p_isc_so) {
         T_STRCPY(pcon->error, _dbxso(pcon->error), "No Memory");
         pcon->error_code = 1009; 
         result = CACHE_NOCON;
         return result;
      }
      memset((void *) pcon->p_isc_so, 0, sizeof(DBXISCSO));
      pcon->p_isc_so->loaded = 0;
      pcon->p_isc_so->no_connections = 0;
      pcon->p_isc_so->multiple_connections = 0;
      pcon->p_zv = &(pcon->p_isc_so->zv);
   }

   if (pcon->p_isc_so->loaded == 2) {
      strcpy(pcon->error, "Cannot create multiple connections to the database");
      pcon->error_code = 1009; 
      strcpy((char *) pcon->output_val.svalue.buf_addr, "0");
      rc = CACHE_NOCON;
      goto isc_open_exit;
   }

   if (!pcon->p_isc_so->loaded) {
      rc = isc_load_library(pcon);
      if (rc != CACHE_SUCCESS) {
         goto isc_open_exit;
      }
   }

   rc = pcon->p_isc_so->p_CacheSetDir(pcon->path);

   if (!isc_authenticate(pcon)) {
      pcon->error_code = error_code;
      strcpy((char *) pcon->output_val.svalue.buf_addr, "0");
      rc = CACHE_NOCON;
   }
   else {
      strcpy((char *) pcon->output_val.svalue.buf_addr, "1");
      pcon->p_isc_so->loaded = 2;
      rc = CACHE_SUCCESS;
   }

isc_open_exit:

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_create_string(&(pcon->output_val.svalue), (void *) pcon->error, DBX_TYPE_STR8);
   }

   return rc;
}


int isc_parse_zv(char *zv, DBXZV * p_sv)
{
   int result;
   double dbx_version;
   char *p, *p1, *p2;

   p_sv->dbx_version = 0;
   p_sv->majorversion = 0;
   p_sv->minorversion = 0;
   p_sv->dbx_build = 0;
   p_sv->vnumber = 0;

   p = strstr(zv, "Cache");
   if (p) {
      p_sv->dbtype = DBX_DBTYPE_CACHE;
   }
   else {
     p = strstr(zv, "IRIS");
      if (p) {
         p_sv->dbtype = DBX_DBTYPE_IRIS;
      }
   }

   result = 0;
   p = zv;
   dbx_version = 0;
   while (*(++ p)) {
      if (*(p - 1) == ' ' && isdigit((int) (*p))) {
         dbx_version = strtod(p, NULL);
         if (*(p + 1) == '.' && dbx_version >= 1.0 && dbx_version <= 5.2)
            break;
         else if (*(p + 4) == '.' && dbx_version >= 2000.0)
            break;
         dbx_version = 0;
      }
   }

   if (dbx_version > 0) {
      p_sv->dbx_version = dbx_version;
      p_sv->majorversion = (int) strtol(p, NULL, 10);
      p1 = strstr(p, ".");
      if (p1) {
         p_sv->minorversion = (int) strtol(p1 + 1, NULL, 10);
      }
      p2 = strstr(p, "Build ");
      if (p2) {
         p_sv->dbx_build = (int) strtol(p2 + 6, NULL, 10);
      }

      if (p_sv->majorversion >= 2007)
         p_sv->vnumber = (((p_sv->majorversion - 2000) * 100000) + (p_sv->minorversion * 10000) + p_sv->dbx_build);
      else
         p_sv->vnumber = ((p_sv->majorversion * 100000) + (p_sv->minorversion * 10000) + p_sv->dbx_build);

      result = 1;
   }

   return result;
}


int isc_change_namespace(DBXCON *pcon, char *nspace)
{
   int rc, len;
   CACHE_ASTR expr;

   len = (int) strlen(nspace);
   if (len == 0 || len > 64) {
      return CACHE_ERNAMSP;
   }
   if (pcon->p_isc_so->p_CacheExecuteA == NULL) {
      return CACHE_ERNAMSP;
   }

   T_SPRINTF((char *) expr.str, _dbxso(expr.str), "ZN \"%s\"", nspace);
   expr.len = (unsigned short) strlen((char *) expr.str);

   rc = pcon->p_isc_so->p_CacheExecuteA(&expr);

   return rc;
}


int isc_get_namespace(DBXCON *pcon, char *nspace, int nspace_len)
{
   int rc;
   CACHE_ASTR retval;
   CACHE_ASTR expr;

   *nspace = '\0';
   strcpy((char *) expr.str, "$Namespace");
   expr.len = (unsigned short) strlen((char *) expr.str);

   rc = pcon->p_isc_so->p_CacheEvalA(&expr);

   if (rc == CACHE_SUCCESS) {
      retval.len = 256;
      rc = pcon->p_isc_so->p_CacheConvert(CACHE_ASTRING, &retval);

      strncpy((char *) nspace, (char *) retval.str, retval.len);
      nspace[retval.len] = '\0';
   }
   return rc;
}


int isc_cleanup(DBXCON *pcon)
{
   int n;

   for (n = 0; n < DBX_MAXARGS; n ++) {
      if (pcon->args[n].cvalue.pstr) {
         /* printf("\r\ncache_cleanup %d &zstr=%p; pstr=%p;", n, &(pcon->cargs[n].zstr), pcon->cargs[n].pstr); */
         pcon->p_isc_so->p_CacheExStrKill(&(pcon->args[n].cvalue.zstr));
         pcon->args[n].cvalue.pstr = NULL;
      }
   }
   return 1;
}


int isc_pop_value(DBXCON *pcon, DBXVAL *value, int required_type)
{
   int rc, ex, ctype;
   unsigned int n, max, len;
   char *pstr8, *p8, *outstr8;
   CACHE_EXSTR zstr;

   ex = 0;
   zstr.len = 0;
   zstr.str.ch = NULL;
   outstr8 = NULL;
   ctype = CACHE_ASTRING;

   if (pcon->p_isc_so->p_CacheType) {
      ctype = pcon->p_isc_so->p_CacheType();

      if (ctype == CACHE_OREF) {
         rc = pcon->p_isc_so->p_CachePopOref(&(value->num.oref));

         value->type = DBX_TYPE_OREF;
         T_SPRINTF((char *) value->svalue.buf_addr, value->svalue.len_alloc, "%d", value->num.oref);
         value->svalue.len_alloc = (int) strlen((char *) value->svalue.buf_addr);
         return rc;
      }
      else if (ctype == CACHE_INT) {
         value->type = DBX_TYPE_INT;
         rc = pcon->p_isc_so->p_CachePopInt(&(value->num.int32));
         sprintf(value->svalue.buf_addr, "%d", value->num.int32);
         value->svalue.len_used = (int) strlen(value->svalue.buf_addr);
         return rc;
      }
   }
   else {
      ctype = CACHE_ASTRING;
   }

   if (ex) {
      rc = pcon->p_isc_so->p_CachePopExStr(&zstr);
      len = zstr.len;
      outstr8 = (char *) zstr.str.ch;
   }
   else {
      rc = pcon->p_isc_so->p_CachePopStr((int *) &len, (Callin_char_t **) &outstr8);
   }

   if (rc != CACHE_SUCCESS) {
      strcpy(pcon->error, "Invalid String <WIDE CHAR> - Check character set");
      pcon->error_code = CACHE_BAD_STRING;
      if (ex) {
         pcon->p_isc_so->p_CacheExStrKill(&zstr);
      }
      goto isc_pop_value_exit;
   }

   max = 0;
   if (value->svalue.len_alloc > 8) {
      max = (value->svalue.len_alloc - 2);
   }

   pstr8 = (char *) value->svalue.buf_addr;
   if (len >= max) {
      p8 = (char *) dbx_malloc(sizeof(char) * (len + 2), 301);
      if (p8) {
         if (value->svalue.buf_addr)
            dbx_free((void *) value->svalue.buf_addr, 301);
         value->svalue.buf_addr = (char *) p8;
         pstr8 = (char *) value->svalue.buf_addr;
         max = len;
      }
   }
   for (n = 0; n < len; n ++) {
      if (n > max)
         break;
      pstr8[n] = (char) outstr8[n];
   }
   pstr8[n] = '\0';

   value->svalue.len_used = n;

   if (ex) {
      pcon->p_isc_so->p_CacheExStrKill(&zstr);
   }

isc_pop_value_exit:

   return rc;
}


char * isc_callin_message(DBXCON *pcon, int error_code)
{
   char *p;

   switch (error_code) {
      case CACHE_SUCCESS:
         p = (char *) "Operation completed successfully!";
         break;
      case CACHE_ACCESSDENIED:
         p = (char *) "<ACCESS DENIED>Authentication has failed. Check the audit log for the real authentication error.";
         break;
      case CACHE_ALREADYCON:
         p = (char *) "Connection already existed.";
         break;
      case CACHE_CHANGEPASSWORD:
         p = (char *) "Password change required.";
         break;
      case CACHE_CONBROKEN:
         p = (char *) "<NOCON>Connection was broken by the server. Check arguments for validity.";
         break;
      case CACHE_FAILURE:
         p = (char *) "An unexpected error has occurred.";
         break;
      case CACHE_STRTOOLONG:
         p = (char *) "<MAXSTRING>String is too long.";
         break;
      case CACHE_NOCON:
         p = (char *) "<NOCON>No connection has been established.";
         break;
      case CACHE_ERSYSTEM:
         p = (char *) "<SYSTEM>Either the server generated a <SYSTEM> error, or callin detected an internal data inconsistency.";
         break;
      case CACHE_ERARGSTACK:
         p = (char *) "<STACK>Argument stack overflow.";
         break;
      case CACHE_ERSTRINGSTACK:
         p = (char *) "<STRINGSTACK>String stack overflow.";
         break;
      case CACHE_ERPROTECT:
         p = (char *) "<PROTECT>Protection violation.";
         break;
      case CACHE_ERUNDEF:
         p = (char *) "<UNDEFINED>Global node is undefined";
         break;
      case CACHE_ERUNIMPLEMENTED:
         p = (char *) "<UNIMPLEMENTED>String is undefined OR feature is not implemented.";
         break;
      case CACHE_ERSUBSCR:
         p = (char *) "<SUBSCRIPT>Subscript error in Global node (subscript null/empty or too long)";
         break;
      case CACHE_ERNOROUTINE:
         p = (char *) "<NOROUTINE>Routine does not exist";
         break;
      case CACHE_ERNOLINE:
         p = (char *) "<NOLINE>Function does not exist in routine";
         break;
      case CACHE_ERPARAMETER:
         p = (char *) "<PARAMETER>Function arguments error";
         break;
      case CACHE_BAD_GLOBAL:
         p = (char *) "<SYNTAX>Invalid global name";
         break;
      case CACHE_BAD_NAMESPACE:
         p = (char *) "<NAMESPACE>Invalid NameSpace name";
         break;
      case CACHE_BAD_FUNCTION:
         p = (char *) "<FUNCTION>Invalid function name";
         break;
      case CACHE_ERNOCLASS:
         p = (char *) "<CLASS DOES NOT EXIST>Class does not exist";
         break;
      case CACHE_ERBADOREF:
         p = (char *) "<INVALID OREF>Invalid Object Reference";
         break;
      case CACHE_ERNOMETHOD:
         p = (char *) "<METHOD DOES NOT EXIST>Method does not exist";
         break;
      case CACHE_ERNOPROPERTY:
         p = (char *) "<PROPERTY DOES NOT EXIST>Property does not exist";
         break;
      case CACHE_ETIMEOUT:
         p = (char *) "<TIMEOUT>Operation timed out";
         break;
      case CACHE_ERWIDECHAR:
         p = (char *) "<WIDE CHAR>Invalid String - Check character set";
         break;
      case CACHE_BAD_STRING:
         p = (char *) "<SYNTAX>Invalid string";
         break;
      case CACHE_ERNAMSP:
         p = (char *) "<NAMESPACE>Invalid Namespace";
         break;
      default:
         p = (char *)  "Server Error";
         break;
   }
   return p;
}


int isc_error_message(DBXCON *pcon, int error_code)
{
   int size, size1, len;
   CACHE_ASTR *pcerror;

   size = DBX_ERROR_SIZE;

   if (pcon) {
      if (error_code < 0) {
         pcon->error_code = 900 + (error_code * -1);
      }
      else {
         pcon->error_code = error_code;
      }
   }

   if (error_code == CACHE_BAD_GLOBAL) {
      if (pcon->error[0])
         return 0;
   }
   if (error_code == CACHE_BAD_FUNCTION) {
      if (pcon->error[0])
         return 0;
   }
   if (error_code == CACHE_BAD_STRING) {
      if (pcon->error[0])
         return 0;
   }
   pcon->error[0] = '\0';

   size1 = size;

      pcerror = (CACHE_ASTR *) dbx_malloc(sizeof(CACHE_ASTR), 801);
      if (pcerror) {
         pcerror->str[0] = '\0';
         pcerror->len = 50;
         pcon->p_isc_so->p_CacheErrxlateA(error_code, pcerror);
         pcerror->str[50] = '\0';

         if (pcerror->len > 0) {
            len = pcerror->len;
            if (len >= DBX_ERROR_SIZE) {
               len = DBX_ERROR_SIZE - 1;
            }
            T_STRNCPY(pcon->error, _dbxso(pcon->error), (char *) pcerror->str, len);
            pcon->error[len] = '\0';
         }
         dbx_free((void *) pcerror, 801);
         size1 -= (int) strlen(pcon->error);
      }
 
   {
      char *p;
      p = (char *) isc_callin_message(pcon, error_code);
      if (*p == '<') {
         p = strstr(p, ">");
         if (p) {
            p ++;
         }
      }
      if (p)
         T_STRNCAT(pcon->error, _dbxso(pcon->error), p, size1 - 1);
   }

   pcon->error[size - 1] = '\0';

   return 0;
}


int isc_error(DBXCON *pcon, int error_code)
{
   int size, size1;

   size = DBX_ERROR_SIZE;
   size1 = size - (int) strlen(pcon->error);

   switch (error_code) {
      case CACHE_CONBROKEN:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NOCON>", size1 - 1);
         break;
      case CACHE_NOCON:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NOCON>", size1 - 1);
         break;
      case CACHE_ERSYSTEM:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<SYSTEM>", size1 - 1);
         break;
      case CACHE_ERARGSTACK:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<STACK>", size1 - 1);
         break;
      case CACHE_STRTOOLONG:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<MAXSTRING>", size1 - 1);
         break;
      case CACHE_ERMXSTR:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<MAXSTRING>", size1 - 1);
         break;
      case CACHE_ERSTRINGSTACK:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<STRINGSTACK>", size1 - 1);
         break;
      case CACHE_ERPROTECT:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<PROTECT>", size1 - 1);
         break;
      case CACHE_ERUNDEF:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<UNDEFINED>", size1 - 1);
         break;
      case CACHE_ERUNIMPLEMENTED:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<UNIMPLEMENTED>", size1 - 1);
         break;
      case CACHE_ERSUBSCR:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<SUBSCRIPT>", size1 - 1);
         break;
      case CACHE_ERNOROUTINE:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NOROUTINE>", size1 - 1);
         break;
      case CACHE_ERNOLINE:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NOLINE>", size1 - 1);
         break;
      case CACHE_ERPARAMETER:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<PARAMETER>", size1 - 1);
         break;
      case CACHE_BAD_GLOBAL:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<SYNTAX>", size1 - 1);
         break;
      case CACHE_BAD_NAMESPACE:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NAMESPACE>", size1 - 1);
         break;
      case CACHE_BAD_FUNCTION:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<FUNCTION>", size1 - 1);
         break;
      case CACHE_ERNOCLASS:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<CLASS DOES NOT EXIST>", size1 - 1);
         break;
      case CACHE_ERBADOREF:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<INVALID OREF>", size1 - 1);
         break;
      case CACHE_ERNOMETHOD:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<METHOD DOES NOT EXIST>", size1 - 1);
         break;
      case CACHE_ERNOPROPERTY:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<PROPERTY DOES NOT EXIST>", size1 - 1);
         break;
      case CACHE_ETIMEOUT:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<TIMEOUT>", size1 - 1);
         break;
      case CACHE_BAD_STRING:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<SYNTAX>", size1 - 1);
         break;
      case CACHE_ERNAMSP:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<NAMESPACE>", size1 - 1);
         break;
      case CACHE_ACCESSDENIED:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<ACCESS DENIED>", size1 - 1);
         break;
      default:
         T_STRNCAT(pcon->error, _dbxso(pcon->error), "<SYNTAX>", size1 - 1);
         break;
   }

   return 1;
}


int ydb_load_library(DBXCON *pcon)
{
   int n, len, result;
   char primlib[DBX_ERROR_SIZE], primerr[DBX_ERROR_SIZE];
   char verfile[256], fun[64];
   char *libnam[16];

   strcpy(pcon->p_ydb_so->libdir, pcon->path);
   strcpy(pcon->p_ydb_so->funprfx, "ydb");
   strcpy(pcon->p_ydb_so->dbname, "YottaDB");

   strcpy(verfile, pcon->path);
   len = (int) strlen(pcon->p_ydb_so->libdir);
   if (pcon->p_ydb_so->libdir[len - 1] != '/' && pcon->p_ydb_so->libdir[len - 1] != '\\') {
      pcon->p_ydb_so->libdir[len] = '/';
      len ++;
   }

   n = 0;
#if defined(_WIN32)
   libnam[n ++] = (char *) DBX_YDB_DLL;
#else
#if defined(MACOSX)
   libnam[n ++] = (char *) DBX_YDB_DYLIB;
   libnam[n ++] = (char *) DBX_YDB_SO;
#else
   libnam[n ++] = (char *) DBX_YDB_SO;
   libnam[n ++] = (char *) DBX_YDB_DYLIB;
#endif
#endif

   libnam[n ++] = NULL;
   strcpy(pcon->p_ydb_so->libnam, pcon->p_ydb_so->libdir);
   len = (int) strlen(pcon->p_ydb_so->libnam);

   for (n = 0; libnam[n]; n ++) {
      strcpy(pcon->p_ydb_so->libnam + len, libnam[n]);
      if (!n) {
         strcpy(primlib, pcon->p_ydb_so->libnam);
      }

      pcon->p_ydb_so->p_library = dbx_dso_load(pcon->p_ydb_so->libnam);
      if (pcon->p_ydb_so->p_library) {
         break;
      }

      if (!n) {
         int len1, len2;
         char *p;
#if defined(_WIN32)
         DWORD errorcode;
         LPVOID lpMsgBuf;

         lpMsgBuf = NULL;
         errorcode = GetLastError();
         sprintf(pcon->error, "Error loading %s Library: %s; Error Code : %ld",  pcon->p_ydb_so->dbname, primlib, errorcode);
         len2 = (int) strlen(pcon->error);
         len1 = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorcode,
                        /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPTSTR) &lpMsgBuf,
                        0,
                        NULL 
                        );
         if (lpMsgBuf && len1 > 0 && (DBX_ERROR_SIZE - len2) > 30) {
            strncpy(primerr, (const char *) lpMsgBuf, DBX_ERROR_SIZE - 1);
            p = strstr(primerr, "\r\n");
            if (p)
               *p = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            p = strstr(primerr, "%1");
            if (p) {
               *p = 'I';
               *(p + 1) = 't';
            }
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
         if (lpMsgBuf)
            LocalFree(lpMsgBuf);
#else
         p = (char *) dlerror();
         sprintf(primerr, "Cannot load %s library: Error Code: %d", pcon->p_ydb_so->dbname, errno);
         len2 = strlen(pcon->error);
         if (p) {
            strncpy(primerr, p, DBX_ERROR_SIZE - 1);
            primerr[DBX_ERROR_SIZE - 1] = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
#endif
      }
   }

   if (!pcon->p_ydb_so->p_library) {
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_init", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_init = (int (*) (void)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_init) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_exit", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_exit = (int (*) (void)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_exit) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_malloc", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_malloc = (int (*) (size_t)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_malloc) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_free", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_free = (int (*) (void *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_free) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_data_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_data_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, unsigned int *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_data_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_delete_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_delete_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_delete_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_set_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_set_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_set_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_get_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_get_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_get_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_subscript_next_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_subscript_next_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_subscript_next_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_subscript_previous_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_subscript_previous_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_subscript_previous_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_node_next_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_node_next_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_node_next_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_node_previous_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_node_previous_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_node_previous_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_incr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_incr_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_incr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_ci", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_ci = (int (*) (const char *, ...)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_ci) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_cip", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_cip = (int (*) (ci_name_descriptor *, ...)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_cip) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_lock_incr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_lock_incr_s = (int (*) (unsigned long long, ydb_buffer_t *, int, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_lock_incr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_lock_decr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_lock_decr_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_lock_decr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_zstatus", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_zstatus = (void (*) (ydb_char_t *, ydb_long_t)) dbx_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);

   pcon->p_ydb_so->loaded = 1;

ydb_load_library_exit:

   if (pcon->error[0]) {
      pcon->p_ydb_so->loaded = 0;
      pcon->error_code = 1009;
      strcpy((char *) pcon->output_val.svalue.buf_addr, "0");
      result = CACHE_NOCON;

      return result;
   }

   return CACHE_SUCCESS;
}


int ydb_open(DBXCON *pcon)
{
   int rc, result;
   char buffer[256], buffer1[256];
   ydb_buffer_t zv, data;

   if (!pcon->p_ydb_so) {
      pcon->p_ydb_so = (DBXYDBSO *) dbx_malloc(sizeof(DBXYDBSO), 0);
      if (!pcon->p_ydb_so) {
         T_STRCPY(pcon->error, _dbxso(pcon->error), "No Memory");
         pcon->error_code = 1009; 
         result = CACHE_NOCON;
         return result;
      }
      memset((void *) pcon->p_ydb_so, 0, sizeof(DBXYDBSO));
      pcon->p_ydb_so->loaded = 0;
      pcon->p_ydb_so->no_connections = 0;
      pcon->p_ydb_so->multiple_connections = 0;
      pcon->p_zv = &(pcon->p_ydb_so->zv);
   }

   if (pcon->p_ydb_so->loaded == 2) {
      strcpy(pcon->error, "Cannot create multiple connections to the database");
      pcon->error_code = 1009; 
      strcpy((char *) pcon->output_val.svalue.buf_addr, "0");
      rc = CACHE_NOCON;
      goto ydb_open_exit;
   }

   if (!pcon->p_ydb_so->loaded) {
      rc = ydb_load_library(pcon);
      if (rc != CACHE_SUCCESS) {
         goto ydb_open_exit;
      }
   }

   rc = pcon->p_ydb_so->p_ydb_init();

/*
   YDB_COPY_STRING_TO_BUFFER("$zstatus", pglobal, rc);
   rc = pcon->p_ydb_so->p_ydb_get_s(pglobal, 0, NULL, pdata);
   if (pdata->len_used >= 0) {
      pdata->buf_addr[pdata->len_used] = '\0';
   }
   printf("\r\np_ydb_get_s($zstatus)=%d; len_used=%d; s=%s\r\n", rc, pdata->len_used, pdata->buf_addr);
*/


   /* v1.3.8 */
   strcpy(buffer, "$zyrelease");
   zv.buf_addr = buffer;
   zv.len_used = (int) strlen(buffer);
   zv.len_alloc = 255;

   data.buf_addr = buffer1;
   data.len_used = 0;
   data.len_alloc = 255;

   rc = pcon->p_ydb_so->p_ydb_get_s(&zv, 0, NULL, &data);

   if (rc != CACHE_SUCCESS) {
      strcpy(buffer, "$zversion");
      zv.buf_addr = buffer;
      zv.len_used = (int) strlen(buffer);
      zv.len_alloc = 255;

      data.buf_addr = buffer1;
      data.len_used = 0;
      data.len_alloc = 255;

      rc = pcon->p_ydb_so->p_ydb_get_s(&zv, 0, NULL, &data);
   }

   if (data.len_used > 0) {
      data.buf_addr[data.len_used] = '\0';
   }

   if (rc == CACHE_SUCCESS) {
      ydb_parse_zv(data.buf_addr, pcon->p_zv);
      if (pcon->p_zv->dbx_build)
         sprintf(pcon->p_zv->version, "%d.%d.b%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->dbx_build);
      else
         sprintf(pcon->p_zv->version, "%d.%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion);
   }

ydb_open_exit:

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_create_string(&(pcon->output_val.svalue), (void *) pcon->error, DBX_TYPE_STR8);
   }

   return rc;
}


int ydb_parse_zv(char *zv, DBXZV * p_ydb_sv)
{
   int result;
   double dbx_version;
   char *p, *p1, *p2;

   p_ydb_sv->dbx_version = 0;
   p_ydb_sv->majorversion = 0;
   p_ydb_sv->minorversion = 0;
   p_ydb_sv->dbx_build = 0;
   p_ydb_sv->vnumber = 0;

   result = 0;
/*
   $zversion
   GT.M V6.3-007 Linux x86_64
   $zyrelease
   YottaDB r1.28 Linux x86_64
*/

   p_ydb_sv->dbtype = DBX_DBTYPE_YOTTADB;

   p = zv;
   dbx_version = 0;
   while (*(++ p)) {
      if (*(p - 1) == 'V' && isdigit((int) (*p))) {
         dbx_version = strtod(p, NULL);
         break;
      }
      if (*(p - 1) == 'r' && isdigit((int) (*p))) {
         dbx_version = strtod(p, NULL);
         break;
      }
   }

   if (dbx_version > 0) {
      p_ydb_sv->dbx_version = dbx_version;
      p_ydb_sv->majorversion = (int) strtol(p, NULL, 10);
      p1 = strstr(p, ".");
      if (p1) {
         p_ydb_sv->minorversion = (int) strtol(p1 + 1, NULL, 10);
      }
      p2 = strstr(p, "-");
      if (p2) {
         p_ydb_sv->dbx_build = (int) strtol(p2 + 1, NULL, 10);
      }

      p_ydb_sv->vnumber = ((p_ydb_sv->majorversion * 100000) + (p_ydb_sv->minorversion * 10000) + p_ydb_sv->dbx_build);

      result = 1;
   }

   return result;
}


int ydb_change_namespace(DBXCON *pcon, char *nspace)
{
   int rc;
   char buffer[256];
   ydb_buffer_t zg, data;

   strcpy(buffer, "$zg");
   zg.buf_addr = buffer;
   zg.len_used = (int) strlen(buffer);
   zg.len_alloc = 255;

   data.buf_addr = nspace;
   data.len_used = 0;
   data.len_alloc = (int) strlen(nspace);

   rc = pcon->p_ydb_so->p_ydb_set_s(&zg, 0, NULL, &data);
 
   return rc;
}


int ydb_get_namespace(DBXCON *pcon, char *nspace, int nspace_len)
{
   int rc;
   char buffer[256];
   ydb_buffer_t zg, data;

   *nspace = '\0';
   strcpy(buffer, "$zg");
   zg.buf_addr = buffer;
   zg.len_used = (int) strlen(buffer);
   zg.len_alloc = 255;

   data.buf_addr = nspace;
   data.len_used = 0;
   data.len_alloc = nspace_len;

   rc = pcon->p_ydb_so->p_ydb_get_s(&zg, 0, NULL, &data);
   if (rc == YDB_OK) {
      nspace[data.len_used] = '\0';
   }

   return rc;
}


int ydb_error_message(DBXCON *pcon, int error_code)
{
   int rc;
   char buffer[256], buffer1[256];
   ydb_buffer_t zstatus, data;

/*
   strcpy(buffer, "$zg");
   zstatus.buf_addr = buffer;
   zstatus.len_used = (int) strlen(buffer);
   zstatus.len_alloc = 255;

   data.buf_addr = buffer1;
   data.len_used = 0;
   data.len_alloc = 255;

   rc = pcon->p_ydb_so->p_ydb_get_s(&zstatus, 0, NULL, &data);
*/

   strcpy(buffer, "$zstatus");
   zstatus.buf_addr = buffer;
   zstatus.len_used = (int) strlen(buffer);
   zstatus.len_alloc = 255;

   data.buf_addr = buffer1;
   data.len_used = 0;
   data.len_alloc = 255;

   rc = pcon->p_ydb_so->p_ydb_get_s(&zstatus, 0, NULL, &data);

   if (data.len_used > 0) {
      data.buf_addr[data.len_used] = '\0';
   }

   return rc;
}


int ydb_function(DBXCON *pcon, DBXFUN *pfun)
{
   int rc;
   ydb_string_t out, in1, in2, in3;

   pcon->output_val.svalue.len_used = 0;
   pcon->output_val.svalue.buf_addr[0] = '\0';

   out.address = (char *) pcon->output_val.svalue.buf_addr;
   out.length = (unsigned long) pcon->output_val.svalue.len_alloc;

   switch (pcon->argc) {
      case 1:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &out);
         break;
      case 2:
         in1.address = (char *) pcon->args[1].svalue.buf_addr;
         in1.length = (unsigned long) pcon->args[1].svalue.len_used;;
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &out, &in1);
         break;
      case 3:
         in1.address = (char *) pcon->args[1].svalue.buf_addr;
         in1.length = (unsigned long) pcon->args[1].svalue.len_used;;
         in2.address = (char *) pcon->args[2].svalue.buf_addr;
         in2.length = (unsigned long) pcon->args[2].svalue.len_used;;
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &out, &in1, &in2);
         break;
      case 4:
         in1.address = (char *) pcon->args[1].svalue.buf_addr;
         in1.length = (unsigned long) pcon->args[1].svalue.len_used;;
         in2.address = (char *) pcon->args[2].svalue.buf_addr;
         in2.length = (unsigned long) pcon->args[2].svalue.len_used;;
         in3.address = (char *) pcon->args[3].svalue.buf_addr;
         in3.length = (unsigned long) pcon->args[3].svalue.len_used;
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &out, &in1, &in2, &in3);
         break;
      default:
         rc = CACHE_SUCCESS;
         break;
   }

   pcon->output_val.svalue.len_used = (int) strlen(pcon->output_val.svalue.buf_addr);

   return rc;
}


int dbx_version(DBXCON *pcon)
{
   char buffer[256];

   T_SPRINTF((char *) buffer, _dbxso(buffer), "mg-dbx: version: %s; ABI: %d", DBX_VERSION, NODE_MODULE_VERSION);

   if (pcon->p_zv && pcon->p_zv->version[0]) {
      if (pcon->p_zv->dbtype == DBX_DBTYPE_CACHE)
         T_STRCAT((char *) buffer, _dbxso(buffer), "; Cache version: ");
      else if (pcon->p_zv->dbtype == DBX_DBTYPE_IRIS)
         T_STRCAT((char *) buffer, _dbxso(buffer), "; IRIS version: ");
      else if (pcon->p_zv->dbtype == DBX_DBTYPE_YOTTADB)
         T_STRCAT((char *) buffer, _dbxso(buffer), "; YottaDB version: ");
      T_STRCAT((char *) buffer, _dbxso(buffer), pcon->p_zv->version);
   }

   dbx_create_string(&(pcon->output_val.svalue), (void *) buffer, DBX_TYPE_STR8);

   return 0;
}


int dbx_open(DBXCON *pcon)
{
   int rc;

   if (!pcon->dbtype) {
      strcpy(pcon->error, "Unable to determine the database type");
      rc = CACHE_NOCON;
      goto dbx_open_exit;
   }

   if (!pcon->path[0]) {
      strcpy(pcon->error, "Unable to determine the path to the database installation");
      rc = CACHE_NOCON;
      goto dbx_open_exit;
   }

   dbx_enter_critical_section((void *) &dbx_async_mutex);
   if (pcon->dbtype != DBX_DBTYPE_YOTTADB && p_isc_so_global) {
      rc = CACHE_SUCCESS;
      pcon->p_ydb_so = NULL;
      pcon->p_isc_so = p_isc_so_global;
      pcon->p_isc_so->no_connections ++;
      pcon->p_isc_so->multiple_connections ++;
      pcon->p_zv = &(p_isc_so_global->zv);
      dbx_leave_critical_section((void *) &dbx_async_mutex);
      return rc;
   }
   if (pcon->dbtype == DBX_DBTYPE_YOTTADB && p_ydb_so_global) {
      rc = CACHE_SUCCESS;
      pcon->p_isc_so = NULL;
      pcon->p_ydb_so = p_ydb_so_global;
      pcon->p_ydb_so->no_connections ++;
      pcon->p_ydb_so->multiple_connections ++;
      pcon->p_zv = &(p_ydb_so_global->zv);
      dbx_leave_critical_section((void *) &dbx_async_mutex);
      return rc;
   }

   if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
      rc = isc_open(pcon);
      pcon->p_isc_so->no_connections ++;
      p_isc_so_global = pcon->p_isc_so;
   }
   else {
      rc = ydb_open(pcon);
      pcon->p_ydb_so->no_connections ++;
      p_ydb_so_global = pcon->p_ydb_so;
   }

   dbx_pool_thread_init(pcon, 1);

   dbx_leave_critical_section((void *) &dbx_async_mutex);

dbx_open_exit:

   return rc;
}


int dbx_do_nothing(DBXCON *pcon)
{
   /* Function called asynchronously */

   return 0;
}


int dbx_close(DBXCON *pcon)
{
   int no_connections;

   no_connections = 0;

   dbx_enter_critical_section((void *) &dbx_async_mutex);
   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->p_ydb_so) {
         pcon->p_ydb_so->no_connections --;
         no_connections =  pcon->p_ydb_so->no_connections;
      }
   }
   else {
      if (pcon->p_isc_so) {
         pcon->p_isc_so->no_connections --;
         no_connections =  pcon->p_isc_so->no_connections;
      }
   }
   dbx_leave_critical_section((void *) &dbx_async_mutex);

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->p_ydb_so && no_connections == 0) {
         if (pcon->p_ydb_so->loaded) {
            pcon->p_ydb_so->p_ydb_exit();
/*
            printf("\r\np_ydb_exit=%d\r\n", rc);
            dbx_dso_unload(pcon->p_ydb_so->p_library); 
*/
            pcon->p_ydb_so->p_library = NULL;
            pcon->p_ydb_so->loaded = 0;
         }

         strcpy(pcon->error, "");
         dbx_create_string(&(pcon->output_val.svalue), (void *) "1", DBX_TYPE_STR);

         strcpy(pcon->p_ydb_so->libdir, "");
         strcpy(pcon->p_ydb_so->libnam, "");
      }
   }
   else {
      if (pcon->p_isc_so && no_connections == 0 && pcon->p_isc_so->multiple_connections == 0) {

         if (pcon->p_isc_so->loaded) {

            /* pcon->p_isc_so->p_CacheEnd(); */
            dbx_dso_unload(pcon->p_isc_so->p_library);
            pcon->p_isc_so->p_library = NULL;
            pcon->p_isc_so->loaded = 0;
         }

         T_STRCPY(pcon->error, _dbxso(pcon->error), "");
         dbx_create_string(&(pcon->output_val.svalue), (void *) "1", DBX_TYPE_STR8);

         T_STRCPY(pcon->p_isc_so->libdir, _dbxso(pcon->p_isc_so->libdir), "");
         T_STRCPY(pcon->p_isc_so->libnam, _dbxso(pcon->p_isc_so->libnam), "");
      }
   }

   T_STRCPY(pcon->p_zv->version, _dbxso(pcon->p_zv->version), "");

   T_STRCPY(pcon->path, _dbxso(pcon->path), "");
   T_STRCPY(pcon->username, _dbxso(pcon->username), "");
   T_STRCPY(pcon->password, _dbxso(pcon->password), "");
   T_STRCPY(pcon->nspace, _dbxso(pcon->nspace), "");
   T_STRCPY(pcon->input_device, _dbxso(pcon->input_device), "");
   T_STRCPY(pcon->output_device, _dbxso(pcon->output_device), "");

   return 0;
}


int dbx_namespace(DBXCON *pcon)
{
   int rc;
   char nspace[256];

   DBX_DB_LOCK(rc, 0);

   *nspace = '\0';
   if (pcon->argc > 0) {
      strncpy(nspace, pcon->args[0].svalue.buf_addr, pcon->args[0].svalue.len_used);
      nspace[pcon->args[0].svalue.len_used] = '\0';
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->argc > 0) {
         rc = ydb_change_namespace(pcon, nspace);
      }
      rc = ydb_get_namespace(pcon, nspace, 256);
   }
   else {
      if (pcon->argc > 0) {
         rc = isc_change_namespace(pcon, nspace);
      }
      rc = isc_get_namespace(pcon, nspace, 256);
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) nspace, DBX_TYPE_STR8);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_reference(DBXCON *pcon, int n)
{
   int rc;
   unsigned int ne;

/*
{
   char buffer[256];
   strcpy(buffer, "");
   if (pcon->args[n].svalue.len_used > 0) {
      strncpy(buffer,  pcon->args[n].svalue.buf_addr,  pcon->args[n].svalue.len_used);
      buffer[pcon->args[n].svalue.len_used] = '\0';
   }
   printf("\r\n >>>>>>> n=%d; type=%d; int32=%d; str=%s", n, pcon->args[n].type,  pcon->args[n].num.int32, buffer);
}
*/

   if (pcon->args[n].type == DBX_TYPE_INT) {
      rc = pcon->p_isc_so->p_CachePushInt((int) pcon->args[n].num.int32);
   }
   else if (pcon->args[n].type == DBX_TYPE_DOUBLE) {
      rc = pcon->p_isc_so->p_CachePushDbl(pcon->args[n].num.real);
      /* rc = pcon->p_isc_so->p_CachePushIEEEDbl(pcon->args[n].num.real); */
   }
   else {
      if (pcon->args[n].svalue.len_used < CACHE_MAXSTRLEN) {
         rc = pcon->p_isc_so->p_CachePushStr(pcon->args[n].svalue.len_used, (Callin_char_t *) pcon->args[n].svalue.buf_addr);
      }
      else {
         pcon->args[n].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pcon->args[n].cvalue.zstr), pcon->args[n].svalue.len_used + 1);
         for (ne = 0; ne < pcon->args[n].svalue.len_used; ne ++) {
            pcon->args[n].cvalue.zstr.str.ch[ne] = (char) pcon->args[n].svalue.buf_addr[ne];
         }
         pcon->args[n].cvalue.zstr.str.ch[ne] = (char) 0;
         pcon->args[n].cvalue.zstr.len = pcon->args[n].svalue.len_used;

         rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pcon->args[n].cvalue.zstr));
      }
   }

   return rc;
}


int dbx_global_reference(DBXCON *pcon)
{
   int n, rc;

   rc = CACHE_SUCCESS;
   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = CACHE_SUCCESS;
      return rc;
   }

   for (n = 0; n < pcon->argc; n ++) {

      if (n == 0) {
         if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
            if (pcon->lock) {
               rc = pcon->p_isc_so->p_CachePushLock((int) pcon->args[n].svalue.len_used, (Callin_char_t *) pcon->args[n].svalue.buf_addr);
            }
            else {
               if (pcon->args[n].svalue.buf_addr[0] == '^')
                  rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[n].svalue.len_used - 1, (Callin_char_t *) pcon->args[n].svalue.buf_addr + 1);
               else
                  rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[n].svalue.len_used, (Callin_char_t *) pcon->args[n].svalue.buf_addr);
            }
         }
         continue;
      }

      if (pcon->lock == 1) {
         if (n < (pcon->argc - 1)) { /* don't push lock timeout */
            rc = dbx_reference(pcon, n);
         }
      }
      else {
         rc = dbx_reference(pcon, n);
      }
   }
   return rc;
}


int dbx_get(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_get_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_get_s(&(pcon->args[0].svalue), pcon->argc - 2, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalGet(pcon->argc - 1, 0); /* 1 for no 'undefined' */
   }

   if (rc == CACHE_ERUNDEF) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) "", DBX_TYPE_STR8);
   }
   else if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_get_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_set(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_set_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_set_s(&(pcon->args[0].svalue), pcon->argc - 2, &pcon->yargs[0], &(pcon->args[pcon->argc - 1].svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalSet(pcon->argc - 2);
   }

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_set_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_defined(DBXCON *pcon)
{
   int rc, n;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_defined_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_data_s(&(pcon->args[0].svalue), pcon->argc - 1, &pcon->yargs[0], (unsigned int *) &n);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalData(pcon->argc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->p_zv->dbtype != DBX_DBTYPE_YOTTADB) {
         pcon->p_isc_so->p_CachePopInt(&n);
      }
      dbx_create_string(&(pcon->output_val.svalue), (void *) &n, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_defined_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}



int dbx_delete(DBXCON *pcon)
{
   int rc, n;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_delete_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_delete_s(&(pcon->args[0].svalue), pcon->argc - 1, &pcon->yargs[0], YDB_DEL_TREE);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalKill(pcon->argc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->p_zv->dbtype != DBX_DBTYPE_YOTTADB) {
         pcon->p_isc_so->p_CachePopInt(&n);
      }
      n = 0;
      dbx_create_string(&(pcon->output_val.svalue), (void *) &n, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_delete_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_next(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_next_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pcon->args[0].svalue), pcon->argc - 1, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->argc - 1, 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_next_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_previous(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_previous_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pcon->args[0].svalue), pcon->argc - 1, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->argc - 1, -1, 0);

   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_previous_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_increment(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 1;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_increment_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_incr_s(&(pcon->args[0].svalue), pcon->cargc - 2, &pcon->yargs[0], &(pcon->args[pcon->cargc - 1].svalue), &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalIncrement(pcon->cargc - 2);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_increment_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_lock(DBXCON *pcon)
{
   int rc, retval, timeout, locktype;
   unsigned long long timeout_nsec;
   char buffer[32];

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 1;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_lock_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      timeout = -1;
      if (pcon->args[pcon->cargc - 1].svalue.len_used < 16) {
         strncpy(buffer, pcon->args[pcon->cargc - 1].svalue.buf_addr, pcon->args[pcon->cargc - 1].svalue.len_used);
         buffer[pcon->args[pcon->cargc - 1].svalue.len_used] = '\0';
         timeout = (int) strtol(buffer, NULL, 10);
      }
      timeout_nsec = 1000000000;

      if (timeout < 0)
         timeout_nsec *= 3600;
      else
         timeout_nsec *= timeout;
      rc = pcon->p_ydb_so->p_ydb_lock_incr_s(timeout_nsec, &(pcon->args[0].svalue), pcon->cargc - 2, &pcon->yargs[0]);
      if (rc == YDB_OK) {
         retval = 1;
         rc = YDB_OK;
      }
      else if (rc == YDB_LOCK_TIMEOUT) {
         retval = 0;
         rc = YDB_OK;
      }
      else {
         retval = 0;
         rc = CACHE_FAILURE;
      }
   }
   else {
      strncpy(buffer, pcon->args[pcon->cargc - 1].svalue.buf_addr, pcon->args[pcon->cargc - 1].svalue.len_used);
      buffer[pcon->args[pcon->cargc - 1].svalue.len_used] = '\0';
      timeout = (int) strtol(buffer, NULL, 10);
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheAcquireLock(pcon->cargc - 2, locktype, timeout, &retval);
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &retval, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_lock_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_unlock(DBXCON *pcon)
{
   int rc, retval;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 2;
   pcon->increment = 0;
   rc = dbx_global_reference(pcon);
   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_unlock_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_lock_decr_s(&(pcon->args[0].svalue), pcon->cargc - 1, &pcon->yargs[0]);
      if (rc == YDB_OK)
         retval = 1;
      else
         retval = 0;
   }
   else {
      int locktype;
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheReleaseLock(pcon->cargc - 1, locktype);
      if (rc == CACHE_SUCCESS)
         retval = 1;
      else
         retval = 0;
   }

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &retval, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_unlock_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_merge(DBXCON *pcon)
{
   int rc, rc1, narg, ne, ex;
   unsigned int n, max, len, netbuf_used;
   unsigned char *netbuf;
   char *outstr8;
   char buffer[256];
   DBXFUN fun, *pfun;
   CACHE_EXSTR zstr;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pfun = &fun;
      dbx_add_block_size(pcon->ibuffer, pcon->ibuffer_used, 0,  DBX_DSORT_EOD, DBX_DTYPE_STR);
      pcon->ibuffer_used += 5;

      netbuf = (pcon->ibuffer - DBX_IBUFFER_OFFSET);
      netbuf_used = (pcon->ibuffer_used + DBX_IBUFFER_OFFSET);
      dbx_add_block_size(netbuf, 0, pcon->ibuffer_used + 10,  0, DBX_CMND_GMERGE);

      if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
         char buf1[32], buf3[32];
         ydb_string_t out, in1, in2, in3;

         pfun->rflag = 0;
         pfun->label = (char *) "ifc_zmgsis";
         pfun->label_len = 10;
         pfun->routine = (char *) "";
         pfun->routine_len = 0;
         pcon->argc = 3;

         out.address = (char *) pcon->ibuffer;
         out.length = (unsigned long) pcon->ibuffer_size;

         strcpy(buf1, "0");
         in1.address = buf1;
         in1.length = 1;

         in2.address = (char *) netbuf;
         in2.length = (unsigned long) netbuf_used;

         strcpy(buf3, "dbx");
         in3.address = buf3;
         in3.length = 3;
         /* rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, pcon->ibuffer, "0", netbuf, "dbx"); */
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &out, &in1, &in2, &in3);

         pcon->ibuffer_used = 0;
         if (rc > 0) {
            pcon->ibuffer_used = rc;
         }
         rc = CACHE_SUCCESS;
      }
      else {
         ex = 1;
         zstr.len = 0;
         zstr.str.ch = NULL;
         outstr8 = NULL;

         pfun->rflag = 0;
         pfun->label = (char *) "ifc";
         pfun->label_len = 3;
         pfun->routine = (char *) "%zmgsis";
         pfun->routine_len = 7;
         pcon->argc = 3;
         rc = pcon->p_isc_so->p_CachePushFunc(&(pfun->rflag), (int) pfun->label_len, (const Callin_char_t *) pfun->label, (int) pfun->routine_len, (const Callin_char_t *) pfun->routine);

         rc1 = 0;
         rc = pcon->p_isc_so->p_CachePushInt(rc1);

         if (netbuf_used < CACHE_MAXSTRLEN) {
            rc = pcon->p_isc_so->p_CachePushStr(netbuf_used, (Callin_char_t *) netbuf);
         }
         else {
            pcon->args[0].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pcon->args[0].cvalue.zstr), netbuf_used + 1);
            for (ne = 0; ne < (int) netbuf_used; ne ++) {
               pcon->args[0].cvalue.zstr.str.ch[ne] = (char) netbuf[ne];
            }
            pcon->args[0].cvalue.zstr.str.ch[ne] = (char) 0;
            pcon->args[0].cvalue.zstr.len = netbuf_used;
            rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pcon->args[0].cvalue.zstr));
         }
         strcpy(buffer, "dbx");
         rc = pcon->p_isc_so->p_CachePushStr(3, (Callin_char_t *) buffer);
         rc = pcon->p_isc_so->p_CacheExtFun(pfun->rflag, pcon->argc);

         if (rc == CACHE_SUCCESS) {
            if (ex) {
               rc = pcon->p_isc_so->p_CachePopExStr(&zstr);
               len = zstr.len;
               outstr8 = (char *) zstr.str.ch;
            }
            else {
               rc = pcon->p_isc_so->p_CachePopStr((int *) &len, (Callin_char_t **) &outstr8);
            }
            max = pcon->ibuffer_size - 1;
            for (n = 0; n < len; n ++) {
               if (n > max)
                  break;
               pcon->ibuffer[n] = (char) outstr8[n];
            }
            pcon->ibuffer[n] = '\0';
            pcon->ibuffer_used = len;

            if (ex) {
               rc1 = pcon->p_isc_so->p_CacheExStrKill(&zstr);
            }
            rc = CACHE_SUCCESS;
         }
         else {
            rc = CACHE_FAILURE;
            strcpy(pcon->error, "InterSystems server error - unable to invoke function");
            goto dbx_merge_exit;
         }
      }
      goto dbx_merge_exit;
   }

   narg = 0;
   for (n = 0; n < (unsigned int) pcon->argc; n ++) {
      if (pcon->args[n].sort == DBX_DSORT_GLOBAL) {
         if (n > 0) {
            rc = pcon->p_isc_so->p_CacheAddGlobalDescriptor(narg);
            if (pcon->args[n].svalue.buf_addr[n] == '^')
               rc = pcon->p_isc_so->p_CacheAddGlobal((int) pcon->args[n].svalue.len_used - 1, (Callin_char_t *) pcon->args[n].svalue.buf_addr + 1);
            else
               rc = pcon->p_isc_so->p_CacheAddGlobal((int) pcon->args[n].svalue.len_used, (Callin_char_t *) pcon->args[n].svalue.buf_addr);
         }
         else {
            if (pcon->args[n].svalue.buf_addr[n] == '^')
               rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[n].svalue.len_used - 1, (Callin_char_t *) pcon->args[n].svalue.buf_addr + 1);
            else
               rc = pcon->p_isc_so->p_CachePushGlobal((int) pcon->args[n].svalue.len_used, (Callin_char_t *) pcon->args[n].svalue.buf_addr);
         }
         narg = 0;
      }
      else {
         rc = dbx_reference(pcon, n);
         narg ++;
         if (rc != CACHE_SUCCESS) {
            break;
         }
      }
   }

   if (rc != CACHE_SUCCESS) {
      goto dbx_merge_exit;
   }

   rc = pcon->p_isc_so->p_CacheAddGlobalDescriptor(narg);
   rc = pcon->p_isc_so->p_CacheMerge();

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_merge_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_function_reference(DBXCON *pcon, DBXFUN *pfun)
{
   int n, rc;

   rc = CACHE_SUCCESS;

   for (n = 0; n < pcon->argc; n ++) {

      if (n == 0) {
         T_STRNCPY(pfun->buffer, _dbxso(pfun->buffer), pcon->args[n].svalue.buf_addr, pcon->args[n].svalue.len_used);
         pfun->buffer[pcon->args[n].svalue.len_used] = '\0';
         pfun->label = pfun->buffer;
         pfun->routine = strstr(pfun->buffer, "^");
         *pfun->routine = '\0';
         pfun->routine ++;
         pfun->label_len = (int) strlen(pfun->label);
         pfun->routine_len = (int) strlen(pfun->routine);

         if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
            break;
         }

         rc = pcon->p_isc_so->p_CachePushFunc(&(pfun->rflag), (int) pfun->label_len, (const Callin_char_t *) pfun->label, (int) pfun->routine_len, (const Callin_char_t *) pfun->routine);
      }
      rc = dbx_reference(pcon, n);
   }
   return rc;
}


int dbx_function(DBXCON *pcon)
{
   int rc;
   DBXFUN fun;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_function_reference(pcon, &fun);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_function_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_function(pcon, &fun);
   }
   else {
      rc = pcon->p_isc_so->p_CacheExtFun(fun.rflag, pcon->argc - 1);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_function_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_class_reference(DBXCON *pcon, int optype)
{
   int n, rc;

   rc = CACHE_SUCCESS;

   for (n = 0; n < pcon->cargc; n ++) {

      if (n == 0) {
         ;
      }
      else if (n == 1) {
         if (optype == 0) { /* classmethod from mclass */
            rc = pcon->p_isc_so->p_CachePushClassMethod((int) pcon->args[0].svalue.len_used, (Callin_char_t *) pcon->args[0].svalue.buf_addr, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr, 1);
         }
         else if (optype == 1) { /* method from mclass */
            rc = pcon->p_isc_so->p_CachePushMethod(pcon->args[0].num.oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr, 1);
         }
         else if (optype == 2) { /* set from mclass */
            rc = pcon->p_isc_so->p_CachePushProperty(pcon->args[0].num.oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr);
         }
         else if (optype == 3) { /* get from mclass */
            rc = pcon->p_isc_so->p_CachePushProperty(pcon->args[0].num.oref, (int) pcon->args[1].svalue.len_used, (Callin_char_t *) pcon->args[1].svalue.buf_addr);
         }
      }
      else {
         rc = dbx_reference(pcon, n);
      }
   }

   return rc;
}


int dbx_classmethod(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_class_reference(pcon, 0);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_classmethod_exit;
   }

   rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pcon->cargc - 2);

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_classmethod_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_method(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_class_reference(pcon, 1);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_method_exit;
   }

   rc = pcon->p_isc_so->p_CacheInvokeMethod(pcon->cargc - 2);

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_method_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_setproperty(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_class_reference(pcon, 2);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_setproperty_exit;
   }

   rc = pcon->p_isc_so->p_CacheSetProperty();

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_setproperty_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_getproperty(DBXCON *pcon)
{
   int rc;

   DBX_DB_LOCK(rc, 0);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = dbx_class_reference(pcon, 3);

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pcon, rc);
      goto dbx_getproperty_exit;
   }

   rc = pcon->p_isc_so->p_CacheGetProperty();

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

dbx_getproperty_exit:

   DBX_DB_UNLOCK(rc);

   isc_cleanup(pcon);

   return 0;
}


int dbx_sql_execute(DBXCON *pcon)
{
   int rc, len, dsort, dtype, cn, no_cols;
   unsigned long offset;
   char label[16], routine[16], params[9], buffer[8];
   DBXFUN fun;

   pcon->psql->no_cols = 0;
   pcon->psql->row_no = 0;
   pcon->psql->sqlcode = 0;
   strcpy(pcon->psql->sqlstate, "00000");

   strcpy(params, "");
   if (pcon->psql->sql_type == DBX_SQL_ISCSQL) {
      strcpy(label, "sqleisc");
   }
   else {
      strcpy(label, "sqlemg");
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      strcpy(label, "sqlemg");
   }

   strcpy(routine, "%zmgsis");
   fun.label = label;
   fun.label_len = (int) strlen(label);
   fun.routine = routine;
   fun.routine_len = (int) strlen(routine);
   pcon->output_val.svalue.len_used = 0;
   pcon->output_val.svalue.buf_addr[0] = '\0';
   pcon->output_val.svalue.buf_addr[1] = '\0';
   pcon->output_val.svalue.buf_addr[2] = '\0';
   pcon->output_val.svalue.buf_addr[3] = '\0';
   pcon->output_val.svalue.buf_addr[4] = '\0';

   DBX_DB_LOCK(rc, 0);

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      ydb_string_t out, in1, in2, in3;

      sprintf(buffer, "%d", pcon->psql->sql_no);

      out.address = (char *) pcon->output_val.svalue.buf_addr;
      out.length = (unsigned long) pcon->output_val.svalue.len_alloc;
      in1.address = (char *) buffer;
      in1.length = (unsigned long) strlen(buffer);
      in2.address = (char *) pcon->psql->sql_script;
      in2.length = (unsigned long) strlen(pcon->psql->sql_script);
      in3.address = (char *) params;
      in3.length = (unsigned long) strlen(params);

      rc = pcon->p_ydb_so->p_ydb_ci(fun.label, &out, &in1, &in2, &in3);

      if (rc == YDB_OK) {
         pcon->output_val.svalue.len_used = (unsigned int) dbx_get_size((unsigned char *) pcon->output_val.svalue.buf_addr);
      }
      else if (pcon->p_ydb_so->p_ydb_zstatus) {
         pcon->p_ydb_so->p_ydb_zstatus((ydb_char_t *) pcon->error, (ydb_long_t) 255);
      }
   }
   else {
      rc = pcon->p_isc_so->p_CachePushFunc(&(fun.rflag), (int) fun.label_len, (const Callin_char_t *) fun.label, (int) fun.routine_len, (const Callin_char_t *) fun.routine);
      rc = pcon->p_isc_so->p_CachePushInt(pcon->psql->sql_no);
      rc = pcon->p_isc_so->p_CachePushStr((int) pcon->psql->sql_script_len, (Callin_char_t *) pcon->psql->sql_script);
      rc = pcon->p_isc_so->p_CachePushStr((int) strlen(params), (Callin_char_t *) params);
      rc = pcon->p_isc_so->p_CacheExtFun(fun.rflag, 3);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }

   DBX_DB_UNLOCK(rc);

   if (rc != CACHE_SUCCESS) {
      pcon->psql->sqlcode = -1;
      strcpy(pcon->psql->sqlstate, "HY000");
      dbx_error_message(pcon, rc);
      goto dbx_sql_execute_exit;
   }

   offset = 4;
   len = (int) dbx_get_block_size((unsigned char *) pcon->output_val.svalue.buf_addr, offset, &dsort, &dtype);
   offset += 5;

   /* printf("\r\nEXEC SQL: len=%d; offset=%d; sort=%d; type=%d; str=%s;", len, offset, dsort, dtype, pcon->output_val.svalue.buf_addr + offset); */

   if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
      pcon->psql->sqlcode = -1;
      strcpy(pcon->psql->sqlstate, "HY000");
      strncpy(pcon->error, pcon->output_val.svalue.buf_addr + offset, len);
      pcon->error[len] = '\0';
      offset += len;
      goto dbx_sql_execute_exit;      
   }

   strncpy(buffer, pcon->output_val.svalue.buf_addr + offset, len);
   buffer[len] = '\0';
   offset += len;
   no_cols = (int) strtol(buffer, NULL, 10);
   /* printf("\r\nlen=%d; no_cols=%d;", len, no_cols); */

   for (cn = 0; cn < no_cols; cn ++) {
      len = (int) dbx_get_block_size((unsigned char *) pcon->output_val.svalue.buf_addr, offset, &dsort, &dtype);
      offset += 5;

      /* printf("\r\nEXEC SQL COL: cn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", cn, len, offset, dsort, dtype, pcon->output_val.svalue.buf_addr + offset); */

      if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
         break;
      }

      pcon->psql->cols[cn] = (DBXSQLCOL *) dbx_malloc(sizeof(DBXSQLCOL) + (len + 4), 0);
      pcon->psql->cols[cn]->name.buf_addr = ((char *) pcon->psql->cols[cn]) + sizeof(DBXSQLCOL);
      strncpy(pcon->psql->cols[cn]->name.buf_addr, pcon->output_val.svalue.buf_addr + offset, len);
      pcon->psql->cols[cn]->name.buf_addr[len] = '\0';
      pcon->psql->cols[cn]->name.len_used = len;

      offset += len;
   }
   pcon->psql->no_cols = no_cols;
   pcon->psql->cols[no_cols] = NULL; 

dbx_sql_execute_exit:

   isc_cleanup(pcon);

   return 0;
}


int dbx_sql_row(DBXCON *pcon, int rn, int dir)
{
   int rc, len, dsort, dtype, row_no, eod;
   unsigned long offset;
   char label[16], routine[16], params[9], buffer[8], rowstr[8];
   DBXFUN fun;

   eod = 0;
   pcon->psql->sqlcode = 0;
   strcpy(pcon->psql->sqlstate, "00000");
   pcon->output_val.offs = 0;
   if (dir == 1)
      strcpy(params, "+1");
   else if (dir == -1)
      strcpy(params, "-1");
   else
      strcpy(params, "");
   strcpy(label, "sqlrow");

   strcpy(routine, "%zmgsis");
   fun.label = label;
   fun.label_len = (int) strlen(label);
   fun.routine = routine;
   fun.routine_len = (int) strlen(routine);
   pcon->output_val.svalue.len_used = 0;
   pcon->output_val.svalue.buf_addr[0] = '\0';
   pcon->output_val.svalue.buf_addr[1] = '\0';
   pcon->output_val.svalue.buf_addr[2] = '\0';
   pcon->output_val.svalue.buf_addr[3] = '\0';
   pcon->output_val.svalue.buf_addr[4] = '\0';

   DBX_DB_LOCK(rc, 0);

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      ydb_string_t out, in1, in2, in3;

      sprintf(buffer, "%d", pcon->psql->sql_no);
      sprintf(rowstr, "%d", rn);

      out.address = (char *) pcon->output_val.svalue.buf_addr;
      out.length = (unsigned long) pcon->output_val.svalue.len_alloc;
      in1.address = (char *) buffer;
      in1.length = (unsigned long) strlen(buffer);
      in2.address = (char *) rowstr;
      in2.length = (unsigned long) strlen(rowstr);
      in3.address = (char *) params;
      in3.length = (unsigned long) strlen(params);

      rc = pcon->p_ydb_so->p_ydb_ci(fun.label, &out, &in1, &in2, &in3);

      if (rc == YDB_OK) {
         pcon->output_val.svalue.len_used = (unsigned int) dbx_get_size((unsigned char *) pcon->output_val.svalue.buf_addr);
      }
      else if (pcon->p_ydb_so->p_ydb_zstatus) {
         pcon->p_ydb_so->p_ydb_zstatus((ydb_char_t *) pcon->error, (ydb_long_t) 255);
      }
   }
   else {
      rc = pcon->p_isc_so->p_CachePushFunc(&(fun.rflag), (int) fun.label_len, (const Callin_char_t *) fun.label, (int) fun.routine_len, (const Callin_char_t *) fun.routine);
      rc = pcon->p_isc_so->p_CachePushInt(pcon->psql->sql_no);
      rc = pcon->p_isc_so->p_CachePushInt(rn);
      rc = pcon->p_isc_so->p_CachePushStr((int) strlen(params), (Callin_char_t *) params);
      rc = pcon->p_isc_so->p_CacheExtFun(fun.rflag, 3);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }

   DBX_DB_UNLOCK(rc);

   if (rc != CACHE_SUCCESS) {
      eod = 1;
      pcon->psql->sqlcode = -1;
      strcpy(pcon->psql->sqlstate, "HY000");
      dbx_error_message(pcon, rc);
      goto dbx_sql_row_exit;
   }

   if (pcon->output_val.svalue.len_used == 0) {
      eod = 1;
      pcon->psql->row_no = 0;
      goto dbx_sql_row_exit;
   }

   offset = 4;
   len = (int) dbx_get_block_size((unsigned char *) pcon->output_val.svalue.buf_addr, offset, &dsort, &dtype);
   offset += 5;

   if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
      pcon->psql->sqlcode = -1;
      strcpy(pcon->psql->sqlstate, "HY000");
      strncpy(pcon->error, pcon->output_val.svalue.buf_addr + offset, len);
      pcon->error[len] = '\0';
      offset += len;
      goto dbx_sql_row_exit;      
   }

   if (len == 0) {
      eod = 1;
      pcon->psql->row_no = 0;
      goto dbx_sql_row_exit;
   }

   strncpy(buffer, pcon->output_val.svalue.buf_addr + offset, len);
   buffer[len] = '\0';
   offset += len;
   row_no = (int) strtol(buffer, NULL, 10);
   pcon->psql->row_no = row_no;
   pcon->output_val.offs = offset;

   /* printf("\r\nROW NUMBER: len=%d; row_no=%d; pcon->output_val.offs=%d; total=%d;", len, row_no, pcon->output_val.offs, pcon->output_val.svalue.len_used); */

dbx_sql_row_exit:

   isc_cleanup(pcon);

   return eod;
}


int dbx_sql_cleanup(DBXCON *pcon)
{
   int rc;
   char label[16], routine[16], params[9], buffer[8];
   DBXFUN fun;

   pcon->psql->sqlcode = 0;
   strcpy(pcon->psql->sqlstate, "00000");
   pcon->output_val.offs = 0;
   strcpy(params, "");
   strcpy(label, "sqldel");

   strcpy(routine, "%zmgsis");
   fun.label = label;
   fun.label_len = (int) strlen(label);
   fun.routine = routine;
   fun.routine_len = (int) strlen(routine);
   pcon->output_val.svalue.len_used = 0;
   pcon->output_val.svalue.buf_addr[0] = '\0';
   pcon->output_val.svalue.buf_addr[1] = '\0';
   pcon->output_val.svalue.buf_addr[2] = '\0';
   pcon->output_val.svalue.buf_addr[3] = '\0';
   pcon->output_val.svalue.buf_addr[4] = '\0';

   DBX_DB_LOCK(rc, 0);

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      ydb_string_t out, in1, in2;

      sprintf(buffer, "%d", pcon->psql->sql_no);

      out.address = (char *) pcon->output_val.svalue.buf_addr;
      out.length = (unsigned long) pcon->output_val.svalue.len_alloc;
      in1.address = (char *) buffer;
      in1.length = (unsigned long) strlen(buffer);
      in2.address = (char *) params;
      in2.length = (unsigned long) strlen(params);

      rc = pcon->p_ydb_so->p_ydb_ci(fun.label, &out, &in1, &in2);

      if (rc == YDB_OK) {
         pcon->output_val.svalue.len_used = (unsigned int) dbx_get_size((unsigned char *) pcon->output_val.svalue.buf_addr);
      }
      else if (pcon->p_ydb_so->p_ydb_zstatus) {
         pcon->p_ydb_so->p_ydb_zstatus((ydb_char_t *) pcon->error, (ydb_long_t) 255);
      }
   }
   else {
      rc = pcon->p_isc_so->p_CachePushFunc(&(fun.rflag), (int) fun.label_len, (const Callin_char_t *) fun.label, (int) fun.routine_len, (const Callin_char_t *) fun.routine);
      rc = pcon->p_isc_so->p_CachePushInt(pcon->psql->sql_no);
      rc = pcon->p_isc_so->p_CachePushStr((int) strlen(params), (Callin_char_t *) params);
      rc = pcon->p_isc_so->p_CacheExtFun(fun.rflag, 2);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
      }
   }

   DBX_DB_UNLOCK(rc);

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   isc_cleanup(pcon);

   return 0;
}


int dbx_global_directory(DBXCON *pcon, DBXQR *pqr_prev, short dir, int *counter)
{
   int rc, eod, len, tmpglolen;
   char *p;
   char buffer[256], tmpglo[32];

   eod = 0;
   if (pqr_prev->global_name.buf_addr[0] != '^') {
      strcpy(buffer, "^");
      if (pqr_prev->global_name.len_used > 0) {
         strncpy(buffer + 1, pqr_prev->global_name.buf_addr, pqr_prev->global_name.len_used);
         pqr_prev->global_name.len_used ++;
         strncpy(pqr_prev->global_name.buf_addr, buffer, pqr_prev->global_name.len_used);
      }
      else {
         strcpy(pqr_prev->global_name.buf_addr, "^");
         pqr_prev->global_name.len_used = 1;
      }
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pqr_prev->global_name.len_used == 1) {
         strcat(pqr_prev->global_name.buf_addr, "%");
         pqr_prev->global_name.len_used ++;
      }

      if (dir == 1) {
         rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pqr_prev->global_name), 0, NULL, &(pcon->output_val.svalue));
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pqr_prev->global_name), 0, NULL, &(pcon->output_val.svalue));
      }

      if (rc == YDB_OK && pcon->output_val.svalue.len_used > 0) {
         strncpy(pqr_prev->global_name.buf_addr, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used);
         pqr_prev->global_name.len_used =  pcon->output_val.svalue.len_used;
         pqr_prev->global_name.buf_addr[pqr_prev->global_name.len_used] = '\0';
         (*counter) ++;
      }
      else {
         eod = 1;
      }
   }
   else {

      strcpy(tmpglo, "MGDBXPPG");
      /* strcpy(tmpglo, "zzz"); */
      tmpglolen = (int) strlen(tmpglo);

      if ((*counter) == 0) {
         rc = pcon->p_isc_so->p_CachePushGlobalX(tmpglolen, (Callin_char_t *) tmpglo, 1, (Callin_char_t *) "^");
         /* rc = pcon->p_isc_so->p_CachePushGlobal(tmpglolen, (Callin_char_t *) tmpglo); */
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CacheGlobalKill(0, 1);
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CachePushGlobalX(tmpglolen, (Callin_char_t *) tmpglo, 1, (Callin_char_t *) "^");
         /* rc = pcon->p_isc_so->p_CachePushGlobal(tmpglolen, (Callin_char_t *) tmpglo); */
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CacheAddGlobalDescriptor(0);
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CacheAddSSVN(8, (Callin_char_t *) "^$GLOBAL");
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CacheAddSSVNDescriptor(0);
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
         rc = pcon->p_isc_so->p_CacheMerge();
         if (rc != CACHE_SUCCESS) {
            eod = 1;
            return eod;
         }
      }

      rc = pcon->p_isc_so->p_CachePushGlobalX(tmpglolen, (Callin_char_t *) tmpglo, 1, (Callin_char_t *) "^");
      if (rc != CACHE_SUCCESS) {
         eod = 1;
         return eod;
      }

      rc = pcon->p_isc_so->p_CachePushStr((int) pqr_prev->global_name.len_used, (Callin_char_t *) pqr_prev->global_name.buf_addr);
      if (rc != CACHE_SUCCESS) {
         eod = 1;
         return eod;
      }

      rc = pcon->p_isc_so->p_CacheGlobalOrder(1, dir, 0);

      if (rc == CACHE_SUCCESS) {
         len = 0;
         rc = pcon->p_isc_so->p_CachePopStr(&len, (Callin_char_t **) &p);
         if (rc == CACHE_SUCCESS && len) {
            strncpy(pqr_prev->global_name.buf_addr, p, len);
            pqr_prev->global_name.buf_addr[len] = '\0';
            pqr_prev->global_name.len_used = len;
            (*counter) ++;
         }
         else {
            eod = 1;
         }
      }
      else {
         eod = 1;
      }
   }

   return eod;
}


int dbx_global_order(DBXCON *pcon, DBXQR *pqr_prev, short dir, short getdata)
{
   int rc, n;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      if (dir == 1) {
         rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pqr_prev->global_name), pqr_prev->keyn, &pqr_prev->key[0], &(pcon->output_val.svalue));
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pqr_prev->global_name), pqr_prev->keyn, &pqr_prev->key[0], &(pcon->output_val.svalue));
      }

      if (getdata && pcon->output_val.svalue.len_used > 0) {
         strcpy(pqr_prev->key[pqr_prev->keyn - 1].buf_addr, pcon->output_val.svalue.buf_addr);
         pqr_prev->key[pqr_prev->keyn - 1].len_used =  pcon->output_val.svalue.len_used;
         rc = pcon->p_ydb_so->p_ydb_get_s(&(pqr_prev->global_name), pqr_prev->keyn, &pqr_prev->key[0], &(pqr_prev->data.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CachePushGlobal((int) pqr_prev->global_name.len_used, (Callin_char_t *) pqr_prev->global_name.buf_addr);
      for (n = 0; n < pqr_prev->keyn; n ++) {
         rc = pcon->p_isc_so->p_CachePushStr(pqr_prev->key[n].len_used, (Callin_char_t *) pqr_prev->key[n].buf_addr);
      }
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pqr_prev->keyn, dir, getdata);
   }
   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {

         if (getdata) {
            rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            if (pcon->output_val.svalue.len_used == 1 && pcon->output_val.svalue.buf_addr[0] == '0') {
               pqr_prev->data.svalue.buf_addr[0] = '\0';
               pqr_prev->data.svalue.len_used = 0;
               rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            }
            else {
               rc = isc_pop_value(pcon, &(pqr_prev->data), DBX_TYPE_STR);
               rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            }
         }
         else {
            rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
         }
         strcpy(pqr_prev->key[pqr_prev->keyn - 1].buf_addr, pcon->output_val.svalue.buf_addr);
         pqr_prev->key[pqr_prev->keyn - 1].len_used =  pcon->output_val.svalue.len_used;
      }
      else {
         strcpy(pqr_prev->key[pqr_prev->keyn - 1].buf_addr, pcon->output_val.svalue.buf_addr);
         pqr_prev->key[pqr_prev->keyn - 1].len_used =  pcon->output_val.svalue.len_used;
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

   return 0;
}


int dbx_global_query(DBXCON *pcon, DBXQR *pqr_next, DBXQR *pqr_prev, short dir, short getdata)
{
   int rc, n, eod;

   eod = 0;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pqr_next->keyn = DBX_MAXARGS;

      if (dir == 1) {
         rc = pcon->p_ydb_so->p_ydb_node_next_s(&(pqr_prev->global_name), pqr_prev->keyn, &pqr_prev->key[0], &(pqr_next->keyn), &pqr_next->key[0]);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_node_previous_s(&(pqr_prev->global_name), pqr_prev->keyn, &pqr_prev->key[0], &(pqr_next->keyn), &pqr_next->key[0]);
      }

      /* printf("\r\npqr_next->keyn=%d; rc=%d\r\n", pqr_next->keyn, rc); */
      if (pqr_next->keyn == YDB_NODE_END || rc != YDB_OK) { /* v1.3.8 */
         eod = 1;
         pqr_next->data.svalue.buf_addr[0] = '\0';
         pqr_next->data.svalue.len_used = 0; /* v1.3.8 */
         pqr_next->keyn = 0;
      }
      if (getdata && !eod) {
         rc = pcon->p_ydb_so->p_ydb_get_s(&(pqr_next->global_name), pqr_next->keyn, &pqr_next->key[0], &(pqr_next->data.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CachePushGlobal((int) pqr_prev->global_name.len_used, (Callin_char_t *) pqr_prev->global_name.buf_addr);
      for (n = 0; n < pqr_prev->keyn; n ++) {
         rc = pcon->p_isc_so->p_CachePushStr(pqr_prev->key[n].len_used, (Callin_char_t *) pqr_prev->key[n].buf_addr);
      }
      rc = pcon->p_isc_so->p_CacheGlobalQuery(pqr_prev->keyn, dir, getdata);
   }

   if (rc == CACHE_SUCCESS) {
      if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
         pqr_next->keyn = 0;
         if (getdata) {
            rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            if (pcon->output_val.svalue.len_used == 1 && pcon->output_val.svalue.buf_addr[0] == '0') {
               rc = isc_pop_value(pcon, &(pqr_next->data), DBX_TYPE_STR);
               rc = isc_pop_value(pcon, &(pqr_next->data), DBX_TYPE_STR);
               eod = 1;
            }
            else {
               rc = isc_pop_value(pcon, &(pqr_next->data), DBX_TYPE_STR);
               rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
               dbx_parse_global_reference(pcon, pqr_next, (char *) pcon->output_val.svalue.buf_addr, (int) pcon->output_val.svalue.len_used);
               rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            }
         }
         else {
            rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
            dbx_parse_global_reference(pcon, pqr_next, (char *) pcon->output_val.svalue.buf_addr, (int) pcon->output_val.svalue.len_used);
            if (pcon->output_val.svalue.len_used == 0) {
               eod = 1;
            }
            rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
         }
      }
   }
   else {
      dbx_error_message(pcon, rc);
   }

   return eod;
}


int dbx_parse_global_reference(DBXCON *pcon, DBXQR *pqr, char *global_ref, int global_ref_len)
{
   int n, nq;
   unsigned int len;
   char *p, *p1, *pc, *pc0;

   if (global_ref_len == 0) {
      global_ref_len = (int) strlen(global_ref);
      if (global_ref_len == 0) {
         pqr->keyn = 0;
         return 0;
      }
   }

   if (global_ref[global_ref_len - 1] == ')') {
      global_ref_len --;
      global_ref[global_ref_len] = '\0';
   }
   p = (char *) strstr((char *) global_ref, "(");
   if (!p) { /* root node; no keys */
      pqr->keyn = 0;
      return 0;
   }

   p ++;

   pc = NULL;
   pqr->keyn = 0;
   for (;;) {

      pc0 = p;
      while (pc0) {
         pc = strstr(pc0, ",");
         if (pc) {
            *pc = '\0';
            nq = 0;
            p1 = p;
            while (*p1) {
               if (*p1 == '\"')
                  nq ++;
               p1 ++;
            }
            *pc = ',';
            if ((nq % 2) == 0) {
               break;
            }
            else {
               pc0 = (pc + 1);
            }
         }
         else {
            break;
         }
      }
      if (pc) {
         *pc = '\0';
      }

      if (*p == '\"') {
         char *p1, *p2;
         p ++;
         p1 = p;
         p2 = p;
         while (*p1) {
            if (*p1 == '\"') {
               len = 0;
               while (*p1 == '\"') {
                  len ++;
                  p1 ++;
               }
               len = len / 2;
               for (n = 0; n < (int) len; n ++) {
                  *(p2++) = '\"';
               }
            }
            else
               *(p2 ++) = *(p1 ++);

         }
         *(p2++) = '\0';
      }

      len = (unsigned int) strlen(p);
      strcpy((char *) pqr->key[pqr->keyn].buf_addr, (char *) p);
      pqr->key[pqr->keyn].len_used = len;
      pqr->keyn ++;

      if (pc)
         p = (pc + 1);
      else
         break;
   }

   return 0;
}


int cache_report_failure(DBXCON *pcon)
{

   if (pcon->error_code == 0) {
      pcon->error_code = 10001;
      T_STRCPY(pcon->error, _dbxso(pcon->error), DBX_TEXT_E_ASYNC);
   }

   return 0;
}


/* ASYNC THREAD : Do the Cache task */
int dbx_launch_thread(DBXCON *pcon)
{
#if !defined(_WIN32)

   dbx_pool_submit_task(pcon);

   return 1;

#else

   {
      int rc = 0;
#if defined(_WIN32)
      HANDLE hthread;
      SIZE_T stack_size;
      DWORD thread_id, cre_flags, result;
      LPSECURITY_ATTRIBUTES pattr;

      stack_size = 0;
      cre_flags = 0;
      pattr = NULL;

      hthread = CreateThread(pattr, stack_size, (LPTHREAD_START_ROUTINE) dbx_thread_main, (LPVOID) pcon, cre_flags, &thread_id);
/*
      printf("\r\n*** %lu Primary Thread : CreateThread (%d) ...", (unsigned long) dbx_current_thread_id(), dbx_queue_size);
*/
      if (!hthread) {
         printf("failed to create thread, errno = %d\n",errno);
      }
      else {
         result = WaitForSingleObject(hthread, INFINITE);
      }
#else
      pthread_attr_t attr;
      pthread_t child_thread;

      size_t stacksize, newstacksize;

      pthread_attr_init(&attr);

      stacksize = 0;
      pthread_attr_getstacksize(&attr, &stacksize);

      newstacksize = DBX_THREAD_STACK_SIZE;

      pthread_attr_setstacksize(&attr, newstacksize);
/*
      printf("Thread: default stack=%lu; new stack=%lu;\n", (unsigned long) stacksize, (unsigned long) newstacksize);
*/
      rc = pthread_create(&child_thread, &attr, dbx_thread_main, (void *) pcon);
      if (rc) {
         printf("failed to create thread, errno = %d\n",errno);
      }
      else {
         rc = pthread_join(child_thread,NULL);
      }
#endif
   }

#endif

   return 1;
}


#if defined(_WIN32)
LPTHREAD_START_ROUTINE dbx_thread_main(LPVOID pargs)
#else
void * dbx_thread_main(void *pargs)
#endif
{
   DBXCON *pcon;
/*
   printf("\r\n*** %lu Worker Thread : Thread (%d) ...", (unsigned long) dbx_current_thread_id(), dbx_queue_size);
*/
   pcon = (DBXCON *) pargs;

   pcon->p_dbxfun(pcon);

#if defined(_WIN32)
   return 0;
#else
   return NULL;
#endif
}


struct dbx_pool_task* dbx_pool_add_task(DBXCON *pcon)
{
   struct dbx_pool_task* enqueue_task;

   enqueue_task = (struct dbx_pool_task*) dbx_malloc(sizeof(struct dbx_pool_task), 601);

   if (!enqueue_task) {
      dbx_request_errors ++;
      return NULL;
   }

   enqueue_task->task_id = 6;
   enqueue_task->pcon = pcon;
   enqueue_task->next = NULL;

#if !defined(_WIN32)
   enqueue_task->parent_tid = pthread_self();

   pthread_mutex_lock(&dbx_task_queue_mutex);

   if (dbx_total_tasks == 0) {
      tasks = enqueue_task;
      bottom_task = enqueue_task;
   }
   else {
      bottom_task->next = enqueue_task;
      bottom_task = enqueue_task;
   }

   dbx_total_tasks ++;

   pthread_mutex_unlock(&dbx_task_queue_mutex);
   pthread_cond_signal(&dbx_pool_cond);
#endif

   return enqueue_task;
}


struct dbx_pool_task * dbx_pool_get_task(void)
{
   struct dbx_pool_task* task;

#if !defined(_WIN32)
   pthread_mutex_lock(&dbx_task_queue_mutex);
#endif

   if (dbx_total_tasks > 0) {
      task = tasks;
      tasks = task->next;

      if (tasks == NULL) {
         bottom_task = NULL;
      }
      dbx_total_tasks --;
   }
   else {
      task = NULL;
   }

#if !defined(_WIN32)
   pthread_mutex_unlock(&dbx_task_queue_mutex);
#endif

   return task;
}


void dbx_pool_execute_task(struct dbx_pool_task *task, int thread_id)
{
   if (task) {

      task->pcon->p_dbxfun(task->pcon);

      task->pcon->done = 1;

#if !defined(_WIN32)
      pthread_cond_broadcast(&dbx_result_cond);
#endif
   }
}


void * dbx_pool_requests_loop(void *data)
{
   int thread_id;
   DBXTID *ptid;
   struct dbx_pool_task *task = NULL;

   ptid = (DBXTID *) data;
   if (!ptid) {
      return NULL;
   }

   thread_id = ptid->thread_id;

/*
#if !defined(_WIN32)
{
   int ret;
   pthread_attr_t tattr;
   size_t size;
   ret = pthread_getattr_np(pthread_self(), &tattr);
   ret = pthread_attr_getstacksize(&tattr, &size);
   printf("\r\n*** pthread_attr_getstacksize (pool) %x *** ret=%d; size=%u\r\n", (unsigned long) pthread_self(), ret, size);
}
#endif
*/

#if !defined(_WIN32)
   pthread_mutex_lock(&dbx_pool_mutex);

   while (1) {
      if (dbx_total_tasks > 0) {
         task = dbx_pool_get_task();
         if (task) {
            pthread_mutex_unlock(&dbx_pool_mutex);
            dbx_pool_execute_task(task, thread_id);
            dbx_free(task, 3001);
            pthread_mutex_lock(&dbx_pool_mutex);
         }
      }
      else {
         pthread_cond_wait(&dbx_pool_cond, &dbx_pool_mutex);
      }
   }
#endif

   return NULL;
}


int dbx_pool_thread_init(DBXCON *pcon, int num_threads)
{
#if !defined(_WIN32)
   int iterator;
   pthread_attr_t attr[DBX_THREADPOOL_MAX];
   size_t stacksize, newstacksize;

   for (iterator = 0; iterator < num_threads; iterator ++) {

      pthread_attr_init(&attr[iterator]);

      stacksize = 0;
      pthread_attr_getstacksize(&attr[iterator], &stacksize);

      newstacksize = DBX_THREAD_STACK_SIZE;
/*
      printf("Thread Pool: default stack=%lu; new stack=%lu;\n", (unsigned long) stacksize, (unsigned long) newstacksize);
*/
      pthread_attr_setstacksize(&attr[iterator], newstacksize);

      dbx_thr_id[iterator].thread_id = iterator;
      dbx_thr_id[iterator].p_mutex = pcon->p_mutex;
      dbx_thr_id[iterator].p_zv = pcon->p_zv;
      
      pthread_create(&dbx_p_threads[iterator], &attr[iterator], dbx_pool_requests_loop, (void *) &dbx_thr_id[iterator]);

   }
#endif
   return 0;
}


int dbx_pool_submit_task(DBXCON *pcon)
{
#if !defined(_WIN32)
   struct timespec   ts;
   struct timeval    tp;

   pcon->done = 0;

   dbx_pool_add_task(pcon);

   pthread_mutex_lock(&dbx_result_mutex);

   while (!pcon->done) {

      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec;
      ts.tv_nsec = tp.tv_usec * 1000;
      ts.tv_sec += 3;

      pthread_cond_timedwait(&dbx_result_cond, &dbx_result_mutex, &ts);
   }

   pthread_mutex_unlock(&dbx_result_mutex);
#endif
   return 1;
}


int dbx_add_block_size(unsigned char *block, unsigned long offset, unsigned long data_len, int dsort, int dtype)
{
   dbx_set_size((unsigned char *) block + offset, data_len);
   block[offset + 4] = (unsigned char) ((dsort * 20) + dtype);

   return 1;
}


unsigned long dbx_get_block_size(unsigned char *block, unsigned long offset, int *dsort, int *dtype)
{
   unsigned long data_len;
   unsigned char uc;

   data_len = 0;
   uc = (unsigned char) block[offset + 4];

   *dtype = uc % 20;
   *dsort = uc / 20;

   if (*dsort != DBX_DSORT_STATUS) {
      data_len = dbx_get_size((unsigned char *) block + offset);
   }

   if (!DBX_DSORT_ISVALID(*dsort)) {
      *dsort = DBX_DSORT_INVALID;
   }

   return data_len;
}


int dbx_set_size(unsigned char *str, unsigned long data_len)
{
   str[0] = (unsigned char) (data_len >> 0);
   str[1] = (unsigned char) (data_len >> 8);
   str[2] = (unsigned char) (data_len >> 16);
   str[3] = (unsigned char) (data_len >> 24);

   return 0;
}


unsigned long dbx_get_size(unsigned char *str)
{
   unsigned long size;

   size = ((unsigned char) str[0]) | (((unsigned char) str[1]) << 8) | (((unsigned char) str[2]) << 16) | (((unsigned char) str[3]) << 24);
   return size;
}


void * dbx_realloc(void *p, int curr_size, int new_size, short id)
{
   if (new_size >= curr_size) {
      if (p) {
         dbx_free((void *) p, 0);
      }
      p = (void *) dbx_malloc(new_size, id);
      if (!p) {
         return NULL;
      }
   }
   return p;
}


void * dbx_malloc(int size, short id)
{
   void *p;

   p = (void *) malloc(size);

   return p;
}


int dbx_free(void *p, short id)
{
   /* printf("\ndbx_free: id=%d; p=%p;", id, p); */

   free((void *) p);

   return 0;
}

DBXQR * dbx_alloc_dbxqr(DBXQR *pqr, int dsize, short context)
{
   int n;
   char *p;

   pqr = (DBXQR *) dbx_malloc(sizeof(DBXQR) + CACHE_MAXSTRLEN, 0);
   if (!pqr) {
      return pqr;
   }
   pqr->kbuffer_size = CACHE_MAXSTRLEN;
   p = (char *) ((char *) pqr) + sizeof(DBXQR);
   pqr->global_name.buf_addr = p;
   pqr->global_name.len_alloc = 128;
   pqr->global_name.len_used = 0;
   p += 128;
   pqr->kbuffer = (unsigned char *) p;
   for (n = 0; n < DBX_MAXARGS; n ++) {
      pqr->key[n].buf_addr = (p + 5);
      pqr->key[n].len_alloc = 256;
      pqr->key[n].len_used = 0;
      p += (5 + 256);
   }

   pqr->data.svalue.len_alloc = CACHE_MAXSTRLEN;
   pqr->data.svalue.len_used = 0;

   pqr->data.svalue.buf_addr = (char *) dbx_malloc(CACHE_MAXSTRLEN, 0);
   if (pqr->data.svalue.buf_addr) {
      pqr->data.svalue.len_alloc = CACHE_MAXSTRLEN;
      pqr->data.svalue.len_used = 0;
   }
   return pqr;
}


int dbx_free_dbxqr(DBXQR *pqr)
{
   if (pqr->data.svalue.buf_addr) {
      dbx_free((void *) pqr->data.svalue.buf_addr, 0); 
   }
   dbx_free((void *) pqr, 0); 
   return 0;
}


int dbx_ucase(char *string)
{
#ifdef _UNICODE

   CharUpperA(string);
   return 1;

#else

   int n, chr;

   n = 0;
   while (string[n] != '\0') {
      chr = (int) string[n];
      if (chr >= 97 && chr <= 122)
         string[n] = (char) (chr - 32);
      n ++;
   }
   return 1;

#endif
}


int dbx_lcase(char *string)
{
#ifdef _UNICODE

   CharLowerA(string);
   return 1;

#else

   int n, chr;

   n = 0;
   while (string[n] != '\0') {
      chr = (int) string[n];
      if (chr >= 65 && chr <= 90)
         string[n] = (char) (chr + 32);
      n ++;
   }
   return 1;

#endif
}


int dbx_create_string(DBXSTR *pstr, void *data, short type)
{
   DBXSTR *pstrobj_in;

   if (!data) {
      return -1;
   }

   pstr->len_used = 0;

   if (type == DBX_TYPE_STROBJ) {
      pstrobj_in = (DBXSTR *) data;

      type = DBX_TYPE_STR8;
      data = (void *) pstrobj_in->buf_addr;
   }

   if (type == DBX_TYPE_STR8) {
      T_STRCPY((char *) pstr->buf_addr, pstr->len_alloc, (char *) data);
   }
   else if (type == DBX_TYPE_INT) {
      T_SPRINTF((char *) pstr->buf_addr, pstr->len_alloc, "%d", (int) *((int *) data));
   }
   else {
      pstr->buf_addr[0] = '\0';
   }
   pstr->len_used = (unsigned long) strlen((char *) pstr->buf_addr);

   return (int) pstr->len_used;
}


int dbx_buffer_dump(DBXCON *pcon, void *buffer, unsigned int len, char *title, unsigned char csize, short mode)
{
   unsigned int n;
   unsigned char *p8;
   unsigned short *p16;
   unsigned short c;

   p8 = NULL;
   p16 = NULL;

   if (csize == 16) {
      p16 = (unsigned short *) buffer;
   }
   else {
      p8 = (unsigned char *) buffer;
   }
   printf("\nbuffer dump (title=%s; size=%d; charsize=%u; mode=%d)...\n", title, len, csize, mode);

   for (n = 0; n < len; n ++) {
      if (csize == 16) {
         c = p16[n];
      }
      else {
         c = p8[n];
      }

      if (mode == 1) {
         printf("\\x%04x ", c);
         if (!((n + 1) % 8)) {
            printf("\r\n");
         }
      }
      else {
         if ((c < 32) || (c > 126)) {
            if (csize == 16) {
               printf("\\x%04x", c);
            }
            else {
               printf("\\x%02x", c);
            }
         }
         else {
            printf("%c", (char) c);
         }
      }
   }

   return 0;
}


int dbx_log_event(char *message, char *title, int level)
{
   /* TODO */
   return 1;
}


int dbx_test_file_access(char *file, int mode)
{
   int result;
   char *p;
   char buffer[256];
   FILE *fp;

   result = 0;
   *buffer = '\0';
   dbx_fopen(&fp, file, "r");
   if (fp) {
      p = fgets(buffer, 250, fp);
      if (p && strlen(buffer)) {
/*
         dbx_log_event(NULL, buffer, (char *) "dbx_test_file_access", 0);
*/
         result = 1;
      }
      fclose(fp);
   }

   return result;


}


DBXPLIB dbx_dso_load(char * library)
{
   DBXPLIB p_library;

#if defined(_WIN32)
   p_library = LoadLibraryA(library);
#else
#if defined(RTLD_DEEPBIND)
   p_library = dlopen(library, RTLD_NOW | RTLD_DEEPBIND);
#else
   p_library = dlopen(library, RTLD_NOW);
#endif
#endif

   return p_library;
}



DBXPROC dbx_dso_sym(DBXPLIB p_library, char * symbol)
{
   DBXPROC p_proc;

#if defined(_WIN32)
   p_proc = GetProcAddress(p_library, symbol);
#else
   p_proc  = (void *) dlsym(p_library, symbol);
#endif

   return p_proc;
}



int dbx_dso_unload(DBXPLIB p_library)
{

#if defined(_WIN32)
   FreeLibrary(p_library);
#else
   dlclose(p_library); 
#endif

   return 1;
}


DBXTHID dbx_current_thread_id(void)
{
#if defined(_WIN32)
   return (DBXTHID) GetCurrentThreadId();
#else
   return (DBXTHID) pthread_self();
#endif
}


unsigned long dbx_current_process_id(void)
{
#if defined(_WIN32)
   return (unsigned long) GetCurrentProcessId();
#else
   return ((unsigned long) getpid());
#endif
}


int dbx_error_message(DBXCON *pcon, int error_code)
{
   int rc;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_error_message(pcon, error_code);
   }
   else {
      rc = isc_error_message(pcon, error_code);
   }

   return rc;
}


int dbx_mutex_create(DBXMUTEX *p_mutex)
{
   int result;

   result = 0;
   if (p_mutex->created) {
      return result;
   }

#if defined(_WIN32)
   p_mutex->h_mutex = CreateMutex(NULL, FALSE, NULL);
   result = 0;
#else
   result = pthread_mutex_init(&(p_mutex->h_mutex), NULL);
#endif

   p_mutex->created = 1;
   p_mutex->stack = 0;
   p_mutex->thid = 0;

   return result;
}



int dbx_mutex_lock(DBXMUTEX *p_mutex, int timeout)
{
   int result;
   DBXTHID tid;
#ifdef _WIN32
   DWORD result_wait;
#endif

   result = 0;

   if (!p_mutex->created) {
      return -1;
   }

   tid = dbx_current_thread_id();
   if (p_mutex->thid == tid) {
      p_mutex->stack ++;
      /* printf("\r\n thread already owns lock : thid=%lu; stack=%d;\r\n", (unsigned long) tid, p_mutex->stack); */
      return 0; /* success - thread already owns lock */
   }

#if defined(_WIN32)
   if (timeout == 0) {
      result_wait = WaitForSingleObject(p_mutex->h_mutex, INFINITE);
   }
   else {
      result_wait = WaitForSingleObject(p_mutex->h_mutex, (timeout * 1000));
   }

   if (result_wait == WAIT_OBJECT_0) { /* success */
      result = 0;
   }
   else if (result_wait == WAIT_ABANDONED) {
      printf("\r\ndbx_mutex_lock: Returned WAIT_ABANDONED state");
      result = -1;
   }
   else if (result_wait == WAIT_TIMEOUT) {
      printf("\r\ndbx_mutex_lock: Returned WAIT_TIMEOUT state");
      result = -1;
   }
   else if (result_wait == WAIT_FAILED) {
      printf("\r\ndbx_mutex_lock: Returned WAIT_FAILED state: Error Code: %d", GetLastError());
      result = -1;
   }
   else {
      printf("\r\ndbx_mutex_lock: Returned Unrecognized state: %d", result_wait);
      result = -1;
   }
#else
   result = pthread_mutex_lock(&(p_mutex->h_mutex));
#endif

   p_mutex->thid = tid;
   p_mutex->stack = 0;

   return result;
}


int dbx_mutex_unlock(DBXMUTEX *p_mutex)
{
   int result;
   DBXTHID tid;

   result = 0;

   if (!p_mutex->created) {
      return -1;
   }

   tid = dbx_current_thread_id();
   if (p_mutex->thid == tid && p_mutex->stack) {
      /* printf("\r\n thread has stacked locks : thid=%lu; stack=%d;\r\n", (unsigned long) tid, p_mutex->stack); */
      p_mutex->stack --;
      return 0;
   }
   p_mutex->thid = 0;
   p_mutex->stack = 0;

#if defined(_WIN32)
   ReleaseMutex(p_mutex->h_mutex);
   result = 0;
#else
   result = pthread_mutex_unlock(&(p_mutex->h_mutex));
#endif /* #if defined(_WIN32) */

   return result;
}


int dbx_mutex_destroy(DBXMUTEX *p_mutex)
{
   int result;

   if (!p_mutex->created) {
      return -1;
   }

#if defined(_WIN32)
   CloseHandle(p_mutex->h_mutex);
   result = 0;
#else
   result = pthread_mutex_destroy(&(p_mutex->h_mutex));
#endif

   p_mutex->created = 0;

   return result;
}


int dbx_enter_critical_section(void *p_crit)
{
   int result;

#if defined(_WIN32)
   EnterCriticalSection((LPCRITICAL_SECTION) p_crit);
   result = 0;
#else
   result = pthread_mutex_lock((pthread_mutex_t *) p_crit);
#endif
   return result;
}


int dbx_leave_critical_section(void *p_crit)
{
   int result;

#if defined(_WIN32)
   LeaveCriticalSection((LPCRITICAL_SECTION) p_crit);
   result = 0;
#else
   result = pthread_mutex_unlock((pthread_mutex_t *) p_crit);
#endif
   return result;
}


int dbx_sleep(unsigned long msecs)
{
#if defined(_WIN32)

   Sleep((DWORD) msecs);

#else

#if 1
   unsigned int secs, msecs_rem;

   secs = (unsigned int) (msecs / 1000);
   msecs_rem = (unsigned int) (msecs % 1000);

   /* printf("\n   ===> msecs=%ld; secs=%ld; msecs_rem=%ld", msecs, secs, msecs_rem); */

   if (secs > 0) {
      sleep(secs);
   }
   if (msecs_rem > 0) {
      usleep((useconds_t) (msecs_rem * 1000));
   }

#else
   unsigned int secs;

   secs = (unsigned int) (msecs / 1000);
   if (secs == 0)
      secs = 1;
   sleep(secs);

#endif

#endif

   return 0;
}



int dbx_fopen(FILE **pfp, const char *file, const char *mode)
{
   int n;

   n = 0;
#if defined(DBX_USE_SECURE_CRT)
   n = (int) fopen_s(pfp, file, mode);
#else
   *pfp = fopen(file, mode);
#endif
   return n;
}


int dbx_strcpy_s(char *to, size_t size, const char *from, const char *file, const char *fun, const unsigned int line)
{
   size_t len_to;

   len_to = strlen(from);

   if (len_to > (size - 1)) {
      char buffer[256];
#if defined(DBX_USE_SECURE_CRT)
      sprintf_s(buffer, _dbxso(buffer), "Buffer Size Error: Function: strcpy_s([%lu], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) len_to, file, fun, line);
#else
      sprintf(buffer, "Buffer Size Error: Function: strcpy([%lu], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) len_to, file, fun, line);
#endif
      printf("\r\n%s", buffer);
   }

#if defined(DBX_USE_SECURE_CRT)
#if defined(_WIN32)
   strcpy_s(to, size, from);
#else
   strcpy_s(to, size, from);
#endif
#else
   strcpy(to, from);
#endif
   return 0;
}


int dbx_strncpy_s(char *to, size_t size, const char *from, size_t count, const char *file, const char *fun, const unsigned int line)
{
   if (count > size) {
      char buffer[256];
#if defined(DBX_USE_SECURE_CRT)
      sprintf_s(buffer, _dbxso(buffer), "Buffer Size Error: Function: strncpy_s([%lu], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) count, file, fun, line);
#else
      sprintf(buffer, "Buffer Size Error: Function: strncpy([%lu], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) count, file, fun, line);
#endif
      printf("\r\n%s", buffer);
   }

#if defined(DBX_USE_SECURE_CRT)
#if defined(_WIN32)
   /* strncpy_s(to, size, from, count); */
   memcpy_s(to, size, from, count);
#else
   strncpy_s(to, size, from, count);
#endif
#else
   strncpy(to, from, count);
#endif
   return 0;
}

int dbx_strcat_s(char *to, size_t size, const char *from, const char *file, const char *fun, const unsigned int line)
{
   size_t len_to, len_from;

   len_to = strlen(to);
   len_from = strlen(from);

   if (len_from > ((size - len_to) - 1)) {
      char buffer[256];
#if defined(DBX_USE_SECURE_CRT)
      sprintf_s(buffer, _dbxso(buffer), "Buffer Size Error: Function: strcat_s([%lu <= (%lu-%lu)], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) (size - len_to), (unsigned long) size, (unsigned long) len_to, (unsigned long) len_from , file, fun, line);
#else
      sprintf(buffer, "Buffer Size Error: Function: strcat([%lu <= (%lu-%lu)], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) (size - len_to), (unsigned long) size, (unsigned long) len_to, (unsigned long) len_from, file, fun, line);
#endif
      printf("\r\n%s", buffer);

   }
#if defined(DBX_USE_SECURE_CRT)
#if defined(_WIN32)
   strcat_s(to, size, from);
#else
   strcat_s(to, size, from);
#endif
#else
   strcat(to, from);
#endif

   return 0;
}

int dbx_strncat_s(char *to, size_t size, const char *from, size_t count, const char *file, const char *fun, const unsigned int line)
{
   size_t len_to;

   len_to = strlen(to);

   if (count > (size - len_to)) {
      char buffer[256];
#if defined(DBX_USE_SECURE_CRT)
      sprintf_s(buffer, _dbxso(buffer), "Buffer Size Error: Function: strncat_s([%lu <= (%lu-%lu)], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) (size - len_to), (unsigned long) size, (unsigned long) len_to, (unsigned long) count , file, fun, line);
#else
      sprintf(buffer, "Buffer Size Error: Function: strncat([%lu <= (%lu-%lu)], [%lu])    file=%s; fun=%s; line=%u;", (unsigned long) (size - len_to), (unsigned long) size, (unsigned long) len_to, (unsigned long) count , file, fun, line);
#endif
      printf("\r\n%s", buffer);
   }

#if defined(DBX_USE_SECURE_CRT)
#if defined(_WIN32)
   strncat_s(to, size, from, count);
#else
   strncat_s(to, size, from, count);
#endif
#else
   strncat(to, from, count);
#endif

   return 0;
}


int dbx_sprintf_s(char *to, size_t size, const char *format, const char *file, const char *fun, const unsigned int line, ...)
{
   va_list arg;
   size_t len_to;
   int ret, i;
   long l;
   char *p, *s;
   void *pv;
   char buffer[32];

   va_start(arg, line);

   /* Estimate size required based on types commonly used in the Gateway */
   len_to = strlen(format);
   p = (char *) format;
   while ((p = strstr(p, "%"))) {
      p ++;
      len_to -= 2;
      if (*p == 's') {
         s = va_arg(arg, char *);
         if (s)
            len_to += strlen(s);
      }
      else if (*p == 'c') {
         len_to += 1;
      }
      else if (*p == 'd' || *p == 'u') {
         i = va_arg(arg, int);
#if defined(DBX_USE_SECURE_CRT)
         sprintf_s(buffer, 32, "%d", i);
#else
         sprintf(buffer, "%d", i);
#endif
         len_to += strlen(buffer);
      }
      else if (*p == 'x') {
         i = va_arg(arg, int);
#if defined(DBX_USE_SECURE_CRT)
         sprintf_s(buffer, 32, "%x", i);
#else
         sprintf(buffer, "%x", i);
#endif
         len_to += strlen(buffer);
      }
      else if (*p == 'l') {
         len_to --;
         l = va_arg(arg, long);
#if defined(DBX_USE_SECURE_CRT)
         sprintf_s(buffer, 32, "%ld", l);
#else
         sprintf(buffer, "%ld", l);
#endif
         len_to += strlen(buffer);
      }
      else if (*p == 'p') {
         pv = va_arg(arg, void *);
#if defined(DBX_USE_SECURE_CRT)
         sprintf_s(buffer, 32, "%p", pv);
#else
         sprintf(buffer, "%p", pv);
#endif
         len_to += strlen(buffer);
      }
      else {
         pv = va_arg(arg, void *);
         len_to += 10;
      }
   }

   if (len_to > (size - 1)) {
      char buffer[256];
#if defined(DBX_USE_SECURE_CRT)
      sprintf_s(buffer, _dbxso(buffer), "Buffer Size Error: Function: sprintf_s([%lu], [%lu], ...)    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) len_to, file, fun, line);
#else
      sprintf(buffer, "Buffer Size Error: Function: sprintf([%lu], [%lu], ...)    file=%s; fun=%s; line=%u;", (unsigned long) size, (unsigned long) len_to, file, fun, line);
#endif
      printf("\r\n%s", buffer);
   }

   va_start(arg, line);

#if defined(DBX_USE_SECURE_CRT)
#if defined(_WIN32)
   ret = sprintf_s(to, size, format, arg);
#else
   ret = vsprintf_s(to, size, format, arg);
#endif
#else
   ret = vsprintf(to, format, arg);
#endif
   va_end(arg);

   return ret;
}

