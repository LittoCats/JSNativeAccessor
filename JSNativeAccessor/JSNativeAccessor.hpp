/*
 *  JSNativeAccessor.hpp
 *  JSNativeAccessor
 *
 *  Created by 程巍巍 on 5/25/16.
 *  Copyright © 2016 程巍巍. All rights reserved.
 *
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
    bool isSlice = false;
    
public:
    Buffer(size_t len, unsigned int ec = BufferEncodingUTF8): length(len), bytes(malloc(len)), encoding(ec) {}
    Buffer(void* bs, size_t len, unsigned int ec = BufferEncodingUTF8): length(len), bytes(bs), encoding(ec) {isSlice = true;}
    ~Buffer(){if (bytes != NULL && !isSlice) free((void*)bytes);}
    
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
    
private:
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

class Signature {
private:
    ffi_cif* cif;
    Signature(ffi_abi abi, ffi_type* rtype, std::vector<ffi_type*> atypes);
    ~Signature() {if (cif != NULL && cif != nullptr) free(cif);}
    
public:
    static CallAsConstructor(_);
    static CallAsFunction(Execute);
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
