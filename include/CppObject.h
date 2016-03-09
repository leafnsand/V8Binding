#pragma once

#include "CppInvoke.h"

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <tuple>

#include <v8.h>

template<typename T>
struct CppClassPersistent
{
    static v8::UniquePersistent<v8::FunctionTemplate> persistent;
};

class CppObject
{
protected:
    CppObject() {}

    template<typename T>
    static T *allocate(v8::Local<v8::Object> self)
    {
        assert(self->InternalFieldCount() == 1);
        auto instance = new T;
        self->SetAlignedPointerInInternalField(0, instance);
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(sizeof(T)));
        persistent.Reset(v8::Isolate::GetCurrent(), self);
        persistent.SetWeak(instance, &deallocate, v8::WeakCallbackType::kParameter);
        return instance;
    }

    template<typename T>
    static void deallocate(const v8::WeakCallbackInfo<T> &data)
    {
        delete data.GetParameter();
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(-sizeof(T)));
        persistent.Reset();
    }

public:
    virtual ~CppObject() {}

    CppObject(const CppObject &) = delete;

    CppObject &operator=(const CppObject &) = delete;

    virtual bool isSharedPtr() const
    {
        return false;
    }

    virtual void *objectPtr() = 0;

    template<typename T>
    static CppObject *getExactObject(v8::Local<v8::Object> self)
    {
        return getObject<T>(self, true, true);
    }

    template<typename T>
    static CppObject *getObject(v8::Local<v8::Object> self)
    {
        return getObject<T>(self, false, true);
    }

    template<typename T>
    static T *cast(v8::Local<v8::Object> self)
    {
        CppObject *object = getObject<T>(self, false, false);
        return object ? static_cast<T *>(object->objectPtr()) : nullptr;
    }

    template<typename T>
    static T *get(v8::Local<v8::Object> self)
    {
        return static_cast<T *>(getObject<T>(self)->objectPtr());
    }

private:
    template<typename T>
    static CppObject *getObject(v8::Local<v8::Object> self, bool is_exact, bool raise_error)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        if (self == v8::Undefined(v8::Isolate::GetCurrent()) || self->InternalFieldCount() <= 0)
        {
            if (raise_error)
            {
                v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "except cpp class, but empty")));
            }
            return nullptr;
        }
        auto pointer = self->GetAlignedPointerFromInternalField(0);
        auto object = static_cast<CppObject *>(pointer);
        if (object == nullptr)
        {
            if (raise_error)
            {
                v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "except cpp class, but got NULL")));
            }
            return nullptr;
        }
        auto &persistent = CppClassPersistent<T>::persistent;
        auto prototype = self->GetPrototype();
        while (true)
        {
            if (prototype == persistent)
            {
                break;
            }
            if (is_exact)
            {
                if (raise_error)
                {
                    v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "except cpp class, but wrong type")));
                }
                return nullptr;
            }
            if (prototype->IsObject())
            {
                prototype = prototype->ToObject()->GetPrototype();
            }
            else
            {
                if (raise_error)
                {
                    v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "except cpp class, but wrong type")));
                }
                return nullptr;
            }
        }
        return object;
    }

    static v8::Persistent<v8::Object> persistent;
};

template<typename T>
class CppObjectValue : public CppObject
{
private:
    CppObjectValue()
    {
        if (MAX_PADDING > 0)
        {
            uintptr_t offset = reinterpret_cast<uintptr_t>(&data[0]) % alignof(T);
            if (offset > 0)
            {
                offset = alignof(T) - offset;
            }
            assert(offset < MAX_PADDING);
            data[sizeof(data) - 1] = static_cast<unsigned char>(offset);
        }
    }

public:
    virtual ~CppObjectValue()
    {
        T *obj = static_cast<T *>(objectPtr());
        obj->~T();
    }

    virtual void *objectPtr() override
    {
        if (MAX_PADDING == 0)
        {
            return &data[0];
        }
        else
        {
            return &data[0] + data[sizeof(data) - 1];
        }
    }

    template<typename... P>
    static void instance(v8::Local<v8::Object> self, P &&... args)
    {
        auto instance = allocate<CppObjectValue<T>>(self);
        ::new(instance->objectPtr()) T(std::forward<P>(args)...);
    }

    template<typename... P>
    static void instance(v8::Local<v8::Object> self, std::tuple<P...> &args)
    {
        auto instance = allocate<CppObjectValue<T>>(self);
        CppInvokeClassConstructor<T>::call(instance->objectPtr(), args);
    }

    static void instance(v8::Local<v8::Object> self, const T &obj)
    {
        auto instance = allocate<CppObjectValue<T>>(self);
        ::new(instance->objectPtr()) T(obj);
    }

private:
    using AlignType = typename std::conditional<alignof(T) <= alignof(double), T, void *>::type;
    static constexpr size_t MAX_PADDING = alignof(T) <= alignof(AlignType) ? 0 : alignof(T) - alignof(AlignType) + 1;
    alignas(AlignType) unsigned char data[sizeof(T) + MAX_PADDING];
};

class CppObjectPtr : public CppObject
{
public:
    virtual void *objectPtr() override
    {
        return ptr;
    }

    template<typename T>
    static void instance(v8::Local<v8::Object> self, T *obj)
    {
        auto instance = allocate<CppObjectPtr>(self);
        instance->ptr = obj;
        assert(instance->ptr);
    }

private:
    void *ptr{ nullptr };
};

template<typename SP, typename T>
class CppObjectSharedPtr : public CppObject
{
public:
    virtual bool isSharedPtr() const override
    {
        return true;
    }

    virtual void *objectPtr() override
    {
        return const_cast<T *>(&*sp);
    }

    SP &sharedPtr()
    {
        return sp;
    }

    static void instance(v8::Local<v8::Object> self, T *obj)
    {
        auto instance = allocate<CppObjectSharedPtr<SP, T>>(self);
        instance->sp.reset(obj);
    }

    static void instance(v8::Local<v8::Object> self, const SP &sp)
    {
        auto instance = allocate<CppObjectSharedPtr<SP, T>>(self);
        instance->sp = sp;
    }

private:
    SP sp;
};

//----------------------------------------------------------------------------

template<typename T>
struct CppObjectTraits
{
    using ObjectType = T;

    static constexpr bool isSharedPtr = false;
    static constexpr bool isSharedConst = false;
};

template<typename T>
struct CppObjectTraits<std::shared_ptr<T>>
{
    using ObjectType = typename std::remove_cv<T>::type;

    static constexpr bool isSharedPtr = true;
    static constexpr bool isSharedConst = std::is_const<T>::value;
};

template<typename SP, typename OBJ, bool IS_SHARED, bool IS_REF>
struct V8CppObjectFactory;

template<typename T>
struct V8CppObjectFactory<T, T, false, false>
{
    static void instance(v8::Local<v8::Object> self, const T &obj)
    {
        CppObjectValue<T>::instance(self);
    }

    static T &cast(v8::Local<v8::Object> self, CppObject *obj)
    {
        return *static_cast<T *>(obj->objectPtr());
    }
};

template<typename T>
struct V8CppObjectFactory<T, T, false, true>
{
    static void instance(v8::Local<v8::Object> self, const T &obj)
    {
        CppObjectPtr::instance(self, const_cast<T *>(&obj));
    }

    static T &cast(v8::Local<v8::Object> self, CppObject *obj)
    {
        return *static_cast<T *>(obj->objectPtr());
    }
};

template<typename SP, typename T>
struct V8CppObjectFactory<SP, T, true, true>
{
    static void instance(v8::Local<v8::Object> self, const SP &sp)
    {
        if (sp)
        {
            CppObjectSharedPtr<SP, T>::instance(self, sp);
        }
    }

    static SP &cast(v8::Local<v8::Object> self, CppObject *obj)
    {
        if (!obj->isSharedPtr())
        {
            v8::HandleScope scope(v8::Isolate::GetCurrent());
            v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "is not shared object!")));
        }
        return static_cast<CppObjectSharedPtr<SP, T> *>(obj)->sharedPtr();
    }
};

template<typename T, bool IS_CONST, bool IS_REF>
struct V8ClassMapping
{
    using ObjectType = typename CppObjectTraits<T>::ObjectType;

    static constexpr bool isShared = CppObjectTraits<T>::isSharedPtr;
    static constexpr bool isRef = isShared ? true : IS_REF;
    static constexpr bool isConst = isShared ? CppObjectTraits<T>::isSharedConst : IS_CONST;

    static void instance(v8::Local<v8::Object> self, const T &t)
    {
        V8CppObjectFactory<T, ObjectType, isShared, isRef>::instance(self, t);
    }

    static T &get(v8::Local<v8::Object> self)
    {
        CppObject *obj = CppObject::getObject<ObjectType>(self);
        return V8CppObjectFactory<T, ObjectType, isShared, isRef>::cast(self, obj);
    }

    static const T &opt(v8::Local<v8::Object> self, const T &def)
    {
        return self->IsUndefined() ? def : get(self);
    }
};


template<typename T>
struct V8TypeMapping<T *>
{
    using Type = typename std::decay<T>::type;
    using PtrType = T *;

    static constexpr bool isConst = std::is_const<T>::value;

    static void instance(v8::Local<v8::Object> self, const Type *p)
    {
        if (p)
        {
            CppObjectPtr::instance(self, const_cast<Type *>(p));
        }
    }

    static PtrType get(v8::Local<v8::Object> self)
    {
        return CppObject::get<Type>(self);
    }

    static PtrType opt(v8::Local<v8::Object> self, PtrType def)
    {
        return self->IsUndefined() ? def : get(self);
    }
};