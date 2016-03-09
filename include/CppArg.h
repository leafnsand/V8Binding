#pragma once

#include "V8Type.h"

#include <v8.h>

#include <cstdint>
#include <type_traits>

struct _arg {};

template<typename T>
struct _opt {};

template<typename T, std::intmax_t DEF_NUM, std::intmax_t DEF_DEN = 1>
struct _def {};

template<typename T>
struct _out {};

template<typename T>
struct _ref {};

template<typename T>
struct _ref_opt {};

template<typename T, std::intmax_t DEF_NUM, std::intmax_t DEF_DEN = 1>
struct _ref_def {};

#define V8_ARGS_TYPE(...) _arg(*)(__VA_ARGS__)
#define V8_ARGS(...) static_cast<V8_ARGS_TYPE(__VA_ARGS__)>(nullptr)

#define V8_FN(r, m, ...) static_cast<r(*)(__VA_ARGS__)>(&m)
#define V8_MEMFN(t, r, m, ...) static_cast<r(t::*)(__VA_ARGS__)>(&t::m)

template<typename T>
struct CppArgHolder
{
    T &value()
    {
        return holder;
    }

    const T &value() const
    {
        return holder;
    }

    void hold(const T &v)
    {
        holder = v;
    }

    T holder;
};

template<typename T>
struct CppArgHolder<T &>
{
    T &value() const
    {
        return *holder;
    }

    void hold(T &v)
    {
        holder = &v;
    }

    T *holder;
};

template<typename T>
struct CppArgTraits
{
    using Type = T;
    using ValueType = typename std::result_of<decltype(&V8Type<T>::get)(v8::MaybeLocal<v8::Value>)>::type;
    using HolderType = CppArgHolder<ValueType>;

    static constexpr bool isInput = true;
    static constexpr bool isOutput = false;
    static constexpr bool isOptional = false;
    static constexpr bool hasDefault = false;
};

template<typename T>
struct CppArgTraits<_opt<T>>
    : CppArgTraits<T>
{
    using Type = T;
    using ValueType = typename std::decay<T>::type;
    using HolderType = CppArgHolder<ValueType>;

    static constexpr bool isOptional = true;
};

template<typename T, std::intmax_t NUM, std::intmax_t DEN>
struct CppArgTraits<_def<T, NUM, DEN>>
    : CppArgTraits<_opt<T>>
{
    static constexpr bool hasDefault = true;
    static constexpr T defaultValue = T(T(NUM) / DEN);
};

template<typename T>
struct CppArgTraits<_out<T>>
    : CppArgTraits<T>
{
    static_assert(std::is_lvalue_reference<T>::value
                  && !std::is_const<typename std::remove_reference<T>::type>::value,
                  "argument with out spec must be non-const reference type");
    static constexpr bool isInput = false;
    static constexpr bool isOutput = true;
};

template<typename T>
struct CppArgTraits<_ref<T>>
    : CppArgTraits<T>
{
    static_assert(std::is_lvalue_reference<T>::value
                  && !std::is_const<typename std::remove_reference<T>::type>::value,
                  "argument with ref spec must be non-const reference type");
    static constexpr bool isOutput = true;
};

template<typename T>
struct CppArgTraits<_ref_opt<T>>
    : CppArgTraits<_opt<T>>
{
    static_assert(std::is_lvalue_reference<T>::value
                  && !std::is_const<typename std::remove_reference<T>::type>::value,
                  "argument with ref spec must be non-const reference type");
    static constexpr bool isOutput = true;
};

template<typename T, std::intmax_t NUM, std::intmax_t DEN>
struct CppArgTraits<_ref_def<T, NUM, DEN>>
    : CppArgTraits<_def<T, NUM, DEN>>
{
    static_assert(std::is_lvalue_reference<T>::value
                  && !std::is_const<typename std::remove_reference<T>::type>::value,
                  "argument with ref spec must be non-const reference type");
    static constexpr bool isOutput = true;
};

template<typename Traits, bool IsInput, bool IsOptional, bool HasDefault>
struct CppArgInput;

template<typename Traits, bool IsOptional, bool HasDefault>
struct CppArgInput<Traits, false, IsOptional, HasDefault>
{
    static void get(v8::MaybeLocal<v8::Value>, typename Traits::HolderType &) {}
};

template<typename Traits, bool HasDefault>
struct CppArgInput<Traits, true, false, HasDefault>
{
    static void get(v8::MaybeLocal<v8::Value> handle, typename Traits::HolderType &r)
    {
        r.hold(V8Type<typename Traits::Type>::get(handle));
    }
};

template<typename Traits>
struct CppArgInput<Traits, true, true, false>
{
    static void get(v8::MaybeLocal<v8::Value> handle, typename Traits::HolderType &r)
    {
        using DefaultType = typename std::decay<typename Traits::ValueType>::type;
        r.hold(V8Type<typename Traits::Type>::opt(handle, DefaultType()));
    }
};

template<typename Traits>
struct CppArgInput<Traits, true, true, true>
{
    static void get(v8::MaybeLocal<v8::Value> handle, typename Traits::HolderType &r)
    {
        r.hold(V8Type<typename Traits::Type>::opt(handle, Traits::defaultValue));
    }
};

template<typename Traits, bool IsOutput>
struct CppArgOutput;

template<typename Traits>
struct CppArgOutput<Traits, false>
{
    static v8::MaybeLocal<v8::Value> set(const typename Traits::ValueType &) {}
};

template<typename Traits>
struct CppArgOutput<Traits, true>
{
    static v8::MaybeLocal<v8::Value> set(const typename Traits::ValueType &v)
    {
        return V8Type<typename Traits::Type>::set(v);
    }
};

template<typename T>
struct CppArg
{
    using Traits = CppArgTraits<T>;
    using Type = typename Traits::Type;
    using HolderType = typename Traits::HolderType;

    static void get(v8::MaybeLocal<v8::Value> handle, HolderType &r)
    {
        return CppArgInput<Traits, Traits::isInput, Traits::isOptional, Traits::hasDefault>::get(handle, r);
    }

    static v8::MaybeLocal<v8::Value> set(const HolderType &v)
    {
        return CppArgOutput<Traits, Traits::isOutput>::set(v.value());
    }
};

template<typename... P>
using CppArgTuple = std::tuple<typename CppArg<P>::HolderType...>;

template<typename... P>
struct CppArgTupleInput;

template<>
struct CppArgTupleInput<>
{
    template<typename... T>
    static void get(const v8::FunctionCallbackInfo<v8::Value> &, int index, std::tuple<T...> &) {}
};

template<typename P0, typename... P>
struct CppArgTupleInput<P0, P...>
{
    template<typename... T>
    static void get(const v8::FunctionCallbackInfo<v8::Value> &args, int index, std::tuple<T...> &t)
    {
        CppArg<P0>::get(args[index], std::get<sizeof...(T) - sizeof...(P) - 1>(t));
        CppArgTupleInput<P...>::get(args, index + 1, t);
    }
};