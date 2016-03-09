#pragma once

#include <v8.h>

#include <cassert>
#include <type_traits>
#include <string>
#include <vector>
#include <map>
#include <tuple>

template<typename T, typename ENABLED = void>
struct V8TypeMapping;

template<typename T, bool IS_CONST, bool IS_REF>
struct V8ClassMapping;

struct V8TypeExists
{
    template<typename T, typename = decltype(T())>
    static std::true_type test(int);

    template<typename>
    static std::false_type test(...);
};

template<typename T>
struct V8TypeMappingExists
{
    using Type = decltype(V8TypeExists::test<V8TypeMapping<T>>(0));
    static constexpr bool value = Type::value;
};

template<typename T>
struct V8Type
    : std::conditional<
        std::is_class<typename std::decay<T>::type>::value
        && !V8TypeMappingExists<typename std::decay<T>::type>::value,
        V8ClassMapping<typename std::decay<T>::type,
            std::is_const<typename std::remove_reference<T>::type>::value,
            std::is_reference<T>::value>,
        V8TypeMapping<typename std::decay<T>::type>
    >::type {};

template<>
struct V8TypeMapping<bool>
{
    static v8::Local<v8::Boolean> set(bool value)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::Boolean::New(v8::Isolate::GetCurrent(), value));
    }

    static bool get(v8::MaybeLocal<v8::Value> handle)
    {
        return handle.ToLocalChecked()->ToBoolean()->Value();
    }

    static bool opt(v8::MaybeLocal<v8::Value> handle, bool def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : handle.ToLocalChecked()->ToBoolean()->Value();
    }
};

template<typename T>
struct V8Int32TypeMapping
{
    static v8::Local<v8::Int32> set(T value)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::Int32::New(v8::Isolate::GetCurrent(), value));
    }

    static T get(v8::MaybeLocal<v8::Value> handle)
    {
        return static_cast<T>(handle.ToLocalChecked()->ToInt32()->Value());
    }

    static T opt(v8::MaybeLocal<v8::Value> handle, T def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : static_cast<T>(handle.ToLocalChecked()->ToInt32()->Value());
    }
};

template<>
struct V8TypeMapping<signed char>
    : V8Int32TypeMapping<signed char> {};

template<>
struct V8TypeMapping<short>
    : V8Int32TypeMapping<short> {};

template<>
struct V8TypeMapping<int>
    : V8Int32TypeMapping<int> {};

template<>
struct V8TypeMapping<long>
    : V8Int32TypeMapping<long> {};

template<typename T>
struct V8Uint32TypeMapping
{
    static v8::Local<v8::Uint32> set(T value)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::Uint32::New(v8::Isolate::GetCurrent(), value));
    }

    static T get(v8::MaybeLocal<v8::Value> handle)
    {
        return static_cast<T>(handle.ToLocalChecked()->ToUint32()->Value());
    }

    static T opt(v8::MaybeLocal<v8::Value> handle, T def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : static_cast<T>(handle.ToLocalChecked()->ToUint32()->Value());
    }
};

template<>
struct V8TypeMapping<unsigned char>
    : V8Uint32TypeMapping<unsigned char> {};

template<>
struct V8TypeMapping<unsigned short>
    : V8Uint32TypeMapping<unsigned short> {};

template<>
struct V8TypeMapping<unsigned int>
    : V8Uint32TypeMapping<unsigned int> {};

template<>
struct V8TypeMapping<unsigned long>
    : V8Uint32TypeMapping<unsigned long> {};

template<typename T>
struct V8IntegerTypeMapping
{
    static v8::Local<v8::Integer> set(T value)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::Integer::New(v8::Isolate::GetCurrent(), value));
    }

    static T get(v8::MaybeLocal<v8::Value> handle)
    {
        return static_cast<T>(handle.ToLocalChecked()->ToInteger()->Value());
    }

    static T opt(v8::MaybeLocal<v8::Value> handle, T def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : static_cast<T>(handle.ToLocalChecked()->ToInteger()->Value());
    }
};

template<>
struct V8TypeMapping<long long>
    : V8IntegerTypeMapping<long long> {};

template<typename T>
using V8UnsignedIntegerTypeMapping = V8IntegerTypeMapping<T>;

template<>
struct V8TypeMapping<unsigned long long>
    : V8UnsignedIntegerTypeMapping<unsigned long long> {};

template<typename T>
struct V8NumberTypeMapping
{
    static v8::Local<v8::Number> set(T value)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::Number::New(v8::Isolate::GetCurrent(), value));
    }

    static T get(v8::MaybeLocal<v8::Value> handle)
    {
        return static_cast<T>(handle.ToLocalChecked()->ToNumber()->Value());
    }

    static T opt(v8::MaybeLocal<v8::Value> handle, T def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : static_cast<T>(handle.ToLocalChecked()->ToNumber()->Value());
    }
};

template<>
struct V8TypeMapping<float>
    : V8NumberTypeMapping<float> {};

template<>
struct V8TypeMapping<double>
    : V8NumberTypeMapping<double> {};

template<>
struct V8TypeMapping<char>
{
    static v8::Local<v8::String> set(char value)
    {
        char str[] = { value, 0 };
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str));
    }

    static char get(v8::MaybeLocal<v8::Value> handle)
    {
        return (*v8::String::Utf8Value(handle.ToLocalChecked()->ToString()))[0];
    }

    static char opt(v8::MaybeLocal<v8::Value> handle, char def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : (*v8::String::Utf8Value(handle.ToLocalChecked()->ToString()))[0];
    }
};

template<>
struct V8TypeMapping<const char *>
{
    static v8::Local<v8::String> set(const char *str)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        if (str == nullptr)
        {
            return scope.Escape(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "<string from nullptr>"));
        }
        else
        {
            return scope.Escape(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str));
        }
    }

    static const char *get(v8::MaybeLocal<v8::Value> handle)
    {
        return *v8::String::Utf8Value(handle.ToLocalChecked()->ToString());
    }

    static const char *opt(v8::MaybeLocal<v8::Value> handle, const char *def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : *v8::String::Utf8Value(handle.ToLocalChecked()->ToString());
    }
};

template<>
struct V8TypeMapping<char *>
    : V8TypeMapping<const char *> {};

template<>
struct V8TypeMapping<std::string>
{
    static v8::Local<v8::String> set(const std::string &str)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        return scope.Escape(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str.data()));
    }

    static std::string get(v8::MaybeLocal<v8::Value> handle)
    {
        return std::string(*v8::String::Utf8Value(handle.ToLocalChecked()->ToString()));
    }

    static std::string opt(v8::MaybeLocal<v8::Value> handle, const std::string& def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : std::string(*v8::String::Utf8Value(handle.ToLocalChecked()->ToString()));
    }
};

template<typename T>
struct V8TypeMapping<std::vector<T>>
{
    static v8::Local<v8::Array> set(const std::vector<T> &vector)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        auto array = v8::Array::New(v8::Isolate::GetCurrent());
        for (auto i = 0; i < vector.size(); ++i)
        {
            array->Set(static_cast<uint32_t>(i), V8Type<T>::set(vector[i]));
        }
        return scope.Escape(array);
    }

    static std::vector<T> get(v8::MaybeLocal<v8::Value> handle)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        std::vector<T> vector;
        auto array = handle.ToLocalChecked().As<v8::Array>();
        for (auto i = 0; i < array->Length(); ++i)
        {
            vector.push_back(V8Type<T>::get(handle));
        }
        return vector;
    }

    static std::vector<T> opt(v8::MaybeLocal<v8::Value> handle, const std::vector<T> &def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : get(handle);
    }
};

template<typename K, typename V>
struct V8TypeMapping<std::map<K, V>>
{
    static v8::Local<v8::Object> set(const std::map<K, V> &map)
    {
        v8::EscapableHandleScope scope(v8::Isolate::GetCurrent());
        auto object = v8::Object::New(v8::Isolate::GetCurrent());
        for (auto &pair : map)
        {
            object->Set(V8Type<K>::set(pair.first), V8Type<V>::set(pair.second));
        }
        return scope.Escape(object);
    }

    static std::map<K, V> get(v8::MaybeLocal<v8::Value> handle)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        std::map<K, V> map;
        auto object = handle.ToLocalChecked().As<v8::Object>();
        auto properties = object->GetPropertyNames();
        for (auto i = 0; i < properties->Length(); ++i)
        {
            auto key = properties->Get(static_cast<uint32_t>(i));
            auto value = object->Get(key);
            map.insert(V8Type<K>::get(key), V8Type<V>::get(value));
        }
        return map;
    }

    static std::map<K, V> opt(v8::MaybeLocal<v8::Value> handle, const std::map<K, V> &def)
    {
        return handle.ToLocalChecked()->IsUndefined() ? def : get(handle);
    }
};