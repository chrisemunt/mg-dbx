/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2017 M/Gateway Developments Ltd,                      |
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
   delete_class_template(this);
}


void mglobal::Init()
{
#if DBX_NODE_VERSION >= 120000
   /* Isolate* isolate = target->GetIsolate(); */
   Isolate* isolate = Isolate::GetCurrent();

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
   DBX_GET_SCOPE;
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

      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, obj->New);
   }
   else {
      /* Invoked as plain function `mglobal(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);

#if DBX_NODE_VERSION >= 100000
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
#else
      args.GetReturnValue().Set(cons->NewInstance(argc, argv));
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
   Local<String> data_str = String::NewFromUtf8(isolate, data);

#if 0
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), s);
#else
   v8::Local<v8::FunctionTemplate> tx = v8::FunctionTemplate::New(isolate, callback, data_str, s);
#endif
   v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
   tx->SetClassName(fn_name);
   t->PrototypeTemplate()->Set(fn_name, tx);
#else
   NODE_SET_PROTOTYPE_METHOD(t, name, callback);
#endif

   return;
}


mglobal * mglobal::NewInstance(const FunctionCallbackInfo<Value>& args, char *cls_str, char *cls_info)
{
   DBX_GET_SCOPE;
#if DBX_NODE_VERSION >= 100000
   Local<Value> argv[2];
#else
   Handle<Value> argv[2];
#endif
   const unsigned argc = 2;

#if DBX_NODE_VERSION >= 100000
   Local<String> cls_info_str = String::NewFromOneByte(isolate, (uint8_t *) cls_info, NewStringType::kInternalized).ToLocalChecked();
#else
   Local<String> cls_info_str = String::NewFromOneByte(isolate, (uint8_t *) cls_info);
#endif

   argv[0] = args[0];
   argv[1] = cls_info_str;

   Local<Function> cons = Local<Function>::New(isolate, constructor);

#if DBX_NODE_VERSION >= 100000
   Local<Object> instance = cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked();
#else
   Local<Object> instance = cons->NewInstance(argc, argv);
#endif
 
   mglobal *cls = ObjectWrap::Unwrap<mglobal>(instance);

   DBX_WRITE_UTF8(DBX_TO_STRING(args[0]), cls->global_name);

   args.GetReturnValue().Set(instance);

   return cls;
}


int mglobal::delete_class_template(mglobal *cx)
{
   return 0;
}


void mglobal::Get(const FunctionCallbackInfo<Value>& args)
{
   DBX_GET_SCOPE;
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Get")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_get;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }


   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_get_s(&(pcon->args[0].svalue), pcon->argc, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalGet(pcon->argc, 0);
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
   DBX_GET_SCOPE;
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Set")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_set;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_set_s(&(pcon->args[0].svalue), pcon->argc - 1, &pcon->yargs[0], &(pcon->args[pcon->argc].svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalSet(pcon->argc - 1);
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
   DBX_GET_SCOPE;
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Defined")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_defined;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_data_s(&(pcon->args[0].svalue), pcon->argc, &pcon->yargs[0], (unsigned int *) &n);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalData(pcon->argc, 0);
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
   DBX_GET_SCOPE;
   short async;
   int rc, n;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Delete")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_delete;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_delete_s(&(pcon->args[0].svalue), pcon->argc, &pcon->yargs[0], YDB_DEL_TREE);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalKill(pcon->argc, 0);
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
   DBX_GET_SCOPE;
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Next")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_next;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pcon->args[0].svalue), pcon->argc, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->argc, 1, 0);
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
   DBX_GET_SCOPE;
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> result;
   DBXGREF gref;
   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;
   gref.global = gx->global_name;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments on Previous")));
   }
   
   DBX_DBFUN_START(c, pcon);

   rc = c->GlobalReference(args, pcon, &gref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (void * (*) (struct tagDBXCON * pcon)) dbx_next;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      gx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, error)));
      }
      return;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pcon->args[0].svalue), pcon->argc, &pcon->yargs[0], &(pcon->output_val.svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pcon->argc, -1, 0);
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



void mglobal::Close(const FunctionCallbackInfo<Value>& args)
{
   DBX_GET_SCOPE;
   short async;
   DBXCON *pcon;

   mglobal *gx = ObjectWrap::Unwrap<mglobal>(args.This());
   DBX_DBNAME *c = gx->c;
   gx->m_count ++;

   pcon = c->pcon;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Too many arguments")));
   }
   if (pcon->argc > 0) {
      isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, "Closing a class template does not take any arguments")));
   }

   //cx->delete_class_template(gx);

   return;
}



