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

#ifndef MG_CURSOR_H
#define MG_CURSOR_H

class mcursor : public node::ObjectWrap
{
   public:

   short          open;
   short          context;
   short          getdata;
   short          multilevel;
   short          format;
   char           global_name[256];
   DBXQR          *pqr_prev;
   DBXQR          *pqr_next;
   DBXCON         *pcon;
   DBXSTR         data;
   int            m_count;
   DBX_DBNAME     *c;


#if DBX_NODE_VERSION >= 100000
   static void                   Init(v8::Local<v8::Object> target);
#else
   static void                   Init(v8::Handle<v8::Object> target);
#endif

#if DBX_NODE_VERSION >= 100000
   static void dbx_set_prototype_method(v8::Local<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#else
   static void dbx_set_prototype_method(v8::Handle<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#endif

   static mcursor * NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

   static int delete_mcursor_template(mcursor *cx);

   static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Previous(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Reset(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

   inline double value() const { return value_; }

   explicit mcursor(double value = 0);
   ~mcursor();

   private:

   static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
   static v8::Persistent<v8::Function> constructor;


   double value_;
};

int dbx_escape_output(DBXSTR *pdata, char *item, int item_len, short context);

#endif

