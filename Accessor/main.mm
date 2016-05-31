//
//  main.m
//  Accessor
//
//  Created by 程巍巍 on 5/25/16.
//  Copyright © 2016 程巍巍. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>

#import "ffi.h"

extern JSClassDefinition* JSNativeAccessor;
static void ffi();
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        
        JSContext *context = [JSContext new];
        context[@"console"][@"log"] = ^(){
            NSArray* argv = [JSContext currentArguments];
            printf("=> ");
            for (NSInteger index = 0; index < argv.count; index++) {
                if (index > 0) printf(", ");
                printf("%s", [[argv[index] toString] UTF8String]);
            }
            printf("\n");
        };
        context.exceptionHandler = ^(JSContext* context, JSValue* exception) {
            printf("exception => %s\n%s\n", exception.toString.UTF8String, [exception toDictionary].description.UTF8String);
        };
        
        context[@"console"][@"log"][@"e"] = ^() {
            NSArray* argv = [JSContext currentArguments];
            if (argv.count == 0) return;
            JSContext *context = [JSContext currentContext];
            context.exceptionHandler(context, argv[0]);
        };
        
        JSContextRef ctx = [context JSGlobalContextRef];
        
        JSClassRef define = JSClassCreate(JSNativeAccessor);
        context[@"JSNativeAccessor"] = [JSValue valueWithJSValueRef:JSObjectMake(ctx, define, NULL) inContext:context];
        
        JSClassRelease(define);
        
        NSString* script = [NSString stringWithContentsOfFile:@"./JSNativeAccessor.js" encoding:NSUTF8StringEncoding error:nil];
        
        [context evaluateScript:script];
    }
    ffi();
    return 0;
}

static void ffi() {

    
}