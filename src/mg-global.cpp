/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2019-2026 MGateway Ltd                                     |
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

#include "mg-dbx.h"
#include "mg-global.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mglobal::constructor;

mglobal::mglobal(int value) : dbx_count(value)
{
}


mglobal::~mglobal()
{
   delete_mglobal_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mglobal::Init(Local<Object> exports)
#else
void mglobal::Init(Handle<Object> exports)
#endif
{
#if DBX_NODE_VERSION >= 120000
/* v2.4.31 */
#if DBX_NODE_VERSION >= 250000
   Isolate* isolate = Isolate::GetCurrent();
#else
   Isolate* isolate = exports->GetIsolate();
#endif
   Local<Context> icontext = isolate->GetCurrentContext();
/*
   Local<ObjectTemplate> mglobal_data_tpl = ObjectTemplate::New(isolate);
   mglobal_data_tpl->SetInternalFieldCount(1);
   Local<Object> mglobal_data = mglobal_data_tpl->NewInstance(icontext).ToLocalChecked();
*/
  /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, (char *) "mglobal", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* v1.4.10 */
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mglobal"));
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* v2.3.24 */
#endif

   /* Prototypes */

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
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "merge", Merge);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "_close", Close);

#if DBX_NODE_VERSION >= 120000
/*
   mglobal_data->SetInternalField(0, constructor);
*/
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
   exports->Set(icontext, String::NewFromUtf8(isolate, "mglobal", NewStringType::kNormal).ToLocalChecked(), tpl->GetFunction(icontext).ToLocalChecked()).FromJust();
#else
   constructor.Reset(isolate, tpl->GetFunction());
#endif

}


void mglobal::New(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
#if DBX_NODE_VERSION >= 100000
   Local<Context> icontext = isolate->GetCurrentContext();
#endif
   HandleScope scope(isolate);
   int rc, fc, mn, argc, otype;
   DBX_DBNAME *c = NULL;
   DBXCON *pcon = NULL;
   DBXMETH *pmeth = NULL;
   Local<Object> obj;

   /* 1.4.10 */
   argc = args.Length();
   if (argc > 0) {
      obj = dbx_is_object(args[0], &otype);
      if (otype) {
         fc = obj->InternalFieldCount();
         if (fc == 3) {
/* v2.4.29 */
#if DBX_NODE_VERSION >= 220000
            mn = obj->GetInternalField(2).As<v8::Value>().As<v8::External>()->Int32Value(icontext).FromJust();
#else
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
#endif
            if (mn == DBX_MAGIC_NUMBER) {
               c = ObjectWrap::Unwrap<DBX_DBNAME>(obj);
            }
         }
      }
   }

   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mglobal(...)` */
      int value = args[0]->IsUndefined() ? 0 : DBX_INT32_VALUE(args[0]);
      mglobal * obj = new mglobal(value);
      obj->c = NULL;

      if (c) { /* 1.4.10 */
         if (c->pcon == NULL) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "No Connection to the database", 1)));
            return;
         }
         pcon = c->pcon;
         pmeth = dbx_request_memory(pcon, 1, 1);
         pmeth->argc = argc;
         obj->c = c;
         obj->pkey = NULL;
         obj->global_name[0] = '\0';
         obj->global_name_len = 0;
         obj->global_name16[0] = 0;
         obj->global_name16_len = 0;
         rc = dbx_global_reset(args, isolate, pcon, pmeth, (void *) obj, 1, 1);
         if (rc < 0) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobal::New() method takes at least one argument (the global name)", 1)));
            dbx_request_memory_free(pcon, pmeth, 0);
            return;
         }
         dbx_request_memory_free(pcon, pmeth, 0);
      }

      obj->Wrap(args.This());
      args.This()->SetInternalField(2, DBX_INTEGER_NEW(DBX_MAGIC_NUMBER_MGLOBAL)); /* v1.4.10 */
      args.GetReturnValue().Set(args.This());
/*
      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, obj->New);
*/
   }
   else {
      /* Invoked as plain function `mglobal(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
   }

}


mglobal * mglobal::NewInstance(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();
   HandleScope scope(isolate);

#if DBX_NODE_VERSION >= 100000
   Local<Value> argv[2];
#else
   Handle<Value> argv[2];
#endif
   const unsigned argc = 1;

   argv[0] = args[0];

   /* 1.4.10 */
   Local<Function> cons = Local<Function>::New(isolate, constructor);
   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked(); /* Invoke mglobal::New */
 
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(instance);

   args.GetReturnValue().Set(instance);

   return gx;
}


int mglobal::async_callback(mglobal *gx)
{
   gx->Unref();
   return 0;
}


int mglobal::delete_mglobal_template(mglobal *gx)
{
   return 0;
}


void mglobal::Get(const FunctionCallbackInfo<Value>& args)
{
   return GetEx(args, 0);
}


void mglobal::Get_bx(const FunctionCallbackInfo<Value>& args)
{
   return GetEx(args, 1);
}


void mglobal::GetEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::get");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   pmeth->binary = binary;
   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Get", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::get");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_get;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }


   if (pcon->net_connection) {
      rc = dbx_get(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_get;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_get_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalGet(pmeth->cargc - 1, 0);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
      }
   }

   if (rc == CACHE_ERUNDEF) {
      dbx_create_string(&(pmeth->output_val.svalue), (void *) "", DBX_DTYPE_STR8);
   }
   else if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();
   
   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::get");
   }

   if (binary) {
      Local<Object> bx = node::Buffer::New(isolate, (char *) pmeth->output_val.svalue.buf_addr, (size_t) pmeth->output_val.svalue.len_used).ToLocalChecked();
      args.GetReturnValue().Set(bx);
   }
   else {
      result = pcon->utf16 ? dbx_new_string16n(isolate, pmeth->output_val.cvalue.buf16_addr, pmeth->output_val.cvalue.len_used) : dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
      args.GetReturnValue().Set(result);
   }
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Set(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::set");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Set", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::set");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_set;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_set(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_set;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_set_s(&(pmeth->args[0].svalue), pmeth->cargc - 2, &pmeth->yargs[0], &(pmeth->args[pmeth->cargc - 1].svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalSet(pmeth->cargc - 2);
   }

   if (rc == CACHE_ERUNDEF) {
      dbx_create_string(&(pmeth->output_val.svalue), (void *) "", DBX_DTYPE_STR);
   }
   else if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pmeth->output_val.svalue), &rc, DBX_DTYPE_INT);
   }
   else {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::set");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Defined(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::defined");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Defined", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::defined");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_defined;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
         dbx_request_memory_free(pcon, pmeth, 0);
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_defined(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_defined;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_data_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0], (unsigned int *) &n);
         dbx_create_string(&(pmeth->output_val.svalue), (void *) &n, DBX_DTYPE_INT);
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalData(pmeth->cargc - 1, 0);
      if (rc == CACHE_SUCCESS) {
         pcon->p_isc_so->p_CachePopInt(&n);
      }
      dbx_create_string(&(pmeth->output_val.svalue), (void *) &n, DBX_DTYPE_INT);
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::defined");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Delete(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::delete");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Delete", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::delete");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_delete(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_delete;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_delete_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0], YDB_DEL_TREE);
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalKill(pmeth->cargc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
      n = 0;
      dbx_create_string(&(pmeth->output_val.svalue), (void *) &n, DBX_DTYPE_INT);
   }
   else {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }


   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::delete");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::next");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::next");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_next;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_next(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_next;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pmeth->cargc - 1, 1, 0);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
      }
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::next");
   }

   result = pcon->utf16 ? dbx_new_string16n(isolate, pmeth->output_val.cvalue.buf16_addr, pmeth->output_val.cvalue.len_used) : dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);

   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::previous");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::previous");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_previous;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_previous(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_previous;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pmeth->cargc - 1, -1, 0);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
      }
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::previous");
   }

   result = pcon->utf16 ? dbx_new_string16n(isolate, pmeth->output_val.cvalue.buf16_addr, pmeth->output_val.cvalue.len_used) : dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);

   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Increment(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::increment");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Increment", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DBFUN_START(c, pcon, pmeth);

   pmeth->increment = 1;
   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::increment");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_increment;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_increment(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_increment;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_incr_s(&(pmeth->args[0].svalue), pmeth->cargc - 2, &pmeth->yargs[0], &(pmeth->args[pmeth->cargc - 1].svalue), &(pmeth->output_val.svalue));
      }
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalIncrement(pmeth->cargc - 2);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
      }
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::increment");
   }

   result = pcon->utf16 ? dbx_new_string16n(isolate, pmeth->output_val.cvalue.buf16_addr, pmeth->output_val.cvalue.len_used) : dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Lock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval, timeout, locktype;
   unsigned long long timeout_nsec;
   char buffer[32];
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::lock");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Lock", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   if (pmeth->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Lock", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DBFUN_START(c, pcon, pmeth);

   pmeth->lock = 1;
   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::lock");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_lock;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_lock(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_lock;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         timeout = -1;
         if (pmeth->args[pmeth->cargc - 1].svalue.len_used < 16) {
            strncpy(buffer, pmeth->args[pmeth->cargc - 1].svalue.buf_addr, pmeth->args[pmeth->cargc - 1].svalue.len_used);
            buffer[pmeth->args[pmeth->cargc - 1].svalue.len_used] = '\0';
            timeout = (int) strtol(buffer, NULL, 10);
         }
         timeout_nsec = 1000000000;

         if (timeout < 0)
            timeout_nsec *= 3600;
         else
            timeout_nsec *= timeout;
         rc = pcon->p_ydb_so->p_ydb_lock_incr_s(timeout_nsec, &(pmeth->args[0].svalue), pmeth->cargc - 2, &pmeth->yargs[0]);
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
         dbx_create_string(&(pmeth->output_val.svalue), (void *) &retval, DBX_DTYPE_INT);
      }
   }
   else {
      strncpy(buffer, pmeth->args[pmeth->cargc - 1].svalue.buf_addr, pmeth->args[pmeth->cargc - 1].svalue.len_used);
      buffer[pmeth->args[pmeth->cargc - 1].svalue.len_used] = '\0';
      timeout = (int) strtol(buffer, NULL, 10);
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheAcquireLock(pmeth->cargc - 2, locktype, timeout, &retval);
      dbx_create_string(&(pmeth->output_val.svalue), (void *) &retval, DBX_DTYPE_INT);
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::lock");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Unlock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::unlock");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   gref.global = gx->global_name;
   gref.global_len = gx->global_name_len;
   gref.global16 = gx->global_name16;
   gref.global16_len = gx->global_name16_len;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Unlock", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DBFUN_START(c, pcon, pmeth);

   pmeth->lock = 2;
   rc = c->GlobalReference(c, args, pmeth, &gref, (async || pcon->net_connection || pcon->tlevel));
   DBX_DB_CHECK(rc);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::unlock");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_unlock;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   if (pcon->net_connection) {
      rc = dbx_unlock(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_unlock;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = pcon->p_ydb_so->p_ydb_lock_decr_s(&(pmeth->args[0].svalue), pmeth->cargc - 1, &pmeth->yargs[0]);
         if (rc == YDB_OK)
            retval = 1;
         else
            retval = 0;
         dbx_create_string(&(pmeth->output_val.svalue), (void *) &retval, DBX_DTYPE_INT);
      }
   }
   else {
      int locktype;
      locktype = CACHE_INCREMENTAL_LOCK;
      rc =  pcon->p_isc_so->p_CacheReleaseLock(pmeth->cargc - 1, locktype);
      if (rc == CACHE_SUCCESS)
         retval = 1;
      else
         retval = 0;
      dbx_create_string(&(pmeth->output_val.svalue), (void *) &retval, DBX_DTYPE_INT);
   }

   if (rc != CACHE_SUCCESS) {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::unlock");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Merge(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, argc, otype, len, nx, fc, mn, ismglobal, mglobal1;
   char *p;
   char buffer[32];
   DBXCON *pcon;
   DBXMETH *pmeth;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   mglobal *gx1;
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBXVAL *pval;
   DBX_GET_ICONTEXT;
   Local<Object> obj;
   Local<String> str;
   Local<String> result;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::merge");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Merge method takes at least one argument (the global to merge from)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   nx = 0;
   mglobal1 = 0;
   pmeth->args[nx].type = DBX_DTYPE_STR;
   pmeth->args[nx].sort = DBX_DSORT_GLOBAL;
   gx->global_name16_len ? dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) gx->global_name16, (int) gx->global_name16_len, 1, 2) : dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) gx->global_name, (int) gx->global_name_len, 0, 2);
   nx ++;
   if ((pval = gx->pkey)) {
      while (pval) {
         pmeth->args[nx].sort = DBX_DSORT_DATA;
         pmeth->args[nx].cvalue.pstr = 0;
         if (pval->type == DBX_DTYPE_INT) {
            pmeth->args[nx].type = DBX_DTYPE_INT;
            pmeth->args[nx].num.int32 = (int) pval->num.int32;
            T_SPRINTF(buffer, _dbxso(buffer), "%d", pval->num.int32);
            dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) buffer, (int) strlen(buffer), 0, 2);
         }
         else {
            if (pval->type == DBX_DTYPE_STR16)
               dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) pval->cvalue.buf16_addr, (int) pval->cvalue.len_used, 1, 2);
            else
               dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) pval->svalue.buf_addr, (int) pval->svalue.len_used, 0, 2);
         }
         nx ++;
         pval = pval->pnext;
      }
   }

   for (argc = 0; argc < pmeth->argc; argc ++) {
      obj = dbx_is_object(args[argc], &otype);
      if (otype) {
         ismglobal = 0;
         fc = obj->InternalFieldCount();
         if (fc == 3) {
/* 2.4.29 */
#if DBX_NODE_VERSION >= 220000
            mn = obj->GetInternalField(2).As<v8::Value>().As<v8::External>()->Int32Value(icontext).FromJust();
#else
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
#endif
            if (mn == DBX_MAGIC_NUMBER_MGLOBAL) {
               ismglobal = 1;
               mglobal1 ++;
               gx1 = ObjectWrap::Unwrap<mglobal>(obj);
               pmeth->args[nx].type = DBX_DTYPE_STR;
               pmeth->args[nx].sort = DBX_DSORT_GLOBAL;
               gx->global_name16_len ? dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) gx1->global_name16, (int) gx1->global_name16_len, 1, 2) : dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) gx1->global_name, (int) strlen(gx1->global_name), 0, 2);
               nx ++;
               if ((pval = gx1->pkey)) {
                  while (pval) {
                     pmeth->args[nx].sort = DBX_DSORT_DATA;
                     pmeth->args[nx].cvalue.pstr = 0;
                     if (pval->type == DBX_DTYPE_INT) {
                        pmeth->args[nx].type = DBX_DTYPE_INT;
                        pmeth->args[nx].num.int32 = (int) pval->num.int32;
                        T_SPRINTF(buffer, _dbxso(buffer), "%d", pval->num.int32);
                        dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) buffer, (int) strlen(buffer), 0, 2);
                     }
                     else {
                        if (pval->type == DBX_DTYPE_STR16)
                           dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) pval->cvalue.buf16_addr, (int) pval->cvalue.len_used, 1, 2);
                        else
                           dbx_ibuffer_add(pmeth, isolate, nx, str, (void *) pval->svalue.buf_addr, (int) pval->svalue.len_used, 0, 2);
                     }
                     nx ++;
                     pval = pval->pnext;
                  }
               }
            }
         }
         if (ismglobal == 0) {
            pmeth->args[nx].sort = DBX_DSORT_DATA;
            if (otype == 2) {
               p = node::Buffer::Data(obj);
               len = (int) node::Buffer::Length(obj);
               dbx_ibuffer_add(pmeth, isolate, nx ++, str, (void *) p, (int) len, 0, 2);
            }
            else {
               str = DBX_TO_STRING(args[argc]);
               dbx_ibuffer_add(pmeth, isolate, nx ++, str, NULL, 0, 0, 2);
            }
         }
      }
      else {
         pmeth->args[nx].sort = DBX_DSORT_DATA;
         str = DBX_TO_STRING(args[argc]);
         dbx_ibuffer_add(pmeth, isolate, nx ++, str, NULL, 0, 0, 2);
      }
   }

   pmeth->argc = nx;
/*
   {
      int n;
      for (n = 0; n < nx; n ++) {
         printf("\r\n pmeth->argc=%d; n=%d; type=%d; sort=%d; len=%d; s=%s;", pmeth->argc, n, pmeth->args[n].type, pmeth->args[n].sort, pmeth->args[n].svalue.len_used, pmeth->args[n].svalue.buf_addr);
      }
   }
*/

   if (mglobal1 == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The global to merge from is not specified", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DBFUN_START(c, pcon, pmeth);

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mglobal::merge");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->gx = (void *) gx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_merge;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   rc = dbx_merge(pmeth);

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pmeth->output_val.svalue), (void *) &rc, DBX_DTYPE_INT);
   }
   else {
      dbx_error_message(pmeth, rc);
      if (pcon->error_mode == 1) { /* v2.2.21 */
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) pcon->error, 1)));
      }
   }

   DBX_DBFUN_END(c);

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mglobal::merge");
   }

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Reset(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::reset");
   }
   pmeth = dbx_request_memory(pcon, 1, 1);

   pmeth->argc = args.Length();

   if (pmeth->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   /* 1.4.10 */
   rc = dbx_global_reset(args, isolate, pcon, pmeth, (void *) gx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mglobal::Close(const FunctionCallbackInfo<Value>& args)
{
   DBXCON *pcon;
   DBXMETH *pmeth;
   DBXVAL *pval, *pvalp;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) gx, (char *) "mglobal::close");
   }
   pmeth = dbx_request_memory(pcon, 1, 0);

   pmeth->argc = args.Length();

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   if (pmeth->argc > 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Closing a global template does not take any arguments", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   pval = gx->pkey;
   while (pval) {
      pvalp = pval;
      pval = pval->pnext;
      dbx_free((void *) pvalp, 0);
   }
   gx->pkey = NULL;
/*
   cx->delete_mglobal_template(gx);
*/
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}

