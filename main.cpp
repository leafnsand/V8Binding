#include "include/CppArg.h"
#include "include/CppBindClass.h"
#include "include/CppBindModule.h"
#include "include/CppFunction.h"
#include "include/CppInvoke.h"
#include "include/CppObject.h"
#include "include/V8Type.h"

#include <v8.h>
#include <libplatform/libplatform.h>

#include <string>
#include <iostream>

class Test
{
public:
    Test(const char *name) : name(name) {}

    void test() { std::cout << name << std::endl; }

    std::string name;
};

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator
{
public:
    virtual void *Allocate(size_t length)
    {
        auto data = AllocateUninitialized(length);
        return data == nullptr ? data : memset(data, 0, length);
    }

    virtual void *AllocateUninitialized(size_t length)
    {
        return malloc(length);
    }

    virtual void Free(void *data, size_t)
    {
        free(data);
    }
};

int main(int argc, char *argv[])
{
    v8::V8::InitializeICU();
    auto mPlatform = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(mPlatform);
    v8::V8::Initialize();
    auto mAllocator = new ArrayBufferAllocator;
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator = mAllocator;
    auto mIsolate = v8::Isolate::New(params);
    mIsolate->Enter();
    {
        v8::HandleScope scope(mIsolate);
        v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(mIsolate);
        v8::Handle<v8::Context> context = v8::Context::New(mIsolate, nullptr, global);
        context->Enter();
    }
    {
        V8Binding(v8::Isolate::GetCurrent()->GetCurrentContext()->Global())
            .beginModule("Module")
                .beginClass<Test>("Class")
                    .addConstructor(V8_ARGS(const char *))
                    .addFunction("test", &Test::test)
                .endClass()
            .endModule();
    }
    mIsolate->Exit();
    mIsolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete mPlatform;
    delete mAllocator;
    return 1;
}