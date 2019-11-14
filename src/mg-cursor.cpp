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
#include "mg-cursor.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mcursor::constructor;

mcursor::mcursor(double value) : value_(value)
{
}


mcursor::~mcursor()
{
   delete_mcursor_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mcursor::Init(Local<Object> target)
#else
void mcursor::Init(Handle<Object> target)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = target->GetIsolate();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHODC("execute", Execute);
   DBX_NODE_SET_PROTOTYPE_METHODC("cleanup", Cleanup);
   DBX_NODE_SET_PROTOTYPE_METHODC("next", Next);
   DBX_NODE_SET_PROTOTYPE_METHODC("previous", Previous);
   DBX_NODE_SET_PROTOTYPE_METHODC("reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHODC("_close", Close);

#if DBX_NODE_VERSION >= 120000
   Local<Context> icontext = isolate->GetCurrentContext();
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
#else
   constructor.Reset(isolate, tpl->GetFunction());
#endif

}


void mcursor::New(const FunctionCallbackInfo<Value>& args)
{
   Isolate* isolate = args.GetIsolate();
#if DBX_NODE_VERSION >= 100000
   Local<Context> icontext = isolate->GetCurrentContext();
#endif
   HandleScope scope(isolate);

   if (args.IsConstructCall()) {
      /* Invoked as constructor: `new mcursor(...)` */
      double value = args[0]->IsUndefined() ? 0 : DBX_NUMBER_VALUE(args[0]);
      mcursor * obj = new mcursor(value);
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
   }
   else {
      /* Invoked as plain function `mcursor(...)`, turn into construct call. */
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
void mcursor::dbx_set_prototype_method(Local<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
#else
void mcursor::dbx_set_prototype_method(Handle<FunctionTemplate> t, FunctionCallback callback, const char* name, const char* data)
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


mcursor * mcursor::NewInstance(const FunctionCallbackInfo<Value>& args)
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
 
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(instance);

   args.GetReturnValue().Set(instance);

   return cx;
}


int mcursor::delete_mcursor_template(mcursor *cx)
{
   return 0;
}


void mcursor::Execute(const FunctionCallbackInfo<Value>& args)
{
   short async;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->m_count ++;

   pcon = c->pcon;
   pcon->psql = cx->psql;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Execute", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_sql_execute;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback_sql_execute, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   dbx_sql_execute(pcon);

   DBX_DBFUN_END(c);

   obj = DBX_OBJECT_NEW();

   key = c->dbx_new_string8(isolate, (char *) "sqlcode", 0);
   DBX_SET(obj, key, DBX_INTEGER_NEW(pcon->psql->sqlcode));
   key = c->dbx_new_string8(isolate, (char *) "sqlstate", 0);
   DBX_SET(obj, key, c->dbx_new_string8(isolate, pcon->psql->sqlstate, 0));
   if (pcon->error[0]) {
      key = c->dbx_new_string8(isolate, (char *) "error", 0);
      DBX_SET(obj, key, c->dbx_new_string8(isolate, pcon->error, 0));
   }

   args.GetReturnValue().Set(obj);
}


void mcursor::Cleanup(const FunctionCallbackInfo<Value>& args)
{
   short async;
   DBXCON *pcon;
   Local<String> result;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->m_count ++;

   pcon = c->pcon;
   pcon->psql = cx->psql;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Cleanup", 1)));
      return;
   }
   
   DBX_DBFUN_START(c, pcon);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pcon);
      baton->pcon->p_dbxfun = (int (*) (struct tagDBXCON * pcon)) dbx_sql_cleanup;
      Local<Function> cb = Local<Function>::Cast(args[pcon->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pcon);
         isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, error, 1)));
         return;
      }
      return;
   }

   dbx_sql_cleanup(pcon);

   DBX_DBFUN_END(c);

   result = c->dbx_new_string8n(isolate, pcon->output_val.svalue.buf_addr, pcon->output_val.svalue.len_used, c->utf8);
   args.GetReturnValue().Set(result);
   return;
}


void mcursor::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->m_count ++;

   pcon = c->pcon;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_order(pcon, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used == 0) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else if (cx->getdata == 0) {
/*
         if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used && cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used < 10) {
            int n;
            char buffer[32];
            strncpy(buffer, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used);
            buffer[cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used] = '\0';
            n = (int) strtol(buffer, NULL, 10);
            args.GetReturnValue().Set(DBX_INTEGER_NEW(n));
            return;
         }
*/
         key = c->dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&(cx->data), (char *) "key=", 4, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&(cx->data), (char *) "&data=", 6, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = c->dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = c->dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8));
            key = c->dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_query(pcon, cx->pqr_next, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->format == 1) {
         char buffer[32], delim[4];

         cx->data.len_used = 0;
         *delim = '\0';
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            sprintf(buffer, (char *) "%skey%d=", delim, n + 1);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 1);
            strcpy(delim, (char *) "&");
         }
         if (cx->getdata) {
            sprintf(buffer, (char *) "%sdata=", delim);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 1);
         }

         key = c->dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();
         key = c->dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);

         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, c->dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = c->dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
         }
      }

      pqr = cx->pqr_next;
      cx->pqr_next = cx->pqr_prev;
      cx->pqr_prev = pqr;

      if (eod == CACHE_SUCCESS) {
         if (cx->format == 1)
            args.GetReturnValue().Set(key);
         else
            args.GetReturnValue().Set(obj);
      }
      else {
         args.GetReturnValue().Set(DBX_NULL());
      }
      return;
   }
   else if (cx->context == 9) {   
      eod = dbx_global_directory(pcon, cx->pqr_prev, 1, &(cx->counter));

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = c->dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      return;
   }
   else if (cx->context == 11) {

      if (!pcon->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      eod = dbx_sql_row(pcon, pcon->psql->row_no, 1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pcon->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size(&(pcon->output_val.svalue), pcon->output_val.offs, &dsort, &dtype);
            pcon->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pcon->output_val.offs, dsort, dtype, pcon->output_val.svalue.buf_addr + pcon->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = c->dbx_new_string8n(isolate, (char *) pcon->psql->cols[n]->name.buf_addr, pcon->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate,  pcon->output_val.svalue.buf_addr + pcon->output_val.offs, len, 0));
            pcon->output_val.offs += len;
         }

         args.GetReturnValue().Set(obj);
      }
      return;
   }
}


void mcursor::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->m_count ++;

   pcon = c->pcon;

   DBX_CALLBACK_FUN(pcon->argc, cb, async);

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_order(pcon, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used == 0) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else if (cx->getdata == 0) {
         key = c->dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&cx->data, (char *) "key=", 4, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&cx->data, (char *) "&data=", 6, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = c->dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = c->dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, c->utf8));
            key = c->dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon);
      DBX_DB_LOCK(n, 0);

      eod = dbx_global_query(pcon, cx->pqr_next, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK(n);

      if (cx->format == 1) {
         char buffer[32], delim[4];

         cx->data.len_used = 0;
         *delim = '\0';
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            sprintf(buffer, (char *) "%skey%d=", delim, n + 1);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 1);
            strcpy(delim, (char *) "&");
         }
         if (cx->getdata) {
            sprintf(buffer, (char *) "%sdata=", delim);
            dbx_escape_output(&(cx->data), buffer, (int) strlen(buffer), 0);
            dbx_escape_output(&(cx->data), cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 1);
         }

         key = c->dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();

         key = c->dbx_new_string8(isolate, (char *) "global", 0);
         DBX_SET(obj, key, c->dbx_new_string8(isolate, cx->pqr_next->global_name.buf_addr, 0));
         key = c->dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, c->dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = c->dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
         }
      }

      pqr = cx->pqr_next;
      cx->pqr_next = cx->pqr_prev;
      cx->pqr_prev = pqr;

      if (eod == CACHE_SUCCESS) {
         if (cx->format == 1)
            args.GetReturnValue().Set(key);
         else
            args.GetReturnValue().Set(obj);
      }
      else {
         args.GetReturnValue().Set(DBX_NULL());
      }
      return;
   }
   else if (cx->context == 9) {
   
      eod = dbx_global_directory(pcon, cx->pqr_prev, -1, &(cx->counter));

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = c->dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, c->utf8);
         args.GetReturnValue().Set(key);
      }
      return;
   }
   else if (cx->context == 11) {

      if (!pcon->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         return;
      }

      eod = dbx_sql_row(pcon, pcon->psql->row_no, -1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pcon->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size(&(pcon->output_val.svalue), pcon->output_val.offs, &dsort, &dtype);
            pcon->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pcon->output_val.offs, dsort, dtype, pcon->output_val.svalue.buf_addr + pcon->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = c->dbx_new_string8n(isolate, (char *) pcon->psql->cols[n]->name.buf_addr, pcon->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, c->dbx_new_string8n(isolate,  pcon->output_val.svalue.buf_addr + pcon->output_val.offs, len, 0));
            pcon->output_val.offs += len;
         }
         args.GetReturnValue().Set(obj);
      }
      return;
   }
}


void mcursor::Reset(const FunctionCallbackInfo<Value>& args)
{
   int n, len;
   char global_name[256];
   DBXCON *pcon;
   Local<Object> obj;
   Local<String> key;
   Local<String> value;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->m_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc < 1) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global reference to start with)", 1)));
      return;
   }

   obj = DBX_TO_OBJECT(args[0]);
   key = c->dbx_new_string8(isolate, (char *) "global", 1);
   if (DBX_GET(obj, key)->IsString()) {
      c->dbx_write_char8(isolate, DBX_TO_STRING(DBX_GET(obj, key)), global_name, pcon->utf8);
   }
   else {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Missing global name in the global object", 1)));
      return;
   }
 
   if (global_name[0] == '\0') {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global name)", 1)));
      return;
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
   strcpy(cx->pqr_next->global_name.buf_addr, cx->global_name);
   strcpy(cx->pqr_prev->global_name.buf_addr, cx->global_name);
   cx->pqr_prev->global_name.len_alloc = (int) strlen((char *) cx->pqr_prev->global_name.buf_addr);
   cx->pqr_next->global_name.len_alloc =  cx->pqr_prev->global_name.len_alloc;

   cx->pqr_prev->keyn = 0;
   key = c->dbx_new_string8(isolate, (char *) "key", 1);
   if (DBX_GET(obj, key)->IsArray()) {
      Local<Array> a = Local<Array>::Cast(DBX_GET(obj, key));
      cx->pqr_prev->keyn = (int) a->Length();
      for (n = 0; n < cx->pqr_prev->keyn; n ++) {
         value = DBX_TO_STRING(DBX_GET(a, n));
         len = (int) c->dbx_string8_length(isolate, value, 0);
         c->dbx_write_char8(isolate, value, cx->pqr_prev->key[n].buf_addr, 1);
         cx->pqr_prev->key[n].len_used = len;
      }
   }

   cx->context = 1;
   cx->counter = 0;
   cx->getdata = 0;
   cx->multilevel = 0;
   cx->format = 0;

   if (pcon->argc > 1) {
      obj = DBX_TO_OBJECT(args[1]);
      key = c->dbx_new_string8(isolate, (char *) "getdata", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->getdata = 1;
         }
      }
      key = c->dbx_new_string8(isolate, (char *) "multilevel", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->context = 2;
         }
      }
      key = c->dbx_new_string8(isolate, (char *) "globaldirectory", 1);
      if (DBX_GET(obj, key)->IsBoolean()) {
         if (DBX_TO_BOOLEAN(DBX_GET(obj, key))->IsTrue()) {
            cx->context = 9;
         }
      }
      key = c->dbx_new_string8(isolate, (char *) "format", 1);
      if (DBX_GET(obj, key)->IsString()) {
         char buffer[64];
         value = DBX_TO_STRING(DBX_GET(obj, key));
         c->dbx_write_char8(isolate, value, buffer, 1);
         dbx_lcase(buffer);
         if (!strcmp(buffer, "url")) {
            cx->format = 1;
         }
      }
   }

   return;
}


void mcursor::Close(const FunctionCallbackInfo<Value>& args)
{
   DBXCON *pcon;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->m_count ++;

   pcon = c->pcon;
   pcon->argc = args.Length();

   if (pcon->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
      return;
   }
   if (pcon->argc > 0) {
      isolate->ThrowException(Exception::Error(c->dbx_new_string8(isolate, (char *) "Closing a globalquery template does not take any arguments", 1)));
      return;
   }

   if (cx->pqr_next) {
      if (cx->pqr_next->data.svalue.buf_addr) {
         dbx_free((void *) cx->pqr_next->data.svalue.buf_addr, 0);
         cx->pqr_next->data.svalue.buf_addr = NULL;
         cx->pqr_next->data.svalue.len_alloc = 0;
         cx->pqr_next->data.svalue.len_used = 0;
      }
      dbx_free((void *) cx->pqr_next, 0);
      cx->pqr_next = NULL;
   }

   if (cx->pqr_prev) {
      if (cx->pqr_prev->data.svalue.buf_addr) {
         dbx_free((void *) cx->pqr_prev->data.svalue.buf_addr, 0);
         cx->pqr_prev->data.svalue.buf_addr = NULL;
         cx->pqr_prev->data.svalue.len_alloc = 0;
         cx->pqr_prev->data.svalue.len_used = 0;
      }
      dbx_free((void *) cx->pqr_prev, 0);
      cx->pqr_prev = NULL;
   }

/*
   cx->delete_mcursor_template(cx);
*/

   return;
}



