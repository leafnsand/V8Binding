#pragma once

#include "CppArg.h"
#include "CppObject.h"
#include "V8Type.h"

#include <v8.h>

enum CppBindMethodCheck
{
    CHK_NORMAL,
    CHK_GETTER,
    CHK_SETTER
};

template <typename T, typename PT = T>
struct CppBindVariableGetter
{
    static void call(v8::Local<v8::String> key, const v8::PropertyCallbackInfo<v8::Value> &v8Args)
    {
        auto ptr = static_cast<const T*>(v8Args.Data().As<v8::External>()->Value());
        assert(ptr);
        v8Args.GetReturnValue().Set(V8Type<PT>::set(*ptr));
    }
};

template <typename T>
struct CppBindVariableSetter
{
    static void call(v8::Local<v8::String> key, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &v8Args)
    {
        auto ptr = static_cast<const T*>(v8Args.Data().As<v8::External>()->Value());
        assert(ptr);
        *ptr = V8Type<T>::get(value);
    }
};

template<int CHK, typename FN, typename R, typename... P>
struct CppBindMethodBase
{
    static_assert(CHK != CHK_GETTER || (!std::is_same<R, void>::value && sizeof...(P) == 0), "the specified function is not getter function");
    static_assert(CHK != CHK_SETTER || (std::is_same<R, void>::value && sizeof...(P) == 1), "the specified function is not setter function");

    static void call(const v8::FunctionCallbackInfo<v8::Value> &v8Args)
    {
        const FN &fn = *reinterpret_cast<const FN *>(v8Args.Data().As<v8::External>()->Value());
        assert(fn);
        CppArgTuple<P...> args;
        CppArgTupleInput<P...>::get(v8Args, 0, args);
        v8Args.GetReturnValue().Set(CppInvokeMethod<FN, R, typename CppArg<P>::HolderType...>::call(fn, args));
    }

    template<typename PROC>
    static FN function(const PROC &fn)
    {
        return static_cast<FN>(fn);
    }
};

template<typename FN, typename ARGS = FN, int CHK = CHK_NORMAL, typename ENABLED = void>
struct CppBindMethod;

template<typename R, typename... P, int CHK>
struct CppBindMethod<R(*)(P...), R(*)(P...), CHK>
    : CppBindMethodBase<CHK, R(*)(P...), R, P...>
{
};

template<typename R, typename... P, int CHK>
struct CppBindMethod<std::function<R(P...)>, std::function<R(P...)>, CHK>
    : CppBindMethodBase<CHK, std::function<R(P...)>, R, P...>
{
};

template<typename R, typename... A, typename... P, int CHK>
struct CppBindMethod<R(*)(A...), _arg(*)(P...), CHK>
    : CppBindMethodBase<CHK, R(*)(A...), R, P...>
{
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename R, typename... A, typename... P, int CHK>
struct CppBindMethod<std::function<R(A...)>, _arg(*)(P...), CHK>
    : CppBindMethodBase<CHK, std::function<R(A...)>, R, P...>
{
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename FN, int CHK>
struct CppBindMethod<FN, FN, CHK,
    typename std::enable_if<CppCouldBeLambda<FN>::value>::type>
    : CppBindMethod<typename CppLambdaTraits<FN>::FunctionType, typename CppLambdaTraits<FN>::FunctionType, CHK> {};

template<typename FN, typename... P, int CHK>
struct CppBindMethod<FN, _arg(*)(P...), CHK,
    typename std::enable_if<CppCouldBeLambda<FN>::value>::type>
    : CppBindMethod<typename CppLambdaTraits<FN>::FunctionType, _arg(*)(P...), CHK> {};

template<typename FN, int CHK>
struct CppBindMethod<FN, FN, CHK,
    typename std::enable_if<std::is_function<FN>::value>::type>
    : CppBindMethod<FN *, FN *, CHK> {};

template<typename FN, typename... P, int CHK>
struct CppBindMethod<FN, _arg(*)(P...), CHK,
    typename std::enable_if<std::is_function<FN>::value>::type>
    : CppBindMethod<FN *, _arg(*)(P...), CHK> {};

template<typename SP, typename T, typename ARGS>
struct CppBindClassConstructor;

template<typename T, typename... P>
struct CppBindClassConstructor<T, T, _arg(*)(P...)>
{
    static void call(const v8::FunctionCallbackInfo<v8::Value> &v8Args)
    {
        CppArgTuple<P...> args;
        CppArgTupleInput<P...>::get(v8Args, 0, args);
        CppObjectValue<T>::instance(v8Args.This(), args);
    }
};

template<typename SP, typename T, typename... P>
struct CppBindClassConstructor<SP, T, _arg(*)(P...)>
{
    static void call(const v8::FunctionCallbackInfo<v8::Value> &v8Args)
    {
        CppArgTuple<P...> args;
        CppArgTupleInput<P...>::get(v8Args, 0, args);
        T *obj = CppInvokeClassConstructor<T>::call(args);
        CppObjectSharedPtr<SP, T>::instance(v8Args.This(), obj);
    }
};

template<typename T, typename V, typename PV = V>
struct CppBindClassVariableGetter
{
    static void call(v8::Local<v8::String> key, const v8::PropertyCallbackInfo<v8::Value> &v8Args)
    {
        auto member = static_cast<V T::* *>(v8Args.Data().As<v8::External>()->Value());
        assert(member);
        const T *obj = CppObject::get<T>(v8Args.This());
        v8Args.GetReturnValue().Set(V8Type<PV>::set(obj->**member));
    }
};

template<typename T, typename V>
struct CppBindClassVariableSetter
{
    static void call(v8::Local<v8::String> key, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &v8Args)
    {
        auto member = static_cast<V T::* *>(v8Args.Data().As<v8::External>()->Value());
        assert(member);
        const T *obj = CppObject::get<T>(v8Args.This());
        obj->**member = V8Type<V>::get(value);
    }
};

template<int CHK, typename T, bool IS_PROXY, bool IS_CONST, typename FN, typename R, typename... P>
struct CppBindClassMethodBase
{
    static_assert(CHK != CHK_GETTER || (!std::is_same<R, void>::value && sizeof...(P) == 0), "the specified function is not getter function");
    static_assert(CHK != CHK_SETTER || (std::is_same<R, void>::value && sizeof...(P) == 1), "the specified function is not setter function");
    static constexpr bool isConst = IS_CONST;

    static void call(const v8::FunctionCallbackInfo<v8::Value> &v8Args)
    {
        auto fn = static_cast<const FN *>(v8Args.Data().As<v8::External>()->Value());
        assert(fn);
        CppArgTuple<P...> args;
        T *obj = CppObject::get<T>(v8Args.This());
        CppArgTupleInput<P...>::get(v8Args, 0, args);
        v8Args.GetReturnValue().Set(CppInvokeClassMethod<T, IS_PROXY, FN, R, typename CppArg<P>::HolderType...>::call(obj, fn, args));
    }

    template<typename PROC>
    static FN function(const PROC &fn)
    {
        return static_cast<FN>(fn);
    }
};

template<typename T, typename FN, typename ARGS = FN, int CHK = CHK_NORMAL, typename ENABLED = void>
struct CppBindClassMethod;

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, R(TF::*)(P...), R(TF::*)(P...), CHK>
    : CppBindClassMethodBase<CHK, T, false, false, R(T::*)(P...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and member function does not match");
};

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, R(TF::*)(P...) const, R(TF::*)(P...) const, CHK>
    : CppBindClassMethodBase<CHK, T, false, true, R(T::*)(P...) const, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and member function does not match");
};

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, R(*)(TF *, P...), R(*)(TF *, P...), CHK,
    typename std::enable_if<!std::is_const<TF>::value>::type>
    : CppBindClassMethodBase<CHK, T, true, false, R(*)(T *, P...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
};

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, R(*)(const TF *, P...), R(*)(const TF *, P...), CHK>
    : CppBindClassMethodBase<CHK, T, true, true, R(*)(const T *, P...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
};

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, std::function<R(TF *, P...)>, std::function<R(TF *, P...)>, CHK,
    typename std::enable_if<!std::is_const<TF>::value>::type>
    : CppBindClassMethodBase<CHK, T, true, false, std::function<R(T *, P...)>, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
};

template<typename T, typename TF, typename R, typename... P, int CHK>
struct CppBindClassMethod<T, std::function<R(const TF *, P...)>, std::function<R(const TF *, P...)>, CHK>
    : CppBindClassMethodBase<CHK, T, true, true, std::function<R(T *, P...)>, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, R(TF::*)(A...), _arg(*)(P...), CHK>
    : CppBindClassMethodBase<CHK, T, false, false, R(T::*)(A...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and member function does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, R(TF::*)(A...) const, _arg(*)(P...), CHK>
    : CppBindClassMethodBase<CHK, T, false, true, R(T::*)(A...) const, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and member function does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, R(*)(TF *, A...), _arg(*)(P...), CHK,
    typename std::enable_if<!std::is_const<TF>::value>::type>
    : CppBindClassMethodBase<CHK, T, true, false, R(*)(T *, A...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, R(*)(const TF *, A...), _arg(*)(P...), CHK>
    : CppBindClassMethodBase<CHK, T, true, true, R(*)(const T *, A...), R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, std::function<R(TF *, A...)>, _arg(*)(P...), CHK,
    typename std::enable_if<!std::is_const<TF>::value>::type>
    : CppBindClassMethodBase<CHK, T, true, false, std::function<R(T *, A...)>, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename TF, typename R, typename... A, typename... P, int CHK>
struct CppBindClassMethod<T, std::function<R(const TF *, A...)>, _arg(*)(P...), CHK>
    : CppBindClassMethodBase<CHK, T, true, true, std::function<R(const T *, A...)>, R, P...>
{
    static_assert(std::is_base_of<TF, T>::value, "class type and function argument type does not match");
    static_assert(sizeof...(A) == sizeof...(P), "the number of arguments and argument-specs do not match");
};

template<typename T, typename FN, int CHK>
struct CppBindClassMethod<T, FN, FN, CHK,
    typename std::enable_if<CppCouldBeLambda<FN>::value>::type>
    : CppBindClassMethod<T, typename CppLambdaTraits<FN>::FunctionType, typename CppLambdaTraits<FN>::FunctionType, CHK> {};

template<typename T, typename FN, typename... P, int CHK>
struct CppBindClassMethod<T, FN, _arg(*)(P...), CHK,
    typename std::enable_if<CppCouldBeLambda<FN>::value>::type>
    : CppBindClassMethod<T, typename CppLambdaTraits<FN>::FunctionType, _arg(*)(P...), CHK> {};

template<typename T, typename FN, int CHK>
struct CppBindClassMethod<T, FN, FN, CHK,
    typename std::enable_if<std::is_function<FN>::value>::type>
    : CppBindClassMethod<T, FN *, FN *, CHK> {};

template<typename T, typename FN, typename... P, int CHK>
struct CppBindClassMethod<T, FN, _arg(*)(P...), CHK,
    typename std::enable_if<std::is_function<FN>::value>::type>
    : CppBindClassMethod<T, FN *, _arg(*)(P...), CHK> {};

#define V8_SP(...) static_cast<__VA_ARGS__*>(nullptr)
#define V8_DEL(...) static_cast<__VA_ARGS__**>(nullptr)

template<typename T, typename PARENT>
class CppBindClass
{
    friend class CppBindModule;

    template<typename TX, typename PX>
    friend class CppBindClass;

private:
    v8::Local<v8::FunctionTemplate> handle;

    explicit CppBindClass(v8::Local<v8::FunctionTemplate> handle) : handle(handle) {}

    CppBindClass(const CppBindClass &that) = delete;

    CppBindClass(CppBindClass &&that) = delete;

    CppBindClass<T, PARENT> &operator=(const CppBindClass<T, PARENT> &that) = delete;

    CppBindClass<T, PARENT> &operator=(CppBindClass<T, PARENT> &&that) = delete;

    static CppBindClass<T, PARENT> bind(v8::Local<v8::Object> parent, const char *name)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        v8::Local<v8::FunctionTemplate> handle;
        auto key = V8Type<const char *>::set(name);
        if (parent->HasOwnProperty(key))
        {
            handle = parent->Get(key);
        }
        else
        {
            handle = v8::FunctionTemplate::New(v8::Isolate::GetCurrent());
            handle->SetClassName(key);
            handle->InstanceTemplate()->SetInternalFieldCount(1);
            handle->GetFunction()->Set(V8Type<const char *>::set("___parent"), parent);
            parent->Set(key, handle);
        }
        return CppBindClass<T, PARENT>(handle);
    }

    template<typename SUPER>
    static CppBindClass<T, PARENT> extend(v8::Local<v8::Object> parent, const char *name)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        v8::Local<v8::FunctionTemplate> handle;
        auto key = V8Type<const char *>::set(name);
        if (parent->HasOwnProperty(key))
        {
            handle = parent->Get(key);
        }
        else
        {
            handle = v8::FunctionTemplate::New(v8::Isolate::GetCurrent());
            handle->SetClassName(key);
            handle->InstanceTemplate()->SetInternalFieldCount(1);
            handle->GetFunction()->Set(V8Type<const char *>::set("___parent"), parent);
            handle->Inherit(CppClassPersistent<SUPER>::persistent.Get(v8::Isolate::GetCurrent()));
            parent->Set(key, handle);
        }
        return CppBindClass<T, PARENT>(handle);
    }

public:
    template<typename V>
    CppBindClass<T, PARENT> &addConstant(const char *name, const V &v)
    {
        handle->Set(V8Type<const char *>::set(name), V8Type<V>::set(v), v8::ReadOnly);
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addStaticVariable(const char *name, V *v, bool writable = true)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->GetFunction()->SetAccessor(V8Type<const char *>::set(name),
                                           &CppBindVariableGetter<V>::call,
                                           writable ? &CppBindVariableSetter<V>::call : nullptr,
                                           v8::External::New(v8::Isolate::GetCurrent(), v),
                                           v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addStaticVariable(const char *name, const V *v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->GetFunction()->SetAccessor(V8Type<const char *>::set(name),
                                           &CppBindVariableGetter<V>::call,
                                           nullptr,
                                           v8::External::New(v8::Isolate::GetCurrent(), const_cast<V *>(v)),
                                           v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    typename std::enable_if<std::is_copy_assignable<V>::value, CppBindClass<T, PARENT> &>::type
    addStaticVariableRef(const char *name, V *v, bool writable = true)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->GetFunction()->SetAccessor(V8Type<const char *>::set(name),
                                           &CppBindVariableGetter<V, V &>::call,
                                           writable ? &CppBindVariableSetter<V>::call : nullptr,
                                           v8::External::New(v8::Isolate::GetCurrent(), v),
                                           v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    typename std::enable_if<!std::is_copy_assignable<V>::value, CppBindClass<T, PARENT> &>::type
    addStaticVariableRef(const char *name, V *v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->GetFunction()->SetAccessor(V8Type<const char *>::set(name),
                                           &CppBindVariableGetter<V, V &>::call,
                                           nullptr,
                                           v8::External::New(v8::Isolate::GetCurrent(), v),
                                           v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addStaticVariableRef(const char *name, const V *v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->GetFunction()->SetAccessor(V8Type<const char *>::set(name),
                                           &CppBindVariableGetter<V, const V &>::call,
                                           nullptr,
                                           v8::External::New(v8::Isolate::GetCurrent(), const_cast<V *>(v)),
                                           v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename FG, typename FS>
    CppBindClass<T, PARENT> &addStaticProperty(const char *name, const FG &get, const FS &set)
    {
        using CppGetter = CppBindMethod<FG, FG, CHK_GETTER>;
        using CppSetter = CppBindMethod<FS, FS, CHK_SETTER>;
        handle->GetFunction()->SetAccessorProperty(V8Type<const char *>::set(name),
                                                   v8::Function::New(v8::Isolate::GetCurrent(), &CppGetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppGetter::function(get))),
                                                   v8::Function::New(v8::Isolate::GetCurrent(), &CppSetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppSetter::function(set))),
                                                   v8::ReadOnly);
        return *this;
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addStaticProperty(const char *name, const FN &get)
    {
        using CppGetter = CppBindMethod<FN, FN, CHK_GETTER>;
        handle->GetFunction()->SetAccessorProperty(V8Type<const char *>::set(name),
                                                   v8::Function::New(v8::Isolate::GetCurrent(), &CppGetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppGetter::function(get))),
                                                   nullptr,
                                                   v8::ReadOnly);
        return *this;
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addStaticFunction(const char *name, const FN &proc)
    {
        using CppProc = CppBindMethod<FN>;
        handle->GetFunction()->Set(V8Type<const char *>::set(name), v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))));
        return *this;
    }

    template<typename FN, typename ARGS>
    CppBindClass<T, PARENT> &addStaticFunction(const char *name, const FN &proc, ARGS)
    {
        using CppProc = CppBindMethod<FN, ARGS>;
        handle->GetFunction()->Set(V8Type<const char *>::set(name), v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))));
        return *this;
    }

    template<typename ARGS>
    CppBindClass<T, PARENT> &addConstructor(ARGS)
    {
        handle->SetCallHandler(&CppBindClassConstructor<T, T, ARGS>::call);
        return *this;
    }

    template<typename SP, typename ARGS>
    CppBindClass<T, PARENT> &addConstructor(SP *, ARGS)
    {
        handle->SetCallHandler(&CppBindClassConstructor<SP, T, ARGS>::call);
        return *this;
    }

    template<typename DEL, typename ARGS>
    CppBindClass<T, PARENT> &addConstructor(DEL **, ARGS)
    {
        handle->SetCallHandler(&CppBindClassConstructor<std::unique_ptr<T, DEL>, T, ARGS>::call);
        return *this;
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addFactory(const FN &proc)
    {
        using CppProc = CppBindMethod<FN, FN>;
        handle->SetCallHandler(v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))));
        return *this;
    }

    template<typename FN, typename ARGS>
    CppBindClass<T, PARENT> &addFactory(const FN &proc, ARGS)
    {
        using CppProc = CppBindMethod<FN, ARGS>;
        handle->SetCallHandler(v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))));
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addVariable(const char *name, V T::* v, bool writable = true)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->PrototypeTemplate()->SetAccessor(V8Type<const char *>::set(name),
                                                 &CppBindClassVariableGetter<T, V>::call,
                                                 writable ? &CppBindClassVariableSetter<T, V>::call : nullptr,
                                                 v8::External::New(v8::Isolate::GetCurrent(), v),
                                                 v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addVariable(const char *name, const V T::* v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->PrototypeTemplate()->SetAccessor(V8Type<const char *>::set(name),
                                                 &CppBindClassVariableGetter<T, V>::call,
                                                 nullptr,
                                                 v8::External::New(v8::Isolate::GetCurrent(), const_cast<V T::*>(v)),
                                                 v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    typename std::enable_if<std::is_copy_assignable<V>::value, CppBindClass<T, PARENT> &>::type
    addVariableRef(const char *name, V T::* v, bool writable = true)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->PrototypeTemplate()->SetAccessor(V8Type<const char *>::set(name),
                                                 &CppBindClassVariableGetter<T, V, V &>::call,
                                                 writable ? &CppBindClassVariableSetter<T, V>::call : nullptr,
                                                 v8::External::New(v8::Isolate::GetCurrent(), v),
                                                 v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    typename std::enable_if<!std::is_copy_assignable<V>::value, CppBindClass<T, PARENT> &>::type
    addVariableRef(const char *name, V T::* v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->PrototypeTemplate()->SetAccessor(V8Type<const char *>::set(name),
                                                 &CppBindClassVariableGetter<T, V, V &>::call,
                                                 nullptr,
                                                 v8::External::New(v8::Isolate::GetCurrent(), v),
                                                 v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename V>
    CppBindClass<T, PARENT> &addVariableRef(const char *name, const V T::* v)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        handle->PrototypeTemplate()->SetAccessor(V8Type<const char *>::set(name),
                                                 &CppBindClassVariableGetter<T, V, const V &>::call,
                                                 nullptr,
                                                 v8::External::New(v8::Isolate::GetCurrent(), const_cast<V T::*>(v)),
                                                 v8::DEFAULT, v8::ReadOnly);
        return *this;
    }

    template<typename FG, typename FS>
    CppBindClass<T, PARENT> &addProperty(const char *name, const FG &get, const FS &set)
    {
        using CppGetter = CppBindClassMethod<T, FG, FG, CHK_GETTER>;
        using CppSetter = CppBindClassMethod<T, FS, FS, CHK_SETTER>;
        handle->PrototypeTemplate()->SetAccessorProperty(V8Type<const char *>::set(name),
                                                         v8::Function::New(v8::Isolate::GetCurrent(), &CppGetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppGetter::function(get))),
                                                         v8::Function::New(v8::Isolate::GetCurrent(), &CppSetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppSetter::function(set))),
                                                         v8::ReadOnly);
        return *this;
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addProperty(const char *name, const FN &get)
    {
        return addPropertyReadOnly(name, get);
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addPropertyReadOnly(const char *name, const FN &get)
    {
        using CppGetter = CppBindClassMethod<T, FN, FN, CHK_GETTER>;
        handle->PrototypeTemplate()->SetAccessorProperty(V8Type<const char *>::set(name),
                                                         v8::Function::New(v8::Isolate::GetCurrent(), &CppGetter::call, v8::External::New(v8::Isolate::GetCurrent(), CppGetter::function(get))),
                                                         nullptr,
                                                         v8::ReadOnly);
        return *this;
    }

    template<typename FN>
    CppBindClass<T, PARENT> &addFunction(const char *name, const FN &proc)
    {
        using CppProc = CppBindClassMethod<T, FN>;
        handle->PrototypeTemplate()->Set(V8Type<const char *>::set(name),
                                         v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))),
                                         v8::ReadOnly);
        return *this;
    }

    template<typename FN, typename ARGS>
    CppBindClass<T, PARENT> &addFunction(const char *name, const FN &proc, ARGS)
    {
        using CppProc = CppBindClassMethod<T, FN, ARGS>;
        handle->PrototypeTemplate()->Set(V8Type<const char *>::set(name),
                                         v8::Function::New(v8::Isolate::GetCurrent(), &CppProc::call, v8::External::New(v8::Isolate::GetCurrent(), CppProc::function(proc))),
                                         v8::ReadOnly);
        return *this;
    }

    template<typename SUB>
    CppBindClass<SUB, CppBindClass<T, PARENT>> beginClass(const char *name)
    {
        return CppBindClass<SUB, CppBindClass<T, PARENT>>::bind(handle->GetFunction(), name);
    }

    template<typename SUB, typename SUPER>
    CppBindClass<SUB, CppBindClass<T, PARENT>> beginExtendClass(const char *name)
    {
        return CppBindClass<SUB, CppBindClass<T, PARENT>>::template extend<SUPER>(handle->GetFunction(), name);
    }

    PARENT endClass()
    {
        return PARENT(handle->GetFunction()->Get(V8Type<const char *>::set("___parent")));
    }
};