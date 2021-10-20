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

#ifndef MG_GLOBAL_H
#define MG_GLOBAL_H

#define MG_GLOBAL_CHECK_CLASS(a) \
   if (a->c == NULL) { \
      v8::Isolate* isolatex = args.GetIsolate(); \
      isolatex->ThrowException(v8::Exception::Error(dbx_new_string8(isolatex, (char *) "Error in the instantiation of the mglobal class", 1))); \
      return; \
   } \

class mglobal : public node::ObjectWrap
{
public:

   int            dbx_count;
   char           global_name[256];
   int            global_name_len;
   unsigned short global_name16[256];
   int            global_name16_len;
   DBXVAL         *pkey;
   DBX_DBNAME     *c;

   static v8::Persistent<v8::Function>       constructor;

#if DBX_NODE_VERSION >= 100000
   static void       Init                    (v8::Local<v8::Object> exports);
#else
   static void       Init                    (v8::Handle<v8::Object> exports);
#endif
   explicit          mglobal                 (int value = 0);
                     ~mglobal                ();

   static mglobal *  NewInstance             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static int        async_callback          (mglobal *gx);
   static int        delete_mglobal_template (mglobal *gx);

   static void       Get         (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Get_bx      (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       GetEx       (const v8::FunctionCallbackInfo<v8::Value>& args, int binary);
   static void       Set         (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Defined     (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Delete      (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Next        (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Previous    (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Increment   (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Lock        (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Unlock      (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Merge       (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Reset       (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Close       (const v8::FunctionCallbackInfo<v8::Value>& args);

private:

   static void       New         (const v8::FunctionCallbackInfo<v8::Value>& args);
};


#endif

