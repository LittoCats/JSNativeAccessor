/*
 *  JSNativeAccessor.cpp
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

#include "JSNativeAccessor.hpp"

// MARK: Macro

#define GetProperty(name) Get ## name(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)

#define SetProperty(name) Set ## name(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)

#define CallAsFunction(name) Call ## name(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)

#define CallAsConstructor(name) Construct ## name(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)

#define GetPropertyNames(Type) GetPropertyNames ## Type(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames)

typedef void(*ScheduleOnJSCThread)(JSGlobalContextRef ctx, void(*)(void*), void*);
static ScheduleOnJSCThread scheduleOnJSThread = NULL;

static std::string base64_encode(string const& data);
static std::string base64_decode(std::string const& encoded_string);

static map<string, BufferEncoding> EncodingMap = {
    {"hex", BufferEncodingHex},
    {"base16", BufferEncodingHex},
    {"base64", BufferEncodingBase64},
    {"ascii", BufferEncodingAscii},
    {"utf8", BufferEncodingUTF8},
    {"utf-8", BufferEncodingUTF8}
};

static map<BufferEncoding, const char*> EncodingNameMap = {
    {BufferEncodingHex,     "hex"},
    {BufferEncodingBase64,  "base64"},
    {BufferEncodingAscii,   "ascii"},
    {BufferEncodingUTF8,    "utf-8"}
};

static JSClassRef NBuffer = JSClassCreate(&Buffer::Definition);


// MARK: Buffer
const JSClassDefinition Buffer::Definition = {
    .version                = JSNativeAccessorVersion,
    .className              = "Buffer",
    .finalize               = Finalize,
    
    .getProperty            = GetByteAtIndex,
    .setProperty            = SetByteAtIndex,
    
    .staticValues           = StaticValues,
    .staticFunctions        = StaticFunctions
};

const JSStaticValue Buffer::StaticValues[] =
{
    {"length", GetLength, NULL, kJSPropertyAttributeReadOnly},
    {"__pointer__", GetPointer, NULL, kJSPropertyAttributeReadOnly},
    {NULL, NULL, NULL, 0}
};

const JSStaticFunction Buffer::StaticFunctions[] =
{
    {"compare",     CallCompare,    kJSPropertyAttributeReadOnly},
    {"fill",        CallFill,       kJSPropertyAttributeReadOnly},
    {"indexOf",     CallIndexOf,    kJSPropertyAttributeReadOnly},
    {"slice",       CallSlice,      kJSPropertyAttributeReadOnly},
    {"toString",    CallToString,   kJSPropertyAttributeReadOnly},
    {"write",       CallWrite,      kJSPropertyAttributeReadOnly},
    
#define INA(name, T)    \
    {"write" # name,       CallWriteNum<T>,      kJSPropertyAttributeReadOnly},   \
    {"read" # name,       CallReadNum<T>,      kJSPropertyAttributeReadOnly},
    
    INA(UInt8,  uint8_t)
    INA(Int8,   int8_t)
    INA(UInt16, uint16_t)
    INA(Int16,  int16_t)
    INA(UInt32, uint32_t)
    INA(Int32,  int32_t)
    INA(UInt64, uint64_t)
    INA(Int64,  int64_t)
    
    INA(WChar, wchar_t)
    INA(UWChar, unsigned wchar_t)
    
    INA(Float, float)
    INA(Double, double)
    INA(LongDouble, long double)
    
    INA(UChar, unsigned char)
    INA(Char, char)
    
    INA(UShort, unsigned short)
    INA(Short, short)
    
    INA(Long, long)
    INA(LongLong, long long)
    
    INA(ULong, unsigned long)
    INA(ULongLong, unsigned long long)
    
    INA(UInt, unsigned int)
    INA(Int, int)
    INA(Unsigned, unsigned)
    INA(Signed, signed)
    
    INA(USize, size_t)
    INA(SSize, ssize_t)
    
    INA(Ptrdiff, ptrdiff_t)
    
    INA(Bool, bool)
    
#undef INA
    
    {NULL, NULL, 0}
};

void Buffer::Finalize(JSObjectRef object)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(object);
    delete buffer;
}

template <typename NumType>
JSValueRef Buffer::CallAsFunction(WriteNum) {
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return JSValueMakeUndefined(ctx);
    
    NumType number = 0;
    ssize_t offset = 0;
    bool noAssert = false;
    
    if (argumentCount > 0) {
        number = JSValueToNumber(ctx, arguments[0], exception);
        if (*exception) return NULL;
    }
    
    if (argumentCount > 1) {
        offset = JSValueToNumber(ctx, arguments[1], exception);
        if (*exception) return NULL;
        
        if (offset < 0) offset += buffer->length;
    }
    
    if (argumentCount > 2) {
        noAssert = JSValueToBoolean(ctx, arguments[2]);
    }
    
    if (!noAssert && (offset < 0 || offset + sizeof(NumType) > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    uint8_t* bytes = (uint8_t*)buffer->bytes;
    bytes += offset;
    NumType* in = (NumType*)bytes;
    *in = number;
    
    // WriteNum 函数返回了写入数据的长度，可用于测试 NumType 的长度
    return JSValueMakeNumber(ctx, sizeof(NumType));
}

template <typename NumType>
JSValueRef Buffer::CallAsFunction(ReadNum) {
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return JSValueMakeUndefined(ctx);
    
    ssize_t offset = 0;
    bool noAssert = false;
    
    if (argumentCount > 0) {
        offset = JSValueToNumber(ctx, arguments[0], exception);
        if (*exception) return NULL;
        
        if (offset < 0) offset += buffer->length;
    }
    
    if (argumentCount > 1) {
        noAssert = JSValueToBoolean(ctx, arguments[1]);
    }
    
    if (!noAssert && (offset < 0 || offset + sizeof(NumType) > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    uint8_t* bytes = (uint8_t*)buffer->bytes;
    bytes += offset;
    NumType* number = (NumType*)bytes;
    
    return JSValueMakeNumber(ctx, *number);
}

JSValueRef Buffer::CallAsFunction(Compare)
{
    if (argumentCount == 0 || !JSValueIsObject(ctx, arguments[0])) return JSValueMakeUndefined(ctx);
    
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return JSValueMakeUndefined(ctx);
    
    auto other = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
    if (!Buffer::isAsignFrom(other)) return JSValueMakeUndefined(ctx);
    
    return JSValueMakeNumber(ctx, memcmp(buffer->bytes, other->bytes, min(buffer->length, other->length)));
}

/**
 #### buf.fill(value[, offset][, end])
 
 * value
 * offset Number, Optional
 * end Number, Optional
 
 Fills the buffer with the specified value.
 
 If the offset (defaults to 0) and end (defaults to buffer.length) are not given it will fill the entire buffer.
 */
JSValueRef Buffer::CallAsFunction(Fill)
{
    if (argumentCount == 0) return NULL;
    
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    ssize_t start = 0;
    ssize_t end = buffer->length;
    
    if (argumentCount > 1) {
        start = JSValueToNumber(ctx, arguments[1], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (argumentCount > 2) {
        end = JSValueToNumber(ctx, arguments[2], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (start < 0) start += buffer->length;
    if (end < 0) end += buffer->length;
    
    if ((start < 0 || start > buffer->length) || (end < 0 || end > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    if (start > end) {
        start = start ^ end;
        end = start ^ end;
        start = start ^ end;
    }
    
    uint8_t value = 0;
    
    if (JSValueIsString(ctx, arguments[0])) {
        JSStringRef numStr = JSValueToStringCopy(ctx, arguments[0], exception);
        if (*exception == NULL) {
            char buf[2] = {};
            JSStringGetUTF8CString(numStr, buf, 2);
            JSStringRelease(numStr);
            value = (uint8_t)buf[0];
        }
    }else if (JSValueIsNumber(ctx, arguments[0])){
        value = JSValueToNumber(ctx, arguments[0], exception);
    }
    
    uint8_t* bytes = (uint8_t*)buffer->bytes;
    memset(bytes+start, value, end - start);
    return thisObject;
}
JSValueRef Buffer::CallAsFunction(IndexOf)
{
    return NULL;
}

/**
 buf.write(string[, offset][, length][, encoding])#
 
 * string String - data to be written to buffer
 * offset Number, Optional, Default: 0
 * length Number, Optional, Default: buffer.length - offset
 * encoding String, Optional, Default: 'utf8'
 
 Writes string to the buffer at offset using the given encoding. 
 offset defaults to 0, encoding defaults to 'utf8'. length is the number of bytes to write. Returns number of octets written. 
 
 If buffer did not contain enough space to fit the entire string, it will write a partial amount of the string. 
 length defaults to buffer.length - offset. 
 The method will not write partial characters.
 */
JSValueRef Buffer::CallAsFunction(Write)
{
    if (argumentCount == 0) return NULL;
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    BufferEncoding en = BufferEncodingUTF8;
    ssize_t offset = 0;
    size_t length = 0;
    
    if (argumentCount > 1) {
        offset = JSValueToNumber(ctx, arguments[1], exception);
        if (*exception != NULL) return NULL;
    }
    if (offset < 0) offset += buffer->length;
    
    if (argumentCount > 2) {
        length = JSValueToNumber(ctx, arguments[2], exception);
        if (*exception != NULL) return NULL;
    }else{
        length = buffer->length - offset;
    }
    
    if (offset < 0 || (offset + length > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    if (argumentCount > 3) {
        JSStringRef enStr = JSValueToStringCopy(ctx, arguments[3], exception);
        if (*exception != NULL) return NULL;
        size_t len = JSStringGetMaximumUTF8CStringSize(enStr);
        char* buffer = new char[len];
        JSStringGetUTF8CString(enStr, buffer, len);
        JSStringRelease(enStr);
        
        string name(buffer);
        delete [] buffer;
        
        transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        auto iter = EncodingMap.find(name);
        if (iter != EncodingMap.end()) {
            en = iter->second;
        }
    }
    
    JSStringRef str = JSValueToStringCopy(ctx, arguments[0], exception);
    if (*exception != NULL) return NULL;
    size_t len = JSStringGetMaximumUTF8CStringSize(str);
    char* buf = new char[len];
    len = JSStringGetUTF8CString(str, buf, len);
    JSStringRelease(str);
    
    string content(buf);
    delete [] buf;
    
    if (en == BufferEncodingHex) {
        static char Table[] = {
            0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0xA,0xB,0xC,0xD,0xE,0xF,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0xa,0xb,0xc,0xd,0xe,0xf,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
            0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
        
        len = len / 2;
        for (auto index  = 0; index < len; index++) {
            uint8_t byte = (Table[content[index * 2]] << 4) | (Table[content[index * 2 + 1]]);
            content[index] = byte;
        }
        
        content[len] = 0x0;
    }else if (en == BufferEncodingBase64) {
        content = base64_decode(content);
        len = content.size();
    }else{
        len -= 1;
    }
    
    uint8_t* bytes = (uint8_t*)buffer->bytes;
    memcpy(bytes+offset, content.data(), min(length, len));
    
    return thisObject;
}

/**
 buf.slice([start[, end]])
 
 * start Number, Optional, Default: 0
 * end Number, Optional, Default: buffer.length
 
 Returns a new buffer which references the same memory as the old, but offset and cropped by the start (defaults to 0) and end (defaults to buffer.length) indexes. 
 
 Negative indexes start from the end of the buffer.
 
 Modifying the new buffer slice will modify memory in the original buffer!
 */
JSValueRef Buffer::CallAsFunction(Slice)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    ssize_t start = 0;
    ssize_t end = buffer->length;
    
    if (argumentCount > 0) {
        start = JSValueToNumber(ctx, arguments[0], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (argumentCount > 1) {
        end = JSValueToNumber(ctx, arguments[1], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (start < 0) start += buffer->length;
    if (end < 0) end += buffer->length;
    
    if ((start < 0 || start > buffer->length) || (end < 0 || end > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    if (start > end) {
        start = start ^ end;
        end = start ^ end;
        start = start ^ end;
    }
    
    uint8_t* bytes = (uint8_t*)buffer->bytes;
    
    auto newBuffer = new Buffer(bytes+start, end-start, buffer->encoding);
    
    auto slice = JSObjectMake(ctx, NBuffer, newBuffer);
    
    // slice 需要引用原始对象，以确保原始对象释放而造成内存指针无效
    static JSStringRef parentName = JSStringCreateWithUTF8CString("__parent__");
    JSObjectSetProperty(ctx, slice, parentName, thisObject, kJSPropertyAttributeReadOnly, exception);
    
    return slice;
}

/**
 #### buf.toString([encoding][, start][, end])#
 
 * encoding String, Optional, Default: 'utf8'
 
 * start Number, Optional, Default: 0
 
 * end Number, Optional, Default: buffer.length
 
 Decodes and returns a string from buffer data encoded using the specified character set encoding.
 If encoding is undefined or null, then encoding defaults to 'utf8'.
 The start and end parameters default to 0 and buffer.length when undefined.
 */
JSValueRef Buffer::CallAsFunction(ToString)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    BufferEncoding en = BufferEncodingUTF8;
    ssize_t start = 0;
    ssize_t end = buffer->length;
    
    if (argumentCount > 0) {
        JSStringRef enStr = JSValueToStringCopy(ctx, arguments[0], exception);
        if (*exception != NULL) return NULL;
        size_t len = JSStringGetMaximumUTF8CStringSize(enStr);
        char* buffer = new char[len];
        JSStringGetUTF8CString(enStr, buffer, len);
        JSStringRelease(enStr);
        
        string name(buffer);
        delete [] buffer;
        
        transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        auto iter = EncodingMap.find(name);
        if (iter != EncodingMap.end()) {
            en = iter->second;
        }
    }
    
    if (argumentCount > 1) {
        start = JSValueToNumber(ctx, arguments[1], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (argumentCount > 2) {
        end = JSValueToNumber(ctx, arguments[2], exception);
        if (*exception != NULL) return NULL;
    }
    
    if (start < 0) start += buffer->length;
    if (end < 0) end += buffer->length;
    
    if ((start < 0 || start > buffer->length) || (end < 0 || end > buffer->length)) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    if (start > end) {
        start = start ^ end;
        end = start ^ end;
        start = start ^ end;
    }
    
    const char* bytes = (char*)buffer->bytes;
    string smsg(bytes+start, end-start);
    
    if (en == BufferEncodingHex) {
        static char Table[] = "0123456789abcdef";
        smsg.resize(smsg.size()*2);
        
        //  倒序转换
        for (auto index = end - start; index-- > 0;) {
            char code = smsg[index];
            smsg[index*2+1] = Table[code & 0xF];
            smsg[index*2] = Table[(code >> 4) & 0xF];
        }
    }else if (en == BufferEncodingBase64){
        smsg = base64_encode(smsg);
    }
    
    JSStringRef jmsg = JSStringCreateWithUTF8CString(smsg.c_str());
    JSValueRef msg = JSValueMakeString(ctx, jmsg);
    JSStringRelease(jmsg);
    return msg;
}

JSValueRef Buffer::GetProperty(ByteAtIndex)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(object);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    ssize_t index = 0;
    {
        // 如果 propertyName 的字符不是 0~9 开头，则表示不是一个有效的 index，返回 NULL，引擎按正常的 Object 属性取值
        const JSChar* chars = JSStringGetCharactersPtr(propertyName);
        size_t length = JSStringGetLength(propertyName);
        char* buffer = new char[length];
        
        JSChar c;
        for (size_t index = 0; index < length; index++) {
            c = chars[index];
            if ((c > '9' || c < '0') && c != '-') {
                delete [] buffer;
                return NULL;
            }
            buffer[index] = (char)c;
        }
        index = strtol(buffer, NULL, 10);
        delete [] buffer;
    }
    
    if (index < 0) index += buffer->length; // 如果索引为 负，则表示从结尾查找
    
    if (index < 0 || index >= buffer->length) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    auto bytes = (uint8_t*)buffer->bytes;
    
    return JSValueMakeNumber(ctx, bytes[index]);
}

bool Buffer::SetProperty(ByteAtIndex)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(object);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    ssize_t index = 0;
    {
        // 如果 propertyName 的字符不是 0~9 开头，则表示不是一个有效的 index，返回 NULL，引擎按正常的 Object 属性取值
        const JSChar* chars = JSStringGetCharactersPtr(propertyName);
        size_t length = JSStringGetLength(propertyName);
        char* buffer = new char[length];
        
        JSChar c;
        for (size_t index = 0; index < length; index++) {
            c = chars[index];
            if ((c > '9' || c < '0') && c != '-') {
                delete [] buffer;
                return NULL;
            }
            buffer[index] = (char)c;
        }
        index = strtol(buffer, NULL, 10);
        delete [] buffer;
    }
    
    if (index < 0) index += buffer->length; // 如果索引为 负，则表示从结尾查找
    
    if (index < 0 || index >= buffer->length) {
        static JSStringRef errmsg = JSStringCreateWithUTF8CString("out of range");
        JSValueRef err = JSValueMakeString(ctx, errmsg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
        return NULL;
    }
    
    uint8_t byte = JSValueToNumber(ctx, value, exception);
    if (*exception != NULL) return NULL;
    
    auto bytes = (uint8_t*)buffer->bytes;
    bytes[index] = byte;
    
    return JSValueMakeBoolean(ctx, true);
}

JSValueRef Buffer::GetProperty(Length)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(object);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    return JSValueMakeNumber(ctx, buffer->length);
}

/**
 pointer 实际上也是一个 buffer , buffer 的内存中存了一个指针
 */
JSValueRef Buffer::GetProperty(Pointer)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(object);
    if (!Buffer::isAsignFrom(buffer)) return NULL;
    
    auto newBuffer = new Buffer(sizeof(void*));
    void** bytes = (void**)newBuffer->bytes;
    *bytes = (void*)buffer->bytes;
    
    // 设置 __buffer__ 属性，如果是 buffer 的指针，则应该可以返向找到 buffer
    JSObjectRef newObject = JSObjectMake(ctx, NBuffer, newBuffer);
    static JSStringRef bufferName = JSStringCreateWithUTF8CString("__buffer__");
    JSObjectSetProperty(ctx, newObject, bufferName, object, kJSPropertyAttributeReadOnly, exception);
    
    return newObject;
}

/**
 
 #### new Buffer(size)
 
 * size Number
 
 Allocates a new buffer of size bytes.  size must be less than 1,073,741,824 bytes (1 GB) on 32 bits architectures or 2,147,483,648 bytes (2 GB) on 64 bits architectures, otherwise a RangeError is thrown.
 
 Unlike ArrayBuffers, the underlying memory for buffers is not initialized. So the contents of a newly created Buffer are unknown and could contain sensitive data. Use buf.fill(0) to initialize a buffer to zeroes.
 
 #### new Buffer(array)
 
 * array Array
 
 Allocates a new buffer using an array of octets.
 
 #### new Buffer(buffer)
 
 * buffer Buffer
 
 Copies the passed buffer data onto a new Buffer instance.
 
 #### new Buffer(str[, encoding])#
 
 * str String - string to encode.
 
 encoding String - encoding to use, Optional.
 
 Allocates a new buffer containing the given str. encoding defaults to 'utf8'.
 */

JSObjectRef Buffer::CallAsConstructor(_)
{
    if (argumentCount == 0) return JSObjectMake(ctx, NBuffer, new Buffer(0));
    
    // new Buffer(size)
    if (JSValueIsNumber(ctx, arguments[0])) {
        size_t len = JSValueToNumber(ctx, arguments[0], exception);
        if (*exception != NULL) return NULL;
        return JSObjectMake(ctx, NBuffer, new Buffer(len));
    }
    
    // new Buffer(str[,encoding])
    if (JSValueIsString(ctx, arguments[0])) {
        JSStringRef str = JSValueToStringCopy(ctx, arguments[0], exception);
        if (*exception != NULL) return NULL;
        size_t len = JSStringGetMaximumUTF8CStringSize(str);
        
        char* buf = new char[len];
        len = JSStringGetUTF8CString(str, buf, len);
        JSStringRelease(str);
        
        string content(buf);
        delete [] buf;
        
        BufferEncoding en = BufferEncodingUTF8;
        if (argumentCount > 1) {
            JSStringRef enStr = JSValueToStringCopy(ctx, arguments[1], exception);
            if (*exception != NULL) return NULL;
            size_t len = JSStringGetMaximumUTF8CStringSize(enStr);
            char* buffer = new char[len];
            JSStringGetUTF8CString(enStr, buffer, len);
            JSStringRelease(enStr);
            
            string name(buffer);
            delete [] buffer;
            
            transform(name.begin(), name.end(), name.begin(), ::tolower);
            
            auto iter = EncodingMap.find(name);
            if (iter != EncodingMap.end()) {
                en = iter->second;
            }
        }
        
        if (en == BufferEncodingHex) {
            static char Table[] = {
                0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0xA,0xB,0xC,0xD,0xE,0xF,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0xa,0xb,0xc,0xd,0xe,0xf,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
                0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
            
            len = len / 2;
            for (auto index  = 0; index < len; index++) {
                uint8_t byte = (Table[content[index * 2]] << 4) | (Table[content[index * 2 + 1]]);
                content[index] = byte;
            }
            
            buf[len] = 0x0;
        }else if (en == BufferEncodingBase64) {
            content = base64_decode(content);
            len = content.size();
        }else if (en == BufferEncodingUTF8) {
            // 如果是基本的 UTF8 字符串，最后一个字符是占位标识 \0，需要去除
            len -= 1;
        }
        
        Buffer* buffer = new Buffer(len);
        
        memcpy((void *)buffer->bytes, content.data(), len);
        
        return JSObjectMake(ctx, NBuffer, buffer);
    }
    
    // new Buffer(buffer)
    if (JSValueIsObjectOfClass(ctx, arguments[0], NBuffer)) {
        auto buffer = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
        if (!Buffer::isAsignFrom(buffer)) return JSObjectMake(ctx, NBuffer, new Buffer(0));
        
        auto newBuffer = new Buffer(buffer->length);
        memcpy((void *)newBuffer->bytes, buffer->bytes, buffer->length);
        return JSObjectMake(ctx, NBuffer, newBuffer);
    }
    
    // new Buffer(array)
    if (JSValueIsObject(ctx, arguments[0])) {
        JSObjectRef object = (JSObjectRef)arguments[0];
        
        static JSStringRef lenName = JSStringCreateWithUTF8CString("length");
        
        JSValueRef lenVal = JSObjectGetProperty(ctx, object, lenName, exception);
        if (*exception != NULL) return JSObjectMake(ctx, NBuffer, new Buffer(0));
        
        size_t len = JSValueToNumber(ctx, lenVal, NULL);
        
        auto buffer = new Buffer(len);
        uint8_t* bytes = (uint8_t*)buffer->bytes;
        
        JSValueRef exception = NULL;
        for (auto index = 0; index < len; index++) {
            exception = NULL;
            JSValueRef numVal = JSObjectGetPropertyAtIndex(ctx, object, index, &exception);
            if (exception != NULL) continue;
            
            uint8_t num = JSValueToNumber(ctx, numVal, &exception);
            if (exception != NULL) continue;
            
            bytes[index] = num;
        }
        
        return JSObjectMake(ctx, NBuffer, buffer);
    }
    static JSStringRef errmsg = JSStringCreateWithUTF8CString("Invalid arguments: new Buffer(size) new Buffer(str[,encoding]) new Buffer(buffer) new Buffer(array)");
    JSValueRef msg = JSValueMakeString(ctx, errmsg);
    *exception = JSObjectMakeError(ctx, 1, &msg, exception);
    return NULL;
}

// MARK: STDC
#include "STDC"

// MARK: ffi
static JSClassRef BuildInClass = JSClassCreate(&kJSClassDefinitionEmpty);

// 特化 Void 对象
template<>
JSValueRef BuildIn<void, &ffi_type_void>::CallAsFunction(GetValue) {return JSValueMakeUndefined(ctx);}
template<>
JSValueRef BuildIn<void, &ffi_type_void>::CallAsFunction(SetValue) {return thisObject;}
template<>
JSObjectRef BuildIn<void, &ffi_type_void>::CallAsConstructor(_){return JSObjectMake(ctx, NBuffer, new Buffer(0));}

// 特化 Pointer(void*) 对象
template<>
JSValueRef BuildIn<void*, &ffi_type_pointer>::CallAsFunction(GetValue){
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (Buffer::isAsignFrom(buffer)) {
        char buf[32] = {};
        sprintf(buf, "%p", *(void**)buffer->bytes);
        JSStringRef jval = JSStringCreateWithUTF8CString(buf);
        JSValueRef value = JSValueMakeString(ctx, jval);
        JSStringRelease(jval);
        return value;
    }
    return JSValueMakeUndefined(ctx);
}
template<>
JSValueRef BuildIn<void*, &ffi_type_pointer>::CallAsFunction(SetValue){
    // 有三种情况：
    // Number 一个 void* 等效的 long long 值
    // String sprintf 获得的字符串
    // Buffer
    
    // // 第二个参数为 第一个参数（指针）的释放方法，必须为 Buffer, 如果不是，则忽略
    
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (Buffer::isAsignFrom(buffer)) {
        void* ptr = NULL;
        void* bfree = NULL;
        static JSStringRef bufferPName = JSStringCreateWithUTF8CString("__buffer__");
        if (argumentCount > 0) {
            JSType type = JSValueGetType(ctx, arguments[0]);
            JSObjectDeleteProperty(ctx, thisObject, bufferPName, NULL);
            if (type == kJSTypeNumber) {
                long long ptrVal = (long long)JSValueToNumber(ctx, arguments[0], NULL);
                ptr = (void*)ptrVal;
            }else
                // Pointer 构造函数的参数可以是一个 printf("%p", ptr) 打印的地址
                if (type == kJSTypeString) {
                    char ptrstr[32] = {};
                    JSStringRef jptrstr = JSValueToStringCopy(ctx, arguments[0], exception);
                    if (*exception != NULL) return NULL;
                    JSStringGetUTF8CString(jptrstr, ptrstr, 32);
                    JSStringRelease(jptrstr);
                    unsigned long long ptrVal = strtoull(((char*)ptrstr)+2, NULL, 16);
                    
                    ptr = (void*)ptrVal;
                }else
                    // Pointer 构造函数的参数是 Buffer，因为只有 Buffer 是 Ｃ 类型的数据，才能构造出指针
                    if (type == kJSTypeObject) {
                        auto buffer = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
                        if (Buffer::isAsignFrom(buffer)) {
                            ptr = (void*)buffer->bytes;
                            JSObjectSetProperty(ctx, thisObject, bufferPName, arguments[0], kJSPropertyAttributeDontEnum, NULL);
                        }
                    }
        }
        
        if (argumentCount > 1 && JSValueIsObject(ctx, arguments[1])) {
            // 第二个参数为 第一个参数（指针）的释放方法，必须为 Buffer, 如果不是，则忽略
            auto buffer = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
            if (Buffer::isAsignFrom(buffer)) {
                bfree = *(void**)buffer->bytes;
            }
        }
        
        ((void**)buffer->bytes)[0] = ptr;
        if (buffer->length >= sizeof(void*) * 2) {
            // 原始 Buffer 实例做为 Pointer 实例时，没法设置释放方法
            ((void**)buffer->bytes)[1] = bfree;
        }
    }
    return thisObject;
}


static void BuildIn_Pointer_Destory(void* bytes) {
    if (((void**)bytes)[1] != NULL && ((void**)bytes)[0] != NULL) {
        ((void(*)(void*))((void**)bytes)[1])(((void**)bytes)[0]);
    }
    free(bytes);
}
template<>
JSObjectRef BuildIn<void*, &ffi_type_pointer>::CallAsConstructor(_)
{
    void* ptr = NULL;
    void* bfree = NULL;
    auto buffer = new Buffer(sizeof(void*)*2, BufferEncodingUTF8, BuildIn_Pointer_Destory);
    auto obj = JSObjectMake(ctx, JSClass, buffer);
    
    if (argumentCount > 0) {
        JSType type = JSValueGetType(ctx, arguments[0]);
        if (type == kJSTypeNumber) {
            long long ptrVal = (long long)JSValueToNumber(ctx, arguments[0], NULL);
            ptr = (void*)ptrVal;
        }else
        // Pointer 构造函数的参数可以是一个 printf("%p", ptr) 打印的地址
        if (type == kJSTypeString) {
            char ptrstr[32] = {};
            JSStringRef jptrstr = JSValueToStringCopy(ctx, arguments[0], exception);
            if (*exception != NULL) return NULL;
            JSStringGetUTF8CString(jptrstr, ptrstr, 32);
            JSStringRelease(jptrstr);
            unsigned long long ptrVal = strtoull(((char*)ptrstr)+2, NULL, 16);
            
            ptr = (void*)ptrVal;
        }else
        // Pointer 构造函数的参数是 Buffer，因为只有 Buffer 是 Ｃ 类型的数据，才能构造出指针
        if (type == kJSTypeObject) {
            auto buffer = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
            if (Buffer::isAsignFrom(buffer)) {
                ptr = (void*)buffer->bytes;
                static JSStringRef name = JSStringCreateWithUTF8CString("__buffer__");
                JSObjectSetProperty(ctx, obj, name, arguments[0], kJSPropertyAttributeDontEnum, NULL);
            }
        }
    }
    
    if (argumentCount > 1 && JSValueIsObject(ctx, arguments[1])) {
        // 第二个参数为 第一个参数（指针）的释放方法，必须为 Buffer, 如果不是，则忽略
        auto buffer = (Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]);
        if (Buffer::isAsignFrom(buffer)) {
            bfree = *(void**)buffer->bytes;
        }
    }
    
    ((void**)buffer->bytes)[0] = ptr;
    ((void**)buffer->bytes)[1] = bfree;
    
    return obj;
}

// 泛化基本数值类型

// 继承 Buffer 的 JSClass(NBuffer)，这里需要特别注意的是，必需设制 Finalize 方法，因为 finalize 方法无法从 NBuffer 类继承过来
template<typename T, ffi_type* FT>
JSClassRef BuildIn<T, FT>::JSClass = []()->JSClassRef{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "BuildIn";
    definition.parentClass = NBuffer;
    definition.finalize = Buffer::Finalize;
    JSStaticFunction staticFunctions[] = {
        {"setValue", CallSetValue, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"getValue", CallGetValue, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {NULL, NULL, 0}
    };
    definition.staticFunctions = staticFunctions;
    
    return JSClassCreate(&definition);
}();

template<typename T, ffi_type* FT>
JSValueRef BuildIn<T, FT>::CallAsFunction(GetValue)
{
    auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
    if (Buffer::isAsignFrom(buffer) && buffer->length >= sizeof(T)) {
        T number = *(T*)buffer->bytes;
        return JSValueMakeNumber(ctx, number);
    }
    return JSValueMakeNumber(ctx, NAN);
}

template<typename T, ffi_type* FT>
JSValueRef BuildIn<T, FT>::CallAsFunction(SetValue)
{
    if (argumentCount > 0) {
        auto buffer = (Buffer*)JSObjectGetPrivate(thisObject);
        if (Buffer::isAsignFrom(buffer) && buffer->length >= sizeof(T)) {
            T* number = (T*)buffer->bytes;
            *number = JSValueToNumber(ctx, arguments[0], exception);
        }
    }
    
    return thisObject;
}

template<typename T, ffi_type* FT>
JSObjectRef BuildIn<T, FT>::CallAsConstructor(_)
{
    T number = 0;
    
    static JSStringRef getValueName = JSStringCreateWithUTF8CString("getValue");
    
    if (argumentCount > 0) {
        if (JSValueIsObject(ctx, arguments[0])) {
            auto fn = JSObjectGetProperty(ctx, (JSObjectRef)arguments[0], getValueName, exception);
            if (*exception != NULL) return NULL;
            
            if (JSValueIsObject(ctx, fn) && JSObjectIsFunction(ctx, (JSObjectRef)fn)) {
                auto nValue = JSObjectCallAsFunction(ctx, (JSObjectRef)fn, (JSObjectRef)arguments[0], 0, NULL, exception);
                if(*exception != NULL) return NULL;
                number = JSValueToNumber(ctx, nValue, exception);
                if (*exception != NULL) return NULL;
            }
            
        }else{
            number = JSValueToNumber(ctx, arguments[0], exception);
            if (*exception != NULL) return NULL;
        }
    }
    
    auto buffer = new Buffer(sizeof(T));
    *((T*)buffer->bytes) = number;
    auto N = JSObjectMake(ctx, JSClass, buffer);
    
    return N;
}

template<typename T, ffi_type* FT>
JSObjectRef BuildIn<T, FT>::Load(JSContextRef ctx, const char* name)
{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = name;
    JSStaticValue staticValues[] = {
        {"__size__",        GetSize, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__alignment__",   GetAlignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__type__",        GetType, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {NULL, NULL, NULL, 0}
    };
    definition.staticValues = staticValues;
    
    JSStaticFunction staticFunctions[] = {
        {"__setValue__", CallSetValue, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__getValue__", CallGetValue, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {NULL, NULL, 0}
    };
    definition.staticFunctions = staticFunctions;
    definition.callAsFunction = CallConstructor;
    definition.callAsConstructor = Construct_;
    definition.finalize = Finalize;
    definition.parentClass = BuildInClass;
    JSClassRef Class = JSClassCreate(&definition);
    auto in = new BuildIn();
    JSObjectRef In = JSObjectMake(ctx, Class, in);
    JSClassRelease(Class);
    
    return In;
}

JSObjectRef CallAsConstructor(BuildIn)
{
    return argumentCount > 0 && JSValueIsObject(ctx, arguments[0]) ? (JSObjectRef)arguments[0] : JSObjectMake(ctx, NULL, NULL);
}

static JSObjectRef LoadBuildIn(JSContextRef ctx)
{
    JSObjectRef buildIn = JSObjectMakeConstructor(ctx, BuildInClass, ConstructBuildIn);
    
#define ImportBuildIn(N, T, FT) BuildIn<T, &FT>::Load(ctx, "" # N)
    
    JSValueRef UT[8] = {
        ImportBuildIn(UInt8, uint8_t, ffi_type_uint8),
        ImportBuildIn(UInt16, uint16_t, ffi_type_uint16),
        NULL,
        ImportBuildIn(UInt32, uint32_t, ffi_type_uint32),
        NULL,
        NULL,
        NULL,
        ImportBuildIn(UInt64, uint64_t, ffi_type_uint64)
    };
    JSValueRef ST[8] = {
        ImportBuildIn(Int8, int8_t, ffi_type_sint8),
        ImportBuildIn(Int16, int16_t, ffi_type_sint16),
        NULL,
        ImportBuildIn(Int32, int32_t, ffi_type_sint32),
        NULL,
        NULL, NULL,
        ImportBuildIn(Int64, int64_t, ffi_type_sint64)
    };
    
#undef ImportBuildIn
    
    map<const char*, JSValueRef> pmap = {
#define ImportBuildIn(N, T, FT) \
        {"" # N, BuildIn<T, &FT>::Load(ctx, "" # N)},
        
        ImportBuildIn(Void, void, ffi_type_void)
        ImportBuildIn(Pointer, void*, ffi_type_pointer)
        ImportBuildIn(Float, float, ffi_type_float)
        ImportBuildIn(Double, double, ffi_type_double)
        ImportBuildIn(LongDouble, long double, ffi_type_longdouble)
        
#undef ImportBuildIn
        
#define ImportBuildIn(N, T, TC) {"" # N, TC[sizeof(T)-1]},
        
        ImportBuildIn(UInt8, uint8_t, UT)
        ImportBuildIn(UInt16, uint16_t, UT)
        ImportBuildIn(UInt32, uint32_t, UT)
        ImportBuildIn(UInt64, uint64_t, UT)
        
        ImportBuildIn(Int8, uint8_t, ST)
        ImportBuildIn(Int16, uint16_t, ST)
        ImportBuildIn(Int32, uint32_t, ST)
        ImportBuildIn(Int64, uint64_t, ST)
        
        ImportBuildIn(UInt, unsigned int, UT)
        ImportBuildIn(UChar, unsigned char, UT)
        ImportBuildIn(UShort, unsigned short, UT)
        ImportBuildIn(ULong, unsigned long, UT)
        ImportBuildIn(Size, size_t, UT)
        ImportBuildIn(ULongLong, unsigned long long, UT)
        
        ImportBuildIn(Int, int, ST)
        ImportBuildIn(Char, char, ST)
        ImportBuildIn(Short, short, ST)
        ImportBuildIn(Long, long, ST)
        ImportBuildIn(SSize, ssize_t, ST)
        ImportBuildIn(LongLong, long long, ST)
        
        ImportBuildIn(Unsigned, unsigned, UT)
        ImportBuildIn(Signed, signed, ST)
        
        ImportBuildIn(Bool, bool, ST)
        
#undef ImportBuildIn
    };

    for (auto iter = pmap.begin(); iter != pmap.end(); iter++) {
        JSStringRef name = JSStringCreateWithUTF8CString(iter->first);
        JSObjectSetProperty(ctx, buildIn, name, iter->second, kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(name);
    }
    
    return buildIn;
}

JSObjectRef Struct::CallAsConstructor(_)
{
    auto structure = (Struct*)JSObjectGetPrivate(constructor);
    auto buffer = new Buffer(structure->ft->size);
    JSObjectRef instance = JSObjectMake(ctx, NBuffer, buffer);
    
    // 调用 __init__ 方法
    static JSStringRef initName = JSStringCreateWithUTF8CString("__init__");
    JSObjectRef init = (JSObjectRef)JSObjectGetProperty(ctx, constructor, initName, NULL);
    JSValueRef* argv = new JSValueRef[argumentCount+1];
    argv[0] = instance;
    memcpy(argv+1, arguments, argumentCount * sizeof(void*));
    JSObjectCallAsFunction(ctx, init, constructor, argumentCount+1, argv, exception);
    
    return instance;
}

JSObjectRef Struct::CallAsConstructor(Struct)
{
    auto T = (ffi_type*)calloc(1, sizeof(ffi_type) + sizeof(void*) * (argumentCount + 1));
    T->type = FFI_TYPE_STRUCT;
    T->elements = (ffi_type**)(((char*)T)+sizeof(ffi_type));
    
    if (argumentCount > 0) {
        for (auto index = 0; index < argumentCount; index++) {
            ffi_type* field = *(ffi_type**)JSObjectGetPrivate((JSObjectRef)arguments[index]);
            T->elements[index] = field;
        }
        T->elements[argumentCount] = NULL;
    }
    
    ffi_cif cif;
    auto status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, T, NULL);
    if (status != FFI_OK) {
        static const char* MSG[] = {"", "FFI_BAD_TYPEDEF", "FFI_BAD_ABI"};
        JSStringRef msg = JSStringCreateWithUTF8CString(MSG[status]);
        JSValueRef err = JSValueMakeString(ctx, msg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
    }
    
    auto structure = new Struct();
    structure->ft = T;
    
    JSObjectRef Struct = JSObjectMake(ctx, Definition, structure);
    
    return Struct;
}

JSClassRef Struct::Definition = NULL;
JSObjectRef Struct::Load(JSContextRef ctx)
{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Structure";
    JSStaticValue staticValues[] = {
        {"__size__",        GetSize, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__alignment__",   GetAlignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__type__",        GetType, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {NULL, NULL, NULL, 0}
    };
    definition.staticValues = staticValues;
    definition.callAsFunction = CallConstructor;
    definition.callAsConstructor = Construct_;
    definition.finalize = Finalize;
    definition.parentClass = BuildInClass;
    Definition = JSClassCreate(&definition);
    
    JSObjectRef Structure = JSObjectMakeConstructor(ctx, Definition, ConstructStruct);
    return Structure;
}

// MARK: FunctionPointer ffi_closure
// FunctionPointer 是 BuildIn 对象 Pointer 的扩展
// 给 Pointer 对象加上了 __signature__ 属性，同时去除了 setValue 方法
static void func_ptr_free(void* ptr)
{
    void** func_ptr = (void**)ptr;
    ffi_closure_free(func_ptr[1]);
    free(ptr);
}

static void ffi_closure_callback(ffi_cif *cif, void *rptr, void ** aptr, void * userd)
{
    static JSStringRef callable_name = JSStringCreateWithUTF8CString("__callable__");
    void** ud = (void**)userd;
    
    JSGlobalContextRef ctx = (JSGlobalContextRef)ud[2];
    JSObjectRef fn = (JSObjectRef)ud[3];
    
    JSValueRef* argv = new JSValueRef[cif->nargs + 1];
    
    // return value
    argv[0] = JSObjectMake(ctx, NBuffer, new Buffer(rptr, cif->rtype->size));
    for (auto index = 0; index < cif->nargs; index++) {
        argv[index+1] = JSObjectMake(ctx, NBuffer, new Buffer(aptr[index], cif->arg_types[index]->size));
    }
    
    JSValueRef callable = JSObjectGetProperty(ctx, fn, callable_name, NULL);
    JSObjectCallAsFunction(ctx, (JSObjectRef)callable, fn, cif->nargs + 1, argv, NULL);
    delete [] argv;
}

template<>
JSObjectRef BuildIn<ffi_closure, &ffi_type_pointer>::CallAsConstructor(_)
{
    static JSStringRef sig_name = JSStringCreateWithUTF8CString("__signature__");
    
    // 提取 signature
    JSValueRef sigVal = JSObjectGetProperty(ctx, constructor, sig_name, exception);
    if (*exception != NULL) return NULL;
    
    auto signature = (Signature*)JSObjectGetPrivate((JSObjectRef)sigVal);
    
    auto buffer = new Buffer(4 * sizeof(void*), BufferEncodingUTF8, func_ptr_free);
    void** excutor = (void**)buffer->bytes;
    
    // excutor 第一个指针为可执行地址
    excutor[1] = ffi_closure_alloc(sizeof(ffi_closure), excutor);
    
    auto status = ffi_prep_closure((ffi_closure *)excutor[1], *(ffi_cif**)signature, ffi_closure_callback, excutor);
    if (status != FFI_OK) {
        static const char* MSG[] = {"", "FFI_BAD_TYPEDEF", "FFI_BAD_ABI"};
        JSStringRef msg = JSStringCreateWithUTF8CString(MSG[status]);
        JSValueRef err = JSValueMakeString(ctx, msg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
    }
    
    JSObjectRef func = JSObjectMake(ctx, NBuffer, buffer);
    excutor[2] = JSContextGetGlobalContext(ctx);
    excutor[3] = func;
    
    static JSStringRef fp_name = JSStringCreateWithUTF8CString("__FunctionPointer__");
    JSObjectSetProperty(ctx, func, fp_name, constructor, kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, exception);
    if (*exception != NULL) return NULL;
    
    static JSStringRef cb_name = JSStringCreateWithUTF8CString("__callback__");
    if (argumentCount > 0) {
        JSObjectSetProperty(ctx, func, cb_name, arguments[0], 0, exception);
    }
    
    static JSStringRef callable_name = JSStringCreateWithUTF8CString("__callable__");
    JSValueRef callable = JSObjectGetProperty(ctx, (JSObjectRef)sigVal, callable_name, exception);
    if (*exception != NULL) return NULL;
    JSObjectSetProperty(ctx, func, callable_name, callable, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, exception);
    
    return func;
}

template<>
JSObjectRef BuildIn<ffi_closure, &ffi_type_pointer>::Load(JSContextRef ctx, const char *name)
{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = name;
    JSStaticValue staticValues[] = {
        {"__size__",        GetSize, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__alignment__",   GetAlignment, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {"__type__",        GetType, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {NULL, NULL, NULL, 0}
    };
    definition.staticValues = staticValues;
    definition.callAsFunction = CallConstructor;
    definition.callAsConstructor = Construct_;
    definition.finalize = Finalize;
    definition.parentClass = BuildInClass;
    JSClassRef Class = JSClassCreate(&definition);
    auto in = new BuildIn();
    JSObjectRef In = JSObjectMake(ctx, Class, in);
    JSClassRelease(Class);
    return In;
}


// MARK: Signature
const JSClassRef Signature::Definition = []()->JSClassRef{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Signature";
    definition.finalize = Finalize;
    
    definition.callAsFunction = CallExecute;
    definition.callAsConstructor = ConstructFunctionPointer;
    
    return JSClassCreate(&definition);
}();
void Signature::Finalize(JSObjectRef object)
{
    auto signature = (Signature*)JSObjectGetPrivate(object);
    if (signature != NULL) delete signature;
}

JSValueRef Signature::CallAsFunction(Execute)
{
    assert(argumentCount == 3);
    // thisObject 为 Buffer 对象，保存了 C 函数指针
    // 第一个参数为 return value 类型为： Buffer
    // 最后一个参数为 异步回调函数，如果最后一个参数为 undefined ，则进行同步调用，如果是 function 则异步调用，但是必须存在
    // 其余参数为 C 函数执行需要的参数 类型为： Buffer
    // 该方法不应该被直接调用，详见文档
    
    auto cif = *(ffi_cif**)JSObjectGetPrivate(function);
    void (*fn)() = FFI_FN(*(void**)((Buffer*)JSObjectGetPrivate(thisObject))->bytes);
    void* rvalue = (void*)((Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[0]))->bytes;
    
    void** avalue = new void*[argumentCount - 2];
    for (auto index = 1; index < argumentCount - 1; index++) {
        avalue[index - 1] = (void*)((Buffer*)JSObjectGetPrivate((JSObjectRef)arguments[index]))->bytes;
    }
    
    if (JSValueIsObject(ctx, arguments[2]) && JSObjectIsFunction(ctx, (JSObjectRef)arguments[2])) {
        struct UserData {
            const JSGlobalContextRef ctx;
            const size_t argumentCount;
            JSValueRef* arguments;
            ffi_cif* cif;
            void (*fn)();
            void* rvalue;
            void**avalue;
            
            UserData(const JSContextRef c, const size_t s, const JSValueRef a[]): ctx(JSContextGetGlobalContext(c)), argumentCount(s) {
                this->arguments = new JSValueRef[argumentCount];
                JSGlobalContextRetain(ctx);
                for (auto index = 0; index < argumentCount; index++) {
                    JSValueProtect(ctx, a[index]);
                    arguments[index] = a[index];
                }
            }
            
            ~UserData() {
                for (auto index = 0; index < argumentCount; index++) {
                    JSValueUnprotect(ctx, arguments[index]);
                }
                JSGlobalContextRelease(ctx);
                delete [] arguments;
            }
        };
        
        shared_ptr<UserData> userd = make_shared<UserData>(ctx, argumentCount, arguments);
        userd->cif = cif;
        userd->fn = fn;
        userd->rvalue = rvalue;
        userd->avalue = avalue;
        // 从 globalObject 上获得 JNA 对像
        static JSStringRef jna_name = JSStringCreateWithUTF8CString("JSNativeAccessor");
        JSObjectRef JNA = (JSObjectRef)JSObjectGetProperty(ctx, JSContextGetGlobalObject(ctx), jna_name, exception);
        if (*exception != NULL) return NULL;
        if (JSValueIsObject(ctx, JNA)) {
            JSNativeAccessor* accessor = (JSNativeAccessor*)JSObjectGetPrivate(JNA);
            accessor->threadPool()->AddJob([userd](std::function<bool(void)> isCanceled){
                if (!isCanceled()){
                    ffi_call(userd->cif, userd->fn, userd->rvalue, userd->avalue);
                    if (!isCanceled()) {
                        if (scheduleOnJSThread != NULL) {
                            shared_ptr<UserData>* sptr = new shared_ptr<UserData>(userd);
                            scheduleOnJSThread(userd->ctx, [](void* data){
                                shared_ptr<UserData> userd = *(shared_ptr<UserData>*)data;
                                JSObjectCallAsFunction(userd->ctx, (JSObjectRef)userd->arguments[userd->argumentCount-1], NULL, 0, NULL, NULL);
                                delete (shared_ptr<UserData>*)data;
                            }, sptr);
                        }else{
                            JSObjectCallAsFunction(userd->ctx, (JSObjectRef)userd->arguments[userd->argumentCount-1], NULL, 0, NULL, NULL);
                        }
                    }
                }
                delete [] userd->avalue;
            });
        }
        
    }else{
        ffi_call(cif, fn, rvalue, avalue);
        delete [] avalue;
    }
    
    return NULL;
}

JSObjectRef Signature::CallAsConstructor(FunctionPointer)
{
    // 如果已经为 signature 生成了 FunctionPointer 属性，则直接不需要再次生成
    static JSStringRef fp_name = JSStringCreateWithUTF8CString("__FunctionPointer__");
    static JSStringRef sig_name = JSStringCreateWithUTF8CString("__signature__");
    JSValueRef fp = JSObjectGetProperty(ctx, constructor, fp_name, exception);
    if (*exception != NULL) return NULL;
    if (JSValueIsObject(ctx, fp)) return (JSObjectRef)fp;
    
    fp = BuildIn<ffi_closure, &ffi_type_pointer>::Load(ctx, "FunctionPointer");
    
    JSObjectSetProperty(ctx, constructor, fp_name, fp, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, exception);
    JSObjectSetProperty(ctx, (JSObjectRef)fp, sig_name, constructor, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, exception);
    
    return (JSObjectRef)fp;
}

JSObjectRef Signature::CallAsConstructor(_)
{
    // 检查参数类型
    // 如果第一个参数为 Number , 则为 abi 否则为 rtype, offset 从 1 开始算起，小于 1 说明不存在
    size_t abi_offset = min(argumentCount, (size_t)(argumentCount > 0 && JSValueIsNumber(ctx, arguments[0])));
    size_t rtype_offset = abi_offset + 1;
    size_t atypes_offset = rtype_offset + 1;
    
    if (rtype_offset > argumentCount) rtype_offset = 0;
    if (atypes_offset > argumentCount) atypes_offset = 0;
    
    ffi_abi abi = abi_offset ? (ffi_abi)JSValueToNumber(ctx, arguments[abi_offset - 1], exception) : FFI_DEFAULT_ABI;
    if (*exception != NULL) return NULL;
    
    int nargs = atypes_offset ? (int)(argumentCount - (atypes_offset - 1)) : 0;
    
    ffi_cif* cif = (ffi_cif*)calloc(1, sizeof(ffi_cif) + sizeof(ffi_type*) * (nargs + 1));
    ffi_type* rtype = NULL;
    ffi_type** atypes = (ffi_type**)(cif + 1);
    
    rtype = rtype_offset ? *(ffi_type**)JSObjectGetPrivate((JSObjectRef)arguments[rtype_offset - 1]) : &ffi_type_void;
    
    if (atypes_offset) {
        for (auto index = atypes_offset - 1; index < argumentCount; index++) {
            atypes[index - (atypes_offset - 1)] = *(ffi_type**)JSObjectGetPrivate((JSObjectRef)arguments[index]);
        }
    }
    
    auto status = ffi_prep_cif(cif, abi, nargs, rtype, atypes);
    if (status != FFI_OK) {
        static const char* MSG[] = {"", "FFI_BAD_TYPEDEF", "FFI_BAD_ABI"};
        JSStringRef msg = JSStringCreateWithUTF8CString(MSG[status]);
        JSValueRef err = JSValueMakeString(ctx, msg);
        *exception = JSObjectMakeError(ctx, 1, &err, exception);
    }

    auto signature = new Signature();
    signature->cif = cif;
    JSObjectRef sig = JSObjectMake(ctx, Definition, signature);
    
    static JSStringRef abi_name = JSStringCreateWithUTF8CString("ABI");
    static JSStringRef rt_name = JSStringCreateWithUTF8CString("RType");
    static JSStringRef at_name = JSStringCreateWithUTF8CString("ATypes");
    static JSStringRef at_len_name = JSStringCreateWithUTF8CString("length");
    
    JSObjectSetProperty(ctx, sig, abi_name, JSValueMakeNumber(ctx, abi), kJSPropertyAttributeReadOnly, exception);
    JSObjectSetProperty(ctx, sig, rt_name, rtype_offset ? arguments[rtype_offset - 1] : JSValueMakeNull(ctx), kJSPropertyAttributeReadOnly, exception);
    
    JSObjectRef atypes_t = JSObjectMake(ctx, NULL, NULL);
    if (atypes_offset) {
        JSObjectSetProperty(ctx, atypes_t, at_len_name, JSValueMakeNumber(ctx, nargs), kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, exception);
        for (auto index = atypes_offset - 1; index < argumentCount; index++) {
            JSObjectSetPropertyAtIndex(ctx, atypes_t, (unsigned int)(index - (atypes_offset - 1)), arguments[index], exception);
        }
    }
    JSObjectSetProperty(ctx, sig, at_name, atypes_t, kJSPropertyAttributeReadOnly, exception);
    
    return sig;
}

static JSObjectRef LoadFFI(JSContextRef ctx)
{
    JSObjectRef abis = JSObjectMake(ctx, NULL, NULL);
    map<const char*, ffi_abi> abisMap = {
#define IABI(abi) {"" # abi, FFI_## abi ## _ABI},
        
//        IABI(FIRST)
        IABI(DEFAULT)
//        IABI(LAST)
//        {"SYSV", FFI_SYSV},
//        {"STDCALL", FFI_STDCALL},
//        {"THISCALL", FFI_THISCALL},
//        {"FASTCALL", FFI_FASTCALL},
//        {"PASCAL", FFI_PASCAL},
//        {"REGISTER", FFI_REGISTER},
//        
//        /* ---- Intel x86 Win32 ---------- */
//#ifdef X86_WIN32
//        {"MS_CDECL", FFI_MS_CDECL},
//        
//#elif defined(X86_WIN64)
//        {"WIN64", FFI_WIN64},
//#else
//        /* ---- Intel x86 and AMD x86-64 - */
//        {"UNIX64", FFI_UNIX64},   /* Unix variants all use the same ABI for x86-64  */
//#endif
        
#undef IABI
    };
    for (auto iter = abisMap.begin(); iter != abisMap.end(); iter++) {
        JSStringRef name = JSStringCreateWithUTF8CString(iter->first);
        JSObjectSetProperty(ctx, abis, name, JSValueMakeNumber(ctx, iter->second), kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(name);
    }
    
    JSObjectRef ffi = JSObjectMake(ctx, NULL, NULL);
    map<const char*, JSValueRef> pmap = {
        {"BuildIn", LoadBuildIn(ctx)},
        {"Structure", Struct::Load(ctx)},
        {"Signature", JSObjectMakeConstructor(ctx, Signature::Definition, Signature::Construct_)},
        {"ABI", abis}
    };
    
    for (auto iter = pmap.begin(); iter != pmap.end(); iter++) {
        JSStringRef name = JSStringCreateWithUTF8CString(iter->first);
        JSObjectSetProperty(ctx, ffi, name, iter->second, kJSPropertyAttributeReadOnly, NULL);
        JSStringRelease(name);
    }
    
    return ffi;
}

// MARK: JSNativeAccessor
#include "JSNativeAccessor"

const JSClassDefinition JSNativeAccessor::Definition = {
    .version                = JSNativeAccessorVersion,
    .className              = "JSNativeAccessor",
    .initialize             = Initialize,
    .finalize               = Finalize,
    
    .staticValues           = StaticValues,
    .staticFunctions        = StaticFunctions
};

void JSNativeAccessor::Initialize(JSContextRef ctx, JSObjectRef accessor)
{
    JSObjectSetPrivate(accessor, new JSNativeAccessor());
    
    JSObjectRef buffer = JSObjectMakeConstructor(ctx, NBuffer, Buffer::Construct_);
    JSObjectRef stdc = GenerateSTDCInterface(ctx);
    
    JSValueRef *exception = NULL;
    
    JSValueRef argv[] = {
        accessor,
        buffer,
        stdc,
        LoadFFI(ctx)
    };
    JSStringRef anames[] = {
        JSStringCreateWithUTF8CString("JNA"),
        JSStringCreateWithUTF8CString("Buffer"),
        JSStringCreateWithUTF8CString("STDC"),
        JSStringCreateWithUTF8CString("FFI")
    };
    
    string script(BuildInScript);
    
#ifdef DEBUG
    script = "try{\n" + script + "\n}catch(error){if(typeof console.log.e === 'function')console.log.e(error);}";
#endif
    
    JSStringRef body = JSStringCreateWithUTF8CString(script.c_str());
    JSObjectRef init = JSObjectMakeFunction(ctx, NULL, sizeof(anames)/sizeof(JSStringRef), anames, body, NULL, 0, exception);
    
    for (auto index = 0; index < sizeof(anames)/sizeof(JSStringRef); index++) {
        JSStringRelease(anames[index]);
    }
    JSStringRelease(body);
    
    JSObjectCallAsFunction(ctx, init, NULL, sizeof(argv)/sizeof(JSObjectRef), argv, exception);
}

void JSNativeAccessor::Finalize(JSObjectRef object)
{
    auto accessor = (JSNativeAccessor*)JSObjectGetPrivate(object);
    delete accessor;
}

const JSStaticValue JSNativeAccessor::StaticValues[] =
{
    
    {NULL, NULL, NULL, 0}
};

const JSStaticFunction JSNativeAccessor::StaticFunctions[] =
{
  
    {NULL, NULL, 0}
};

// MARK: Base64
static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

static std::string base64_encode(string const& data) {
    auto bytes_to_encode = (unsigned char const*)data.data();
    auto in_len = (unsigned int)data.size();
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i)
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
        
    }
    
    return ret;
    
}

static std::string base64_decode(std::string const& encoded_string) {
    int in_len = (int)encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    
    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);  
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];  
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];  
    }  
    
    return ret;  
}


extern "C" {
    const JSClassDefinition* JSNativeAccessor = &JSNativeAccessor::Definition;
    ScheduleOnJSCThread* scheduleOnJSCThread = &scheduleOnJSThread;
}