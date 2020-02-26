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

#ifndef MG_CLASS_H
#define MG_CLASS_H

class mclass : public node::ObjectWrap
{
   public:

   short          open;
   short          context;
   int            oref;
   char           class_name[256];
   DBXCON         *pcon;
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

   static mclass * NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

   static int delete_mclass_template(mclass *cx);

   static void ClassMethod(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Method(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void SetProperty(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void GetProperty(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Reset(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

   inline double value() const { return value_; }

   explicit mclass(double value = 0);
   ~mclass();

   private:

   static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
   static v8::Persistent<v8::Function> constructor;

   double value_;
};

#endif

