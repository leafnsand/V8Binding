#pragma once

#include "V8Type.h"

#include <functional>
#include <vector>

#include <v8.h>

template<typename FN>
struct CppCouldBeLambda
{
    static constexpr bool value = std::is_class<FN>::value;
};

template<typename R, typename... P>
struct CppCouldBeLambda<std::function<R(P...)>>
{
    static constexpr bool value = false;
};

template<typename FN>
struct CppLambdaTraits
    : public CppLambdaTraits<decltype(&FN::operator())> {};

template<typename FN, typename R, typename... P>
struct CppLambdaTraits<R(FN::*)(P...) const>
{
    using FunctionType = std::function<R(P...)>;
};

template<typename FN, typename R, typename... P>
struct CppLambdaTraits<R(FN::*)(P...)>
{
    using FunctionType = std::function<R(P...)>;
};

template<typename FN, typename R, typename TUPLE, size_t N, size_t... INDEX>
struct CppDispatchMethod
    : CppDispatchMethod<FN, R, TUPLE, N - 1, N - 1, INDEX...> {};

template<typename FN, typename R, typename TUPLE, size_t... INDEX>
struct CppDispatchMethod<FN, R, TUPLE, 0, INDEX...>
{
    static R call(const FN &func, TUPLE &args)
    {
        return func(std::get<INDEX>(args).value()...);
    }
};

template<typename FN, typename R, typename... P>
struct CppInvokeMethod
{
    static v8::Local<v8::Value> call(const FN &func, std::tuple<P...> &args)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(CppDispatchMethod<FN, R, std::tuple<P...>, sizeof...(P)>::call(func, args));
    }
};

template<typename FN, typename... P>
struct CppInvokeMethod<FN, void, P...>
{
    static void call(const FN &func, std::tuple<P...> &args)
    {
        CppDispatchMethod<FN, void, std::tuple<P...>, sizeof...(P)>::call(func, args);
    }
};

template<typename T, typename TUPLE, size_t N, size_t... INDEX>
struct CppDispatchClassConstructor
    : CppDispatchClassConstructor<T, TUPLE, N - 1, N - 1, INDEX...> {};

template<typename T, typename TUPLE, size_t... INDEX>
struct CppDispatchClassConstructor<T, TUPLE, 0, INDEX...>
{
    static T *call(TUPLE &args)
    {
        return new T(std::get<INDEX>(args).value()...);
    }

    static T *call(void *mem, TUPLE &args)
    {
        return ::new(mem) T(std::get<INDEX>(args).value()...);
    }
};

template<typename T>
struct CppInvokeClassConstructor
{
    template<typename... P>
    static T *call(std::tuple<P...> &args)
    {
        return CppDispatchClassConstructor<T, std::tuple<P...>, sizeof...(P)>::call(args);
    }

    template<typename... P>
    static T *call(void *mem, std::tuple<P...> &args)
    {
        return CppDispatchClassConstructor<T, std::tuple<P...>, sizeof...(P)>::call(mem, args);
    }
};

template<typename T, bool IS_PROXY, typename FN, typename R, typename TUPLE, size_t N, size_t... INDEX>
struct CppDispatchClassMethod
    : CppDispatchClassMethod<T, IS_PROXY, FN, R, TUPLE, N - 1, N - 1, INDEX...> {};

template<typename T, typename FN, typename R, typename TUPLE, size_t... INDEX>
struct CppDispatchClassMethod<T, false, FN, R, TUPLE, 0, INDEX...>
{
    static R call(T *t, const FN &fn, TUPLE &args)
    {
        return (t->*fn)(std::get<INDEX>(args).value()...);
    }
};

template<typename T, typename FN, typename R, typename TUPLE, size_t... INDEX>
struct CppDispatchClassMethod<T, true, FN, R, TUPLE, 0, INDEX...>
{
    static R call(T *t, const FN &fn, TUPLE &args)
    {
        return fn(t, std::get<INDEX>(args).value()...);
    }
};

template<typename T, bool IS_PROXY, typename FN, typename R, typename... P>
struct CppInvokeClassMethod
{
    static v8::Local<v8::Value> call(T *t, const FN &func, std::tuple<P...> &args)
    {
        return V8Type<R>::set(CppDispatchClassMethod<T, IS_PROXY, FN, R, std::tuple<P...>, sizeof...(P)>::call(t, func, args));
    }
};

template<typename T, bool IS_PROXY, typename FN, typename... P>
struct CppInvokeClassMethod<T, IS_PROXY, FN, void, P...>
{
    static void call(T *t, const FN &func, std::tuple<P...> &args)
    {
        CppDispatchClassMethod<T, IS_PROXY, FN, void, std::tuple<P...>, sizeof...(P)>::call(t, func, args);
    }
};