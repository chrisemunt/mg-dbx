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

#include "mg-dbx.h"
#include "mg-global.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mglobal::constructor;

mglobal::mglobal(double value) : value_(value)
{
}


mglobal::~mglobal()
{
   delete_mglobal_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mglobal::Init(Local<Object> target)
#else
void mglobal::Init(Handle<Object> target)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = target->GetIsolate();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mglobal", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mglobal"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHODC("get", Get);
   DBX_NODE_SET_PROTOTYPE_METHODC("set", Set);
   DBX_NODE_SET_PROTOTYPE_METHODC("defined", Defined);
   DBX_NODE_SET_PROTOTYPE_METHODC("delete", Delete);
   DBX_NODE_SET_PROTOTYPE_METHODC("next", Next);
   DBX_NODE_SET_PROTOTYPE_METHODC("previous", Previous);
   DBX_NODE_SET_PROTOTYPE_METHODC("increment", Increment);
   DBX_NODE_SET_PROTOTYPE_METHODC("lock", Lock);
   DBX_NODE_SET_PROTOTYPE_METHODC("unlock", Unlock);
   DBX_NODE_SET_PROTOTYPE_METHODC("reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHODC("_close", Close);

#if DBX_NODE_VERSION >= 120000
   Local<Context> icontext = isolate->GetCurrentContext();
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
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
   int len;
   char *cls_info;
   char buffer[256], cls_str[256];
   Local<String> cls_info_str;

   DBX_WRITE_UTF8(DBX_TO_STRING(args[0]), cls_str);
   cls_info_str = DBX_TO_STRING(args[1]);
   len = (int) cls_info_str->Length();

   cls_info = (char *) dbx_malloc(sizeof(char) * (len + 2048), 1001);
   DBX_WRITE_UTF8(cls_info_str, cls_info);
   cls_info[len] = '\0';

   T_STRCPY(buffer, _dbxso(buffer), "");
   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mglobal(...)` */
      double value = args[0]->IsUndefined() ? 0 : DBX_NUMBER_VALUE(args[0]);
      mglobal * obj = new mglobal(value);
      obj->Wrap(args.This());
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

#if DBX_NODE_VERSION >= 100000
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
#else
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
#endif
   }

   dbx_free((void *) cls_info, 1001);

}


#if DBX_NODE_VERSION >= 100000
void mglobal::dbx_set_prototype_method(Local<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
#else
void mglobal::dbx_set_prototype_method(Handle<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
#endif
{
#if DBX_NODE_VERSION >= 100000

   v8::Isolate* isolate = v8::Isolate::GetCurrent();
   v8::HandleScope handle_scope(isolate);
   v8::Local<v8::Signature> s = v8::Signature::New(isolate, t);

#if DBX_NODE_VERSION >= 120000
   Local<String> data_str = String::NewFromUtf8(isolate, data, NewStringType::kNormal).ToLocalChecked();
#else
   Local<String> data_str = String::NewFromUtf8(isolate, data);
#endif

#if 0
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), s);
#else
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, data_str, s);
#endif

#if DBX_NODE_VERSION >= 120000
   v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name, NewStringType::kNormal).ToLocalChecked();
#else
   v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
#endif

   tx->SetClassName(fn_name);
   t->PrototypeTemplate()->Set(fn_name, tx);
#else
   NODE_SET_PROTOTYPE_METHOD(t, name, callback);
#endif

   return;
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

   Local<Function> cons = Local<Function>::New(isolate, constructor);

#if DBX_NODE_VERSION >= 100000
   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked();
#else
   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked();
#endif
 
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
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Get", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(result);
}


void mglobal::Set(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Set", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Defined", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Delete", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);

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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Increment", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(result);
}


void mglobal::Lock(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc, retval;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Lock", 1)));
   }
   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Missing or invalid global name on Lock", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      int timeout;
      unsigned long long timeout_nsec;
      timeout = (int) strtol(pcon->args[pcon->cargc - 1].svalue.buf_addr, NULL,10);
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
      int timeout, locktype;
      locktype = CACHE_INCREMENTAL_LOCK;
      timeout = (int) strtol(pcon->args[pcon->cargc - 1].svalue.buf_addr, NULL, 10);
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
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
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;
   gref.pkey = gx->pkey;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Unlock", 1)));
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
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
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

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(result);
}


void mglobal::Reset(const FunctionCallbackInfo<Value>& args)
{
   int n, len, otype;
   char global_name[256], *p;
   DBXCON *pcon;
   DBXVAL *pval, *pvalp;
   Local<Object> obj;
   Local<String> str;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   DBX_GET_ICONTEXT;
   gx->m_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
   }

   c->dbx_write_char8(isolate, DBX_TO_STRING(args[0]), global_name, pcon->utf8);
   if (global_name[0] == '\0') {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The Reset method takes at least one argument (the global name)", 1)));
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
   for (n = 1; n < pcon->argc; n ++) {
      if (args[n]->IsInt32()) {
         pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL), 0);
         pval->type = DBX_TYPE_INT;
         pval->num.int32 = (int) DBX_INT32_VALUE(args[n]);
      }
      else {
         obj = c->dbx_is_object(args[n], &otype);
         if (otype == 2) {
            p = node::Buffer::Data(obj);
            len = (int) strlen(p);
            pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL) + len + 32, 0);
            pval->type = DBX_TYPE_STR;
            pval->svalue.buf_addr = ((char *) pval) + sizeof(DBXVAL);
            memcpy((void *) pval->svalue.buf_addr, (void *) p, (size_t) len);
            pval->svalue.len_alloc = len + 32;
            pval->svalue.len_used = len;
         }
         else {
            str = DBX_TO_STRING(args[n]);
            len = (int) c->dbx_string8_length(isolate, str, 0);
            pval = (DBXVAL *) dbx_malloc(sizeof(DBXVAL) + len + 32, 0);
            pval->type = DBX_TYPE_STR;
            pval->svalue.buf_addr = ((char *) pval) + sizeof(DBXVAL);
            c->dbx_write_char8(isolate, str, pval->svalue.buf_addr, 1);
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

   return;
}


void mglobal::Close(const FunctionCallbackInfo<Value>& args)
{
   DBXCON *pcon;
   DBXVAL *pval, *pvalp;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   DBX_GET_ISOLATE;
   gx->m_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
   }
   if (pcon->argc > 0) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Closing a global template does not take any arguments", 1)));
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

