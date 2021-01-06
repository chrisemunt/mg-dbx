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
#include "mg-cursor.h"
 
using namespace v8;
using namespace node;

Persistent<Function> mcursor::constructor;

mcursor::mcursor(int value) : dbx_count(value)
{
}


mcursor::~mcursor()
{
   delete_mcursor_template(this);
}


#if DBX_NODE_VERSION >= 100000
void mcursor::Init(Local<Object> exports)
#else
void mcursor::Init(Handle<Object> exports)
#endif
{
#if DBX_NODE_VERSION >= 120000
   Isolate* isolate = exports->GetIsolate();
   Local<Context> icontext = isolate->GetCurrentContext();

   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor", NewStringType::kNormal).ToLocalChecked());
   tpl->InstanceTemplate()->SetInternalFieldCount(3); /* 2.0.14 */
#else
   Isolate* isolate = Isolate::GetCurrent();

   /* Prepare constructor template */
   Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
   tpl->SetClassName(String::NewFromUtf8(isolate, "mcursor"));
   tpl->InstanceTemplate()->SetInternalFieldCount(1);
#endif

   /* Prototypes */

   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "execute", Execute);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "cleanup", Cleanup);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "next", Next);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "previous", Previous);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "reset", Reset);
   DBX_NODE_SET_PROTOTYPE_METHOD(tpl, "_close", Close);

#if DBX_NODE_VERSION >= 120000
   constructor.Reset(isolate, tpl->GetFunction(icontext).ToLocalChecked());
   exports->Set(icontext, String::NewFromUtf8(isolate, "mcursor", NewStringType::kNormal).ToLocalChecked(), tpl->GetFunction(icontext).ToLocalChecked()).FromJust();
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
      /* Invoked as constructor: `new mcursor(...)` */
      int value = args[0]->IsUndefined() ? 0 : DBX_INT32_VALUE(args[0]);

      mcursor * obj = new mcursor(value);
      obj->c = NULL;

      if (c) { /* 1.4.10 */
         if (c->pcon == NULL) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "No Connection to the database", 1)));
            return;
         }
         pcon = c->pcon;
         pmeth = dbx_request_memory(pcon, 1);
         pmeth->argc = argc;
         dbx_cursor_init((void *) obj);
         obj->c = c;

         /* 1.4.10 */
         rc = dbx_cursor_reset(args, isolate, pcon, pmeth, (void *) obj, 1, 1);
         if (rc < 0) {
            isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mcursor::New() method takes at least one argument (the query object)", 1)));
            dbx_request_memory_free(pcon, pmeth, 0);
            return;
         }
         dbx_request_memory_free(pcon, pmeth, 0);
      }
      obj->Wrap(args.This());
      args.This()->SetInternalField(2, DBX_INTEGER_NEW(DBX_MAGIC_NUMBER_MCURSOR)); /* 2.0.14 */
      args.GetReturnValue().Set(args.This());
   }
   else {
      /* Invoked as plain function `mcursor(...)`, turn into construct call. */
      const int argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(isolate->GetCurrentContext(), argc, argv).ToLocalChecked());
   }
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
   Local<Object> instance = cons->NewInstance(icontext, argc, argv).ToLocalChecked();
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(instance);

   args.GetReturnValue().Set(instance);

   return cx;
}


int mcursor::async_callback(mcursor *cx)
{
   cx->Unref();
   return 0;
}


int mcursor::delete_mcursor_template(mcursor *cx)
{
   return 0;
}


void mcursor::Execute(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int cn;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<Object> obj, obj1;
   Local<String> key;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::execute");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->psql = cx->psql;

   DBX_CALLBACK_FUN(pmeth->argc, cb, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Execute", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->cx = (void *) cx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_sql_execute;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

      if (c->dbx_queue_task((void *) c->dbx_process_task, (void *) c->dbx_invoke_callback_sql_execute, baton, 0)) {
         char error[DBX_ERROR_SIZE];

         T_STRCPY(error, _dbxso(error), pcon->error);
         c->dbx_destroy_baton(baton, pmeth);
         isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, error, 1)));
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }
      return;
   }

   dbx_sql_execute(pmeth);

   DBX_DBFUN_END(c);

   obj = DBX_OBJECT_NEW();

   key = dbx_new_string8(isolate, (char *) "sqlcode", 0);
   DBX_SET(obj, key, DBX_INTEGER_NEW(pmeth->psql->sqlcode));
   key = dbx_new_string8(isolate, (char *) "sqlstate", 0);
   DBX_SET(obj, key, dbx_new_string8(isolate, pmeth->psql->sqlstate, 0));
   if (pcon->error[0]) {
      key = dbx_new_string8(isolate, (char *) "error", 0);
      DBX_SET(obj, key, dbx_new_string8(isolate, pcon->error, 0));
   }
   else if (pmeth->psql->no_cols > 0) { /* v2.1.18 */

      Local<Array> a = DBX_ARRAY_NEW(pmeth->psql->no_cols);
      key = dbx_new_string8(isolate, (char *) "columns", 0);
      DBX_SET(obj, key, a);

      for (cn = 0; cn < pmeth->psql->no_cols; cn ++) {
         obj1 = DBX_OBJECT_NEW();
         DBX_SET(a, cn, obj1);
         key = dbx_new_string8(isolate, (char *) "name", 0);
         DBX_SET(obj1, key, dbx_new_string8(isolate, pmeth->psql->cols[cn]->name.buf_addr, 0));
         if (pmeth->psql->cols[cn]->stype) {
            key = dbx_new_string8(isolate, (char *) "type", 0);
            DBX_SET(obj1, key, dbx_new_string8(isolate, pmeth->psql->cols[cn]->stype, 0));
         }
      }
   }

   args.GetReturnValue().Set(obj);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mcursor::Cleanup(const FunctionCallbackInfo<Value>& args)
{
   short async;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<String> result;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::cleanup");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->psql = cx->psql;

   DBX_CALLBACK_FUN(pmeth->argc, cb, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Cleanup", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   
   DBX_DBFUN_START(c, pcon, pmeth);

   if (async) {
      DBX_DBNAME::dbx_baton_t *baton = c->dbx_make_baton(c, pmeth);
      baton->cx = (void *) cx;
      baton->isolate = isolate;
      baton->pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_sql_cleanup;
      Local<Function> cb = Local<Function>::Cast(args[pmeth->argc]);

      baton->cb.Reset(isolate, cb);

      cx->Ref();

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

   dbx_sql_cleanup(pmeth);

   DBX_DBFUN_END(c);

   result = dbx_new_string8n(isolate, pmeth->output_val.svalue.buf_addr, pmeth->output_val.svalue.len_used, pcon->utf8);
   args.GetReturnValue().Set(result);
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mcursor::Next(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::next");
   }
   pmeth = dbx_request_memory(pcon, 0);

   DBX_CALLBACK_FUN(pmeth->argc, cb, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Next", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }

      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_order(pmeth, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

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
         key = dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, pcon->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&(cx->data), (char *) "key=", 4, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&(cx->data), (char *) "&data=", 6, 0);
            dbx_escape_output(&(cx->data), cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, pcon->utf8));
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_query(pmeth, cx->pqr_next, cx->pqr_prev, 1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

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

         key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();
         key = dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);

         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
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
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   else if (cx->context == 9) {

      /* v2.1.18 */
      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_directory(pmeth, cx->pqr_prev, 1, &(cx->counter));

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, pcon->utf8);
         args.GetReturnValue().Set(key);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   else if (cx->context == 11) {

      pmeth->psql = cx->psql;

      if (!pmeth->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }

      eod = dbx_sql_row(pmeth, pmeth->psql->row_no, 1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pmeth->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size((unsigned char *) pmeth->output_val.svalue.buf_addr, pmeth->output_val.offs, &dsort, &dtype);
            pmeth->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pmeth->output_val.offs, dsort, dtype, pmeth->output_val.svalue.buf_addr + pmeth->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = dbx_new_string8n(isolate, (char *) pmeth->psql->cols[n]->name.buf_addr, pmeth->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate,  pmeth->output_val.svalue.buf_addr + pmeth->output_val.offs, len, 0));
            pmeth->output_val.offs += len;
         }

         args.GetReturnValue().Set(obj);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mcursor::Previous(const FunctionCallbackInfo<Value>& args)
{
   short async;
   int n, eod;
   DBXCON *pcon;
   DBXMETH *pmeth;
   Local<Object> obj;
   Local<String> key;
   DBXQR *pqr;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ICONTEXT;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::previous");
   }
   pmeth = dbx_request_memory(pcon, 0);

   DBX_CALLBACK_FUN(pmeth->argc, cb, async);

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments on Previous", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   if (async) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Cursor based operations cannot be invoked asynchronously", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   if (cx->context == 1) {
   
      if (cx->pqr_prev->keyn < 1) {
         args.GetReturnValue().Set(DBX_NULL());
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }

      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_order(pmeth, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

      if (cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used == 0) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else if (cx->getdata == 0) {
         key = dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, pcon->utf8);
         args.GetReturnValue().Set(key);
      }
      else {
         if (cx->format == 1) {
            cx->data.len_used = 0;
            dbx_escape_output(&cx->data, (char *) "key=", 4, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, 1);
            dbx_escape_output(&cx->data, (char *) "&data=", 6, 0);
            dbx_escape_output(&cx->data, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 1);
            key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
            args.GetReturnValue().Set(key);
         }
         else {
            obj = DBX_OBJECT_NEW();

            key = dbx_new_string8(isolate, (char *) "key", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].buf_addr, cx->pqr_prev->key[cx->pqr_prev->keyn - 1].len_used, pcon->utf8));
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_prev->data.svalue.buf_addr, cx->pqr_prev->data.svalue.len_used, 0));
            args.GetReturnValue().Set(obj);
         }
      }
   }
   else if (cx->context == 2) {
   
      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_query(pmeth, cx->pqr_next, cx->pqr_prev, -1, cx->getdata);

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

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

         key = dbx_new_string8n(isolate, (char *) cx->data.buf_addr, cx->data.len_used, 0);
         args.GetReturnValue().Set(key);
      }
      else {
         obj = DBX_OBJECT_NEW();

         key = dbx_new_string8(isolate, (char *) "global", 0);
         DBX_SET(obj, key, dbx_new_string8(isolate, cx->pqr_next->global_name.buf_addr, 0));
         key = dbx_new_string8(isolate, (char *) "key", 0);
         Local<Array> a = DBX_ARRAY_NEW(cx->pqr_next->keyn);
         DBX_SET(obj, key, a);
         for (n = 0; n < cx->pqr_next->keyn; n ++) {
            DBX_SET(a, n, dbx_new_string8n(isolate, cx->pqr_next->key[n].buf_addr, cx->pqr_next->key[n].len_used, 0));
         }
         if (cx->getdata) {
            key = dbx_new_string8(isolate, (char *) "data", 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate, cx->pqr_next->data.svalue.buf_addr, cx->pqr_next->data.svalue.len_used, 0));
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
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   else if (cx->context == 9) {
   
      /* v2.1.18 */
      DBX_DBFUN_START(c, pcon, pmeth);
      DBX_DB_LOCK(0);

      eod = dbx_global_directory(pmeth, cx->pqr_prev, -1, &(cx->counter));

      DBX_DBFUN_END(c);
      DBX_DB_UNLOCK();

      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         key = dbx_new_string8n(isolate, cx->pqr_prev->global_name.buf_addr, cx->pqr_prev->global_name.len_used, pcon->utf8);
         args.GetReturnValue().Set(key);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   else if (cx->context == 11) {

      pmeth->psql = cx->psql;

      if (!pmeth->psql) {
         args.GetReturnValue().Set(DBX_NULL());
         dbx_request_memory_free(pcon, pmeth, 0);
         return;
      }

      eod = dbx_sql_row(pmeth, pmeth->psql->row_no, -1);
      if (eod) {
         args.GetReturnValue().Set(DBX_NULL());
      }
      else {
         int len, dsort, dtype;

         obj = DBX_OBJECT_NEW();

         for (n = 0; n < pmeth->psql->no_cols; n ++) {
            len = (int) dbx_get_block_size((unsigned char *) pmeth->output_val.svalue.buf_addr, pmeth->output_val.offs, &dsort, &dtype);
            pmeth->output_val.offs += 5;

            /* printf("\r\n ROW DATA: n=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", n, len, pmeth->output_val.offs, dsort, dtype, pmeth->output_val.svalue.buf_addr + pmeth->output_val.offs); */

            if (dsort == DBX_DSORT_EOD || dsort == DBX_DSORT_ERROR) {
               break;
            }

            key = dbx_new_string8n(isolate, (char *) pmeth->psql->cols[n]->name.buf_addr, pmeth->psql->cols[n]->name.len_used, 0);
            DBX_SET(obj, key, dbx_new_string8n(isolate,  pmeth->output_val.svalue.buf_addr + pmeth->output_val.offs, len, 0));
            pmeth->output_val.offs += len;
         }
         args.GetReturnValue().Set(obj);
      }
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mcursor::Reset(const FunctionCallbackInfo<Value>& args)
{
   int rc;
   DBXCON *pcon;
   DBXMETH *pmeth;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::reset");
   }
   pmeth = dbx_request_memory(pcon, 1);

   pmeth->argc = args.Length();

   if (pmeth->argc < 1) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global reference to start with)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   /* 1.4.10 */
   rc = dbx_cursor_reset(args, isolate, pcon, pmeth, (void *) cx, 0, 0);
   if (rc < 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "The mglobalquery.reset() method takes at least one argument (the global reference to start with)", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }

   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}


void mcursor::Close(const FunctionCallbackInfo<Value>& args)
{
   int cn;
   DBXCON *pcon;
   DBXMETH *pmeth;
   mcursor *cx = ObjectWrap::Unwrap<mcursor>(args.This());
   MG_CURSOR_CHECK_CLASS(cx);
   DBX_DBNAME *c = cx->c;
   DBX_GET_ISOLATE;
   cx->dbx_count ++;

   pcon = c->pcon;
   if (pcon->log_functions) {
      c->LogFunction(c, args, (void *) cx, (char *) "mcursor::close");
   }
   pmeth = dbx_request_memory(pcon, 0);

   pmeth->argc = args.Length();

   if (pmeth->argc >= DBX_MAXARGS) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Too many arguments", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
      return;
   }
   if (pmeth->argc > 0) {
      isolate->ThrowException(Exception::Error(dbx_new_string8(isolate, (char *) "Closing a cursor template does not take any arguments", 1)));
      dbx_request_memory_free(pcon, pmeth, 0);
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

   if (cx->psql) {
      for (cn = 0; cn < cx->psql->no_cols; cn ++) {
         if (cx->psql->cols[cn]) {
            dbx_free((void *) cx->psql->cols[cn], 0);
            cx->psql->cols[cn] = NULL;
         }
      }
      dbx_free((void *) cx->psql, 0);
      cx->psql = NULL;
   }

/*
   cx->delete_mcursor_template(cx);
*/
   dbx_request_memory_free(pcon, pmeth, 0);
   return;
}

