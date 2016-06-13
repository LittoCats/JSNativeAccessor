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
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <list>
#include <functional>
#include <condition_variable>

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
    
public:
    Buffer(size_t len, unsigned int ec = BufferEncodingUTF8, void (*bfree)(void*) = free): length(len), bytes(len > 0 ? malloc(len) : NULL), encoding(ec), bfree(bfree) {}
    Buffer(void* bs, size_t len, unsigned int ec = BufferEncodingUTF8, void (*bfree)(void*) = NULL): length(len), bytes(bs), encoding(ec), bfree(bfree) {}
    ~Buffer(){if (bytes != NULL && bfree != NULL) bfree((void*)bytes);}
    
public:
    template<typename T>
    static bool isAsignFrom(T* data)
    {
        return data != NULL && data != nullptr && *(void**)data == &Definition;
    }
    
public:
    static void Finalize(JSObjectRef object);
private:
    static CallAsFunction(Compare);
    static CallAsFunction(Copy);
    static CallAsFunction(Equals);
    static CallAsFunction(Fill);
    static CallAsFunction(IndexOf);
    static CallAsFunction(Write);
    static CallAsFunction(Slice);
    static CallAsFunction(ToString);
    
private:
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

// MARK: ThreadPool 用于异步任务
template <unsigned ThreadCount = 8>
class ThreadPool {
private:
    std::array<std::thread, ThreadCount> threads;
    std::list<std::function<void(std::function<bool(void)>)>> queue;
    
    std::atomic_int         jobs_left;
    std::atomic_bool        bailout;
    std::atomic_bool        finished;
    std::atomic_bool        canceled;
    std::condition_variable job_available_var;
    std::condition_variable wait_var;
    std::mutex              wait_mutex;
    std::mutex              queue_mutex;
    
    bool isCanceled() {
        return canceled;
    }
    /**
     *  Take the next job in the queue and run it.
     *  Notify the main thread that a job has completed.
     */
    void Task(){
        while (!bailout) {
            next_job()([this]{return this->isCanceled();});
            --jobs_left;
            wait_var.notify_all();
        }
    }
    
    /**
     *  Get the next job; pop the first item in the queue,
     *  otherwise wait for a signal from the main thread.
     */
    std::function<void(std::function<bool(void)>)> next_job() {
        
        std::function<void(std::function<bool(void)>)> res;
        std::unique_lock<std::mutex> job_lock( queue_mutex );
        
        // Wait for a job if we don't have any.
        job_available_var.wait( job_lock, [this]() ->bool { return queue.size() || bailout; } );
        
        // Get job from the queue
        if( !bailout ) {
            res = queue.front();
            queue.pop_front();
        }
        else { // If we're bailing out, 'inject' a job into the queue to keep jobs_left accurate.
            res = [](std::function<bool(void)>){};
            ++jobs_left;
        }
        return res;
    }
    
public:
    ThreadPool()
    : jobs_left( 0 )
    , bailout(false)
    , finished(false)
    , canceled(false)
    {
        for( unsigned i = 0; i < ThreadCount; ++i )
            threads[ i ] = std::move( std::thread( [this,i]{ this->Task(); } ) );
    }
    
    /**
     * 停此所有线程，并清空所有任务
     */
    ~ThreadPool() {
        canceled = true;
        JoinAll();
    }
    
    inline unsigned Size() const {
        return ThreadCount;
    }
    
    inline unsigned JobsRemaining() {
        std::lock_guard<std::mutex> guard( queue_mutex );
        return queue.size();
    }
    
    /**
     *  Add a new job to the pool. If there are no jobs in the queue,
     *  a thread is woken up to take the job. If all threads are busy,
     *  the job is added to the end of the queue.
     */
    void AddJob( std::function<void(std::function<bool(void)>)> job ){
        std::lock_guard<std::mutex> guard( queue_mutex );
        queue.emplace_back( job );
        ++jobs_left;
        job_available_var.notify_one();
    }
    
    /**
     *  Join with all threads. Block until all threads have completed.
     *  Params: WaitForAll: If true, will wait for the queue to empty
     *          before joining with threads. If false, will complete
     *          current jobs, then inform the threads to exit.
     *  The queue will be empty after this call, and the threads will
     *  be done. After invoking `ThreadPool::JoinAll`, the pool can no
     *  longer be used. If you need the pool to exist past completion
     *  of jobs, look to use `ThreadPool::WaitAll`.
     */
    void JoinAll( bool WaitForAll = true ) {
        if( !finished ) {
            if( WaitForAll ) {
                WaitAll();
            }
            
            // note that we're done, and wake up any thread that's
            // waiting for a new job
            bailout = true;
            job_available_var.notify_all();
            
            for( auto &x : threads )
                if( x.joinable() )
                    x.join();
            finished = true;
        }
    }
    
    /**
     *  Wait for the pool to empty before continuing.
     *  This does not call `std::thread::join`, it only waits until
     *  all jobs have finshed executing.
     */
    void WaitAll() {
        if( jobs_left > 0 ) {
            std::unique_lock<std::mutex> lk( wait_mutex );
            wait_var.wait( lk, [this]{ return this->jobs_left == 0; } );
            lk.unlock();
        }
    }
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
    static JSClassRef JSClass;
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
private:
    ThreadPool<> _threadPool;
    
public:
    ThreadPool<>* threadPool() {return &_threadPool;}
    
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
