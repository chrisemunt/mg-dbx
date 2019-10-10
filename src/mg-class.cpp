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
#include "mg-class.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mclass::constructor;

mclass::mclass(double value) : value_(value)
{
}


mclass::~mclass()
{
   delete_mclass_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mclass::Init(Local<Object> target)
#else
void mclass::Init(Handle<Object> target)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = target->GetIsolate();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mclass", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mclass"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHODC("classmethod", ClassMethod);
   DBX_NODE_SET_PROTOTYPE_METHODC("method", Method);
   DBX_NODE_SET_PROTOTYPE_METHODC("setproperty", SetProperty);
   DBX_NODE_SET_PROTOTYPE_METHODC("getproperty", GetProperty);
   DBX_NODE_SET_PROTOTYPE_METHODC("reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHODC("_close", Close);

#if DBX_NODE_VERSION >= 120000
   Local<Context> icontext = isolate->GetCurrentContext();
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
#else
   constructor.Reset(isolate, tpl->GetFunction());
#endif

}


void mclass::New(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
#if DBX_NODE_VERSION >= 100000
   Local<Context> icontext = isolate->GetCurrentContext();
#endif
   HandleScope scope(isolate);

   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mclass(...)` */
      double value = args[0]->IsUndefined() ? 0 : DBX_NUMBER_VALUE(args[0]);
      mclass * obj = new mclass(value);
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
   }
   else {
      /* Invoked as plain function `mclass(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);

#if DBX_NODE_VERSION >= 100000
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
#else
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
#endif
   }

}


#if DBX_NODE_VERSION >= 100000
void mclass::dbx_set_prototype_method(Local<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
#else
void mclass::dbx_set_prototype_method(Handle<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
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


mclass * mclass::NewInstance(const FunctionCallbackInfo<Value>& args)
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
 
   mclass *clx = ObjectWrap::Unwrap<mclass>(instance);

   args.GetReturnValue().Set(instance);

   return clx;
}


int mclass::delete_mclass_template(mclass *clx)
{
   return 0;
}


void mclass::ClassMethod(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->m_count ++;

   pcon = c->pcon;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   cref.optype = 0;
   rc = c->ClassReference(c, args, pcon, &cref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_classmethod;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pcon->cargc - 2);

   if (rc == CACHE_SUCCESS) {
      rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (pcon->output_val.type != DBX_TYPE_OREF) {
      str = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
      args.GetReturnValue().Set(str);
      return;
   }

   DBX_DB_LOCK(rc, 0);
   mclass *clx1 = mclass::NewInstance(args);
   DBX_DB_UNLOCK(rc);

   clx1->c = c;
   clx1->pcon = c->pcon;
   clx1->oref =  pcon->output_val.num.oref;
   strcpy(clx1->class_name, "");

   return;
}


void mclass::Method(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->m_count ++;

   pcon = c->pcon;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   cref.optype = 1;
   rc = c->ClassReference(c, args, pcon, &cref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_method;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = pcon->p_isc_so->p_CacheInvokeMethod(pcon->cargc - 2);

   if (rc == CACHE_SUCCESS) {
      rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (pcon->output_val.type != DBX_TYPE_OREF) {
      str = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
      args.GetReturnValue().Set(str);
      return;
   }

   DBX_DB_LOCK(rc, 0);
   mclass *clx1 = mclass::NewInstance(args);
   DBX_DB_UNLOCK(rc);

   clx1->c = c;
   clx1->pcon = c->pcon;
   clx1->oref =  pcon->output_val.num.oref;
   strcpy(clx1->class_name, "");

   return;
}


void mclass::SetProperty(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->m_count ++;

   pcon = c->pcon;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   cref.optype = 2;
   rc = c->ClassReference(c, args, pcon, &cref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_setproperty;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = pcon->p_isc_so->p_CacheSetProperty();

   if (rc == CACHE_SUCCESS) {
      dbx_create_string(&(pcon->output_val.svalue), (void *) &rc, DBX_TYPE_INT);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   str = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(str);

   return;
}

void mclass::GetProperty(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->m_count ++;

   pcon = c->pcon;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   cref.optype = 3;
   rc = c->ClassReference(c, args, pcon, &cref, async);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_getproperty;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];
         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   rc = pcon->p_isc_so->p_CacheGetProperty();

   if (rc == CACHE_SUCCESS) {
      rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   str = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(str);

   return;
}

void mclass::Reset(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   char class_name[256];
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> str;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   DBX_DBNAME *c = clx->c;
   DBX_GET_ICONTEXT;
   clx->m_count ++;

   pcon = c->pcon;
   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The class:reset method takes at least one argument (the class name)", 1)));
      return;
   }

   c->dbx_write_char8(isolate, DBX_TO_STRING(args[0]), class_name, pcon->utf8);
   if (class_name[0] == '\0') {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The class:reset method takes at least one argument (the class name)", 1)));
      return;
   }

   DBX_DBFUN_START(c, pcon);

   rc = c->ClassReference(c, args, pcon, NULL, async);

   rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pcon->cargc - 2);

   if (rc == CACHE_SUCCESS) {
      rc = isc_pop_value(pcon, &(pcon->output_val), DBX_TYPE_STR);
   }
   else {
      dbx_error_message(pcon, rc);
   }

   DBX_DBFUN_END(c);
   DBX_DB_UNLOCK(rc);

   if (pcon->output_val.type != DBX_TYPE_OREF) {
      str = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
      args.GetReturnValue().Set(str);
      return;
   }

   clx->oref =  pcon->output_val.num.oref;
   strcpy(clx->class_name, class_name);

   return;
}

void mclass::Close(const FunctionCallbackInfo<Value>& args)
{

/*
   clx->delete_mclass_template(clx);
*/

   return;
}




