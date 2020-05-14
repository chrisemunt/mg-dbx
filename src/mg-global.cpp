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
   Isolate* isolate = exports->GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();
/*
   Local<ObjectTemplate> mglobal_data_tpl = ObjectTemplate::New(isolate);
   mglobal_data_tpl->SetInternalFieldCount(1);
   Local<Object> mglobal_data = mglobal_data_tpl->NewInstance(icontext).ToLocalChecked();
*/
  /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mglobal", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* v1.4.10 */
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mglobal"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
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
   Local<Object> obj;

   /* 1.4.10 */
   argc = args.Length();
   if (argc > 0) {
      obj = dbx_is_object(args[0], &otype);
      if (otype) {
         fc = obj->InternalFieldCount();
         if (fc == 3) {
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
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
         c->pcon->argc = argc;
         obj->c = c;
         obj->pkey = NULL;
         obj->global_name[0] = '\0';
         rc = dbx_global_reset(args, isolate, (void *) obj, 1, 1);
         if (rc < 0) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobal::New() method takes at least one argument (the global name)", 1)));
            return;
         }
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


int mglobal::delete_mglobal_template(mglobal *cx)
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
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = binary;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Get", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_get;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Set(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Set", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_set;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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

   if (rc == CACHE_ERUNDEF) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) "", DBX_TYPE_STR);
   }
   else if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
}


void mglobal::Defined(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Defined", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_defined;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Delete(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Delete", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_next;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_next;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Increment(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Increment", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 0;
   pcon->increment = 1;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_increment;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Lock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval, timeout, locktype;
   unsigned long long timeout_nsec;
   char buffer[32];
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Lock", 1)));
      return;
   }
   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Missing or invalid global name on Lock", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 1;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_lock;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Unlock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Unlock", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   pcon->lock = 2;
   pcon->increment = 0;
   rc = c->GlobalReference(c, args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_unlock;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
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


void mglobal::Merge(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, argc, otype, len, nx, fc, mn, ismglobal, mglobal1;
   char *p;
   char buffer[32];
   DBXCON *pcon;
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
   pcon->binary = 0;
   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Merge method takes at least one argument (the global to merge from)", 1)));
      return;
   }

   nx = 0;
   mglobal1 = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->args[nx].type = DBX_TYPE_STR;
   pcon->args[nx].sort = DBX_DSORT_GLOBAL;
   dbx_ibuffer_add(pcon, isolate, nx, str, gx->global_name, (int) strlen(gx->global_name), 2);
   nx ++;
   if ((pval = gx->pkey)) {
      while (pval) {
         pcon->args[nx].sort = DBX_DSORT_DATA;
         pcon->args[nx].cvalue.pstr = 0;
         if (pval->type == DBX_TYPE_INT) {
            pcon->args[nx].type = DBX_TYPE_INT;
            pcon->args[nx].num.int32 = (int) pval->num.int32;
            T_SPRINTF(buffer, _dbxso(buffer), "%d", pval->num.int32);
            dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 2);
         }
         else {
            dbx_ibuffer_add(pcon, isolate, nx, str, pval->svalue.buf_addr, (int) pval->svalue.len_used, 2);
         }
         nx ++;
         pval = pval->pnext;
      }
   }

   for (argc = 0; argc < pcon->argc; argc ++) {
      obj = dbx_is_object(args[argc], &otype);
      if (otype) {
         ismglobal = 0;
         fc = obj->InternalFieldCount();
         if (fc == 3) {
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
            if (mn == DBX_MAGIC_NUMBER_MGLOBAL) {
               ismglobal = 1;
               mglobal1 ++;
               gx1 = ObjectWrap::Unwrap<mglobal>(obj);
               pcon->args[nx].type = DBX_TYPE_STR;
               pcon->args[nx].sort = DBX_DSORT_GLOBAL;
               dbx_ibuffer_add(pcon, isolate, nx, str, gx1->global_name, (int) strlen(gx1->global_name), 2);
               nx ++;
               if ((pval = gx1->pkey)) {
                  while (pval) {
                     pcon->args[nx].sort = DBX_DSORT_DATA;
                     pcon->args[nx].cvalue.pstr = 0;
                     if (pval->type == DBX_TYPE_INT) {
                        pcon->args[nx].type = DBX_TYPE_INT;
                        pcon->args[nx].num.int32 = (int) pval->num.int32;
                        T_SPRINTF(buffer, _dbxso(buffer), "%d", pval->num.int32);
                        dbx_ibuffer_add(pcon, isolate, nx, str, buffer, (int) strlen(buffer), 2);
                     }
                     else {
                        dbx_ibuffer_add(pcon, isolate, nx, str, pval->svalue.buf_addr, (int) pval->svalue.len_used, 2);
                     }
                     nx ++;
                     pval = pval->pnext;
                  }
               }
            }
         }
         if (ismglobal == 0) {
            pcon->args[nx].sort = DBX_DSORT_DATA;
            if (otype == 2) {
               p = node::Buffer::Data(obj);
               len = (int) node::Buffer::Length(obj);
               dbx_ibuffer_add(pcon, isolate, nx ++, str, p, (int) len, 2);
            }
            else {
               str = DBX_TO_STRING(args[argc]);
               dbx_ibuffer_add(pcon, isolate, nx ++, str, NULL, 0, 2);
            }
         }
      }
      else {
         pcon->args[nx].sort = DBX_DSORT_DATA;
         str = DBX_TO_STRING(args[argc]);
         dbx_ibuffer_add(pcon, isolate, nx ++, str, NULL, 0, 2);
      }
   }

   pcon->argc = nx;
/*
   {
      int n;
      for (n = 0; n < nx; n ++) {
         printf("\r\n pcon->argc=%d; n=%d; type=%d; sort=%d; len=%d; s=%s;", pcon->argc, n, pcon->args[n].type, pcon->args[n].sort, pcon->args[n].svalue.len_used, pcon->args[n].svalue.buf_addr);
      }
   }
*/

   if (mglobal1 == 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The global to merge from is not specified", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_merge;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = dbx_merge(pcon);

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);

   result = dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   return;
}


void mglobal::Reset(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
      return;
   }

   /* 1.4.10 */
   rc = dbx_global_reset(args, isolate, (void *) gx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
      return;
   }

   return;
}


void mglobal::Close(const FunctionCallbackInfo<Value>& args)
{
   DBXCON *pcon;
   DBXVAL *pval, *pvalp;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   MG_GLOBAL_CHECK_CLASS(gx);
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->dbx_count ++;

   pcon = c->pcon;
   pcon->binary = 0;
   pcon->lock = 0;
   pcon->increment = 0;
   pcon->argc = args.Length();

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
      return;
   }
   if (pcon->argc > 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Closing a global template does not take any arguments", 1)));
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
   return;
}

int dbx_escape_output(DBXSTR *pdata, char *item, int item_len, short context)
{
   int n;

   if (context == 0) {
      for (n = 0; n < item_len; n ++) {
         pdata->buf_addr[pdata->len_used ++] = item[n];
      }
      return pdata->len_used;
   }

   for (n = 0; n < item_len; n ++) {
      if (item[n] == '&') {
         pdata->buf_addr[pdata->len_used ++] = '%';
         pdata->buf_addr[pdata->len_used ++] = '2';
         pdata->buf_addr[pdata->len_used ++] = '6';
      }
      else if (item[n] == '=') {
         pdata->buf_addr[pdata->len_used ++] = '%';
         pdata->buf_addr[pdata->len_used ++] = '3';
         pdata->buf_addr[pdata->len_used ++] = 'D';
      }
      else {
         pdata->buf_addr[pdata->len_used ++] = item[n];
      }
   }
   return pdata->len_used;
}

