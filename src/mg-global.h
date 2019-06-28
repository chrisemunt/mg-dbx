/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                            |
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

#ifndef MG_GLOBAL_H
#define MG_GLOBAL_H

class mglobal : public node::ObjectWrap
{
   public:

   struct dbx_baton_t {
      mglobal *                 c;
      unsigned char                 result_iserror;
      unsigned char                 result_isarray;
      int                           increment_by;
      int                           sleep_for;
      v8::Local<v8::Object>         json_result;
      v8::Local<v8::Array>          array_result;
      v8::Persistent<v8::Function>  cb;
      DBXCON * pcon;
   };

   short          open;
   char           global_name[256];
   DBXCON         *pcon;
   int m_count;
   DBX_DBNAME *c;

   static void Init();

#if DBX_NODE_VERSION >= 100000
   static void dbx_set_prototype_method(v8::Local<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#else
   static void dbx_set_prototype_method(v8::Handle<v8::FunctionTemplate> t, v8::FunctionCallback callback, const char* name, const char* data);
#endif

   static mglobal * NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args, char *cls_str, char *cls_info);

   static int delete_class_template(mglobal *cx);

   static void Get(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Set(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Defined(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Delete(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
   static void Previous(const v8::FunctionCallbackInfo<v8::Value>& args);

   static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

   inline double value() const { return value_; }

   explicit mglobal(double value = 0);
   ~mglobal();

   private:

   static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
   static v8::Persistent<v8::Function> constructor;


   double value_;
};


#endif

