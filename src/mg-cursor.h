/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2023 M/Gateway Developments Ltd,                      |
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

#ifndef MG_CURSOR_H
#define MG_CURSOR_H

#define MG_CURSOR_CHECK_CLASS(a) \
   if (a->c == NULL) { \
      v8::Isolate* isolatex = args.GetIsolate(); \
      isolatex->ThrowException(v8::Exception::Error(dbx_new_string8(isolatex, (char *) "Error in the instantiation of the mcursor class", 1))); \
      return; \
   } \

class mcursor : public node::ObjectWrap
{
public:

   int            dbx_count;
   short          context;
   short          getdata;
   short          multilevel;
   short          format;
   int            counter;
   char           global_name[256];
   int            global_name_len;
   unsigned short global_name16[256];
   int            global_name16_len;
   DBXQR          *pqr_prev;
   DBXQR          *pqr_next;
   DBXSTR         data;
   DBXSQL         *psql;
   DBX_DBNAME     *c;


#if DBX_NODE_VERSION >= 100000
   static void       Init                    (v8::Local<v8::Object> exports);
#else
   static void       Init                    (v8::Handle<v8::Object> exports);
#endif
   explicit          mcursor                 (int value = 0);
                     ~mcursor                ();

   static mcursor *  NewInstance             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static int        async_callback          (mcursor *cx);
   static int        delete_mcursor_template (mcursor *cx);

   static void       Execute                 (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Cleanup                 (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Next                    (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Previous                (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Reset                   (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Close                   (const v8::FunctionCallbackInfo<v8::Value>& args);

private:

   static void       New                     (const v8::FunctionCallbackInfo<v8::Value>& args);
   static v8::Persistent<v8::Function>       constructor;
};

int                  dbx_escape_output       (DBXSTR *pdata, char *item, int item_len, short context);
int                  dbx_escape_output16     (DBXSTR *pdata, unsigned short *item, int item_len, short context);

#endif

