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

#ifndef MG_CLASS_H
#define MG_CLASS_H

#define MG_CLASS_CHECK_CLASS(a) \
   if (a->c == NULL) { \
      v8::Isolate* isolatex = args.GetIsolate(); \
      isolatex->ThrowException(v8::Exception::Error(dbx_new_string8(isolatex, (char *) "Error in the instantiation of the mclass class", 1))); \
      return; \
   } \

class mclass : public node::ObjectWrap
{
   public:


   int            dbx_count;
   int            oref;
   char           class_name[256];
   int            class_name_len;
   unsigned short class_name16[256];
   int            class_name16_len;
   DBX_DBNAME     *c;


#if DBX_NODE_VERSION >= 100000
   static void       Init                    (v8::Local<v8::Object> exports);
#else
   static void       Init                    (v8::Handle<v8::Object> exports);
#endif
   explicit          mclass                  (int value = 0);
                     ~mclass                 ();

   static mclass *   NewInstance             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static int        async_callback          (mclass *clx);
   static int        delete_mclass_template  (mclass *clx);

   static void       ClassMethod             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       ClassMethod_bx          (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       ClassMethodEx           (const v8::FunctionCallbackInfo<v8::Value>& args, int binary);
   static void       Method                  (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Method_bx               (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       MethodEx                (const v8::FunctionCallbackInfo<v8::Value>& args, int binary);
   static void       SetProperty             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       GetProperty             (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       GetProperty_bx          (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       GetPropertyEx           (const v8::FunctionCallbackInfo<v8::Value>& args, int binary);
   static void       Reset                   (const v8::FunctionCallbackInfo<v8::Value>& args);
   static void       Close                   (const v8::FunctionCallbackInfo<v8::Value>& args);

private:

   static void       New                     (const v8::FunctionCallbackInfo<v8::Value>& args);
   static v8::Persistent<v8::Function>       constructor;
};

#endif

