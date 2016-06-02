/*
 *  JSNativeAccessor.hpp
 *  JSNativeAccessor
 *
 *  Created by 程巍巍 on 5/25/16.
 *  Copyright © 2016 程巍巍. All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef JSNativeAccessor_
#define JSNativeAccessor_

#include <iostream>
#include <algorithm>
#include <string>
#include <map>
#include <vector>


#include <JavaScriptCore/JavaScript.h>
#include "ffi.h"

#ifndef JSNativeAccessorVersion
#define JSNativeAccessorVersion 1
#endif

using namespace std;

#define GetProperty(name) JSValueRef Get ## name(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)

#define SetProperty(name) bool Set ## name(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)

#define CallAsFunction(name) JSValueRef Call ## name(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)

#define CallAsConstructor(name) JSObjectRef Construct ## name(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)

#define GetPropertyNames(Type) void GetPropertyNames ## Type(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames)

enum BufferEncoding {
    BufferEncodingHex      = 1,
    BufferEncodingBase64,
    BufferEncodingAscii,
    BufferEncodingUTF8
};

// MARK: Buffer
class Buffer {
    
public:
    const void* isa = (void*)&Definition;
    const unsigned int encoding;
    const size_t length;
    const void* bytes;
private:
    void (* const bfree)(void*);
private:
    bool isSlice = false;
    
public:
    Buffer(size_t len, unsigned int ec = BufferEncodingUTF8, void (*bfree)(void*) = free): length(len), bytes(malloc(len)), encoding(ec), bfree(bfree) {}
    Buffer(void* bs, size_t len, unsigned int ec = BufferEncodingUTF8): length(len), bytes(bs), encoding(ec), bfree(free) {isSlice = true;}
    ~Buffer(){if (bytes != NULL && !isSlice) bfree((void*)bytes);}
    
public:
    template<typename T>
    static bool isAsignFrom(T* data)
    {
        return data != NULL && data != nullptr && *(void**)data == &Definition;
    }
    
private:
    static void Finalize(JSObjectRef object);
    static CallAsFunction(Compare);
    static CallAsFunction(Copy);
    static CallAsFunction(Equals);
    static CallAsFunction(Fill);
    static CallAsFunction(IndexOf);
    static CallAsFunction(Write);
    static CallAsFunction(Slice);
    static CallAsFunction(ToString);
    
    template<typename NumType>
    static CallAsFunction(WriteNum);
    
    template<typename NumType>
    static CallAsFunction(ReadNum);
    
    static GetProperty(Length);
    
    static GetProperty(ByteAtIndex);
    static SetProperty(ByteAtIndex);
    
    static GetProperty(Pointer);
    
    static const JSStaticValue StaticValues[];
    static const JSStaticFunction StaticFunctions[];
    
public:
    static const JSClassDefinition Definition;
    
    static CallAsConstructor(_);
};

// MARK: libffi
template<typename T, ffi_type* FT>
class BuildIn {
public:
    ffi_type* ft = FT;
    
protected:
    static GetProperty(Size)        {auto buildIn = (BuildIn*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->size);}
    static GetProperty(Alignment)   {auto buildIn = (BuildIn*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->alignment);}
    static GetProperty(Type)        {auto buildIn = (BuildIn*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->type);}
    
    static void Finalize(JSObjectRef object) {auto buildIn = (BuildIn*)JSObjectGetPrivate(object); delete buildIn;}
    
    static CallAsFunction(Constructor) {return JSObjectCallAsConstructor(ctx, function, argumentCount, arguments, exception);}
    static CallAsConstructor(_);
    
    // instance method 会注入给新的对象
    static CallAsFunction(GetValue);
    static CallAsFunction(SetValue);
    
public:
    static JSObjectRef Load(JSContextRef ctx, const char* name);
};

class Struct {
public:
    ffi_type* ft = NULL;
    ~Struct() {if (ft != NULL) free(ft);}
    
private:
    static GetProperty(Size)        {auto buildIn = (Struct*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->size);}
    static GetProperty(Alignment)   {auto buildIn = (Struct*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->alignment);}
    static GetProperty(Type)        {auto buildIn = (Struct*)JSObjectGetPrivate(object); return JSValueMakeNumber(ctx, buildIn->ft->type);}
    
    static void Finalize(JSObjectRef object) {auto buildIn = (Struct*)JSObjectGetPrivate(object); delete buildIn;}
    
    static CallAsFunction(Constructor) {return JSObjectCallAsConstructor(ctx, function, argumentCount, arguments, exception);}
    static CallAsConstructor(_);
    
    static CallAsConstructor(Struct);
public:
    static JSClassRef Definition;
    static JSObjectRef Load(JSContextRef ctx);
};

class FunctionPointer: public BuildIn<void*, &ffi_type_pointer> {
public:
    
    
public:
    
};

class Signature {
private:
    ffi_cif* cif = NULL;
    ~Signature() {if (cif != NULL && cif != nullptr) free(cif);}
    
private:
    static void Finalize(JSObjectRef object);
    static CallAsFunction(Func);
    
    static CallAsConstructor(FunctionPointer);
    
    // 调用 signature 时，参数会被检测并格式化后，转发到该方法
    // 该方法的 function 参数为 signature， this 为 Buffer (内容为 c 函数批针)
    static CallAsFunction(Execute);
    
public:
    static const JSClassRef Definition;
    static CallAsConstructor(_);
};

// MARK: NativeAccessor
class JSNativeAccessor {
    
public:
    static void Initialize(JSContextRef ctx, JSObjectRef object);
    static void Finalize(JSObjectRef object);
    
    static const JSStaticValue StaticValues[];
    static const JSStaticFunction StaticFunctions[];
    
public:
    static const JSClassDefinition Definition;
};

#undef GetProperty
#undef SetProperty
#undef CallAsFunction
#undef CallAsConstructor
#undef GetPropertyNames

#endif
