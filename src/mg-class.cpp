/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2021 M/Gateway Developments Ltd,                      |
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

mclass::mclass(int value) : dbx_count(value)
{
}


mclass::~mclass()
{
   delete_mclass_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mclass::Init(Local<Object> exports)
#else
void mclass::Init(Handle<Object> exports)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = exports->GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mclass", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* 2.0.14 */
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mclass"));
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* v2.3.24 */
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "classmethod", ClassMethod);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "classmethod_bx", ClassMethod_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "method", Method);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "method_bx", Method_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "setproperty", SetProperty);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "getproperty", GetProperty);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "getproperty_bx", GetProperty_bx);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "_close", Close);

#if DBX_NODE_VERSION >= 120000
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
   exports->Set(icontext, String::NewFromUtf8(isolate, "mclass", NewStringType::kNormal).ToLocalChecked(), tpl->GetFunction(icontext).ToLocalChecked()).FromJust();
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
            mn = DBX_INT32_VALUE(obj->GetInternalField(2));
            if (mn == DBX_MAGIC_NUMBER) {
               c = ObjectWrap::Unwrap<DBX_DBNAME>(obj);
            }
         }
      }
   }

   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mclass(...)` */
      int value = args[0]->IsUndefined() ? 0 : DBX_INT32_VALUE(args[0]);
      mclass * obj = new mclass(value);
      obj->c = NULL;

      if (c) { /* 1.4.10 */
         if (c->pcon == NULL) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "No Connection to the database", 1)));
            return;
         }
         obj->c = c;
         pcon = c->pcon;
         pmeth = dbx_request_memory(pcon, 1);
         pmeth->argc = argc;
         if (argc > 1) {
            dbx_write_char8(isolate, DBX_TO_STRING(args[1]), obj->class_name, obj->c->pcon->utf8);
            if (argc > 2) {
               DBX_DBFUN_START(c, c->pcon, pmeth);
               pmeth->ibuffer_used = 0;
               rc = c->ClassReference(c, args, pmeth, NULL, 1, pcon->net_connection);
               if (c->pcon->net_connection) {
                  rc = dbx_classmethod(pmeth);
               }
               else {
                  rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pmeth->cargc - 2);
                  if (rc == CACHE_SUCCESS) {
                     rc = isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
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
               if (pmeth->output_val.type == DBX_DTYPE_OREF) {
                  obj->oref =  pmeth->output_val.num.oref;
               }
            }
         }
         dbx_request_memory_free(pcon, pmeth, 0);
      }
      obj->Wrap(args.This());
      args.This()->SetInternalField(2, DBX_INTEGER_NEW(DBX_MAGIC_NUMBER_MCLASS)); /* 2.0.14 */
      args.GetReturnValue().Set(args.This());
   }
   else {
      /* Invoked as plain function `mclass(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
   }

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

   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked();
   mclass *clx = ObjectWrap::Unwrap<mclass>(instance);

   args.GetReturnValue().Set(instance);

   return clx;
}


int mclass::async_callback(mclass *clx)
{
   clx->Unref();
   return 0;
}


int mclass::delete_mclass_template(mclass *clx)
{
   return 0;
}


void mclass::ClassMethod(const FunctionCallbackInfo<Value>& args)
{
   return ClassMethodEx(args, 0);
}


void mclass::ClassMethod_bx(const FunctionCallbackInfo<Value>& args)
{
   return ClassMethodEx(args, 1);
}


void mclass::ClassMethodEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   MG_CLASS_CHECK_CLASS(clx);
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) clx, (char *) "mclass::classmethod");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->binary = binary;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   cref.optype = 0;
   rc = c->ClassReference(c, args, pmeth, &cref, 0, (async || pcon->net_connection));

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mclass::classmethod");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->clx = (void *) clx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_classmethod;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
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
      rc = dbx_classmethod(pmeth);
   }
   else {
      rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pmeth->cargc - 2);
      if (rc == CACHE_SUCCESS) {
         rc = isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
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
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mclass::classmethod");
   }

   if (pmeth->output_val.type != DBX_DTYPE_OREF) {
      if (binary) {
         Local<Object> bx = node::Buffer::New(isolate, (char *) pmeth->output_val.svalue.buf_addr, (size_t) pmeth->output_val.svalue.len_used).ToLocalChecked();
         args.GetReturnValue().Set(bx);
      }
      else {
         str = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
         args.GetReturnValue().Set(str);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DB_LOCK(0);
   mclass *clx1 = mclass::NewInstance(args);
   DBX_DB_UNLOCK();

   clx1->c = c;
   clx1->oref =  pmeth->output_val.num.oref;
   strcpy(clx1->class_name, "");
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mclass::Method(const FunctionCallbackInfo<Value>& args)
{
   return MethodEx(args, 0);
}


void mclass::Method_bx(const FunctionCallbackInfo<Value>& args)
{
   return MethodEx(args, 1);
}


void mclass::MethodEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   MG_CLASS_CHECK_CLASS(clx);
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) clx, (char *) "mclass::method");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->binary = binary;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   cref.optype = 1;
   rc = c->ClassReference(c, args, pmeth, &cref, 0, (async || pcon->net_connection));

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mclass::method");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->clx = (void *) clx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_method;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
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
      rc = dbx_method(pmeth);
   }
   else {
      rc = pcon->p_isc_so->p_CacheInvokeMethod(pmeth->cargc - 2);
      if (rc == CACHE_SUCCESS) {
         rc = isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
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
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mclass::method");
   }

   if (pmeth->output_val.type != DBX_DTYPE_OREF) {
      if (binary) {
         Local<Object> bx = node::Buffer::New(isolate, (char *) pmeth->output_val.svalue.buf_addr, (size_t) pmeth->output_val.svalue.len_used).ToLocalChecked();
         args.GetReturnValue().Set(bx);
      }
      else {
         str = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
         args.GetReturnValue().Set(str);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DB_LOCK(0);
   mclass *clx1 = mclass::NewInstance(args);
   DBX_DB_UNLOCK();

   clx1->c = c;
   clx1->oref =  pmeth->output_val.num.oref;
   strcpy(clx1->class_name, "");
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mclass::SetProperty(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   MG_CLASS_CHECK_CLASS(clx);
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) clx, (char *) "mclass::setproperty");
   }
   pmeth = dbx_request_memory(pcon, 0);

   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   cref.optype = 2;
   rc = c->ClassReference(c, args, pmeth, &cref, 0, (async || pcon->net_connection));

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mclass::setproperty");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->clx = (void *) clx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_setproperty;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
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
      rc = dbx_setproperty(pmeth);
   }
   else {
      rc = pcon->p_isc_so->p_CacheSetProperty();
   }

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
   DBX_DB_UNLOCK();

   if (pcon->log_transmissions == 2) {
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mclass::setproperty");
   }

   str = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(str);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mclass::GetProperty(const FunctionCallbackInfo<Value>& args)
{
   return GetPropertyEx(args, 0);
}


void mclass::GetProperty_bx(const FunctionCallbackInfo<Value>& args)
{
   return GetPropertyEx(args, 1);
}


void mclass::GetPropertyEx(const FunctionCallbackInfo<Value>& args, int binary)
{
   short async;
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> str;
   DBXCREF cref;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   MG_CLASS_CHECK_CLASS(clx);
   DBX_DBNAME *c = clx->c;
   DBX_GET_ISOLATE;
   clx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) clx, (char *) "mclass::getproperty");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->binary = binary;
   cref.class_name = clx->class_name;
   cref.oref = clx->oref;

   DBX_CALLBACK_FUN(pmeth->argc, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on ClassMethod", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   cref.optype = 3;
   rc = c->ClassReference(c, args, pmeth, &cref, 0, (async || pcon->net_connection));

   if (pcon->log_transmissions) {
      dbx_log_transmission(pcon, pmeth, (char *) "mclass::getproperty");
   }

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->clx = (void *) clx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_getproperty;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);
      baton->cb.Reset(isolate, cb);
      clx->Ref();
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
      rc = dbx_getproperty(pmeth);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGetProperty();
      if (rc == CACHE_SUCCESS) {
         rc = isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
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
      dbx_log_response(pcon, (char *) pmeth->output_val.svalue.buf_addr, (int) pmeth->output_val.svalue.len_used, (char *) "mclass::getproperty");
   }

   if (pmeth->output_val.type != DBX_DTYPE_OREF) {
      if (binary) {
         Local<Object> bx = node::Buffer::New(isolate, (char *) pmeth->output_val.svalue.buf_addr, (size_t) pmeth->output_val.svalue.len_used).ToLocalChecked();
         args.GetReturnValue().Set(bx);
      }
      else {
         str = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
         args.GetReturnValue().Set(str);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   /* 2.0.14 */

   DBX_DB_LOCK(0);
   mclass *clx1 = mclass::NewInstance(args);
   DBX_DB_UNLOCK();

   clx1->c = c;
   clx1->oref =  pmeth->output_val.num.oref;
   strcpy(clx1->class_name, "");
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mclass::Reset(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int rc;
   char class_name[256];
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<Object> obj;
   Local<String> str;
   mclass *clx = ObjectWrap::Unwrap<mclass>(args.This());
   MG_CLASS_CHECK_CLASS(clx);
   DBX_DBNAME *c = clx->c;
   DBX_GET_ICONTEXT;
   clx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) clx, (char *) "mclass::reset");
   }
   pmeth = dbx_request_memory(pcon, 0);

   DBX_CALLBACK_FUN(pmeth->argc, async);

   pmeth->argc = args.Length();

   if (pmeth->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The class:reset method takes at least one argument (the class name)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   dbx_write_char8(isolate, DBX_TO_STRING(args[0]), class_name, pcon->utf8);
   if (class_name[0] == '\0') {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The class:reset method takes at least one argument (the class name)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   DBX_DBFUN_START(c, pcon, pmeth);

   rc = c->ClassReference(c, args, pmeth, NULL, 0, (async || pcon->net_connection));

   if (pcon->net_connection) {
      rc = dbx_classmethod(pmeth);
   }
   else {
      rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pmeth->cargc - 2);
      if (rc == CACHE_SUCCESS) {
         rc = isc_pop_value(pmeth, &(pmeth->output_val), DBX_DTYPE_STR);
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

   if (pmeth->output_val.type != DBX_DTYPE_OREF) {
      str = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
      args.GetReturnValue().Set(str);
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   clx->oref =  pmeth->output_val.num.oref;
   strcpy(clx->class_name, class_name);

   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mclass::Close(const FunctionCallbackInfo<Value>& args)
{

/*
   clx->delete_mclass_template(clx);
*/

   return;
}




