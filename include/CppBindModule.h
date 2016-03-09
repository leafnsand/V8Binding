#pragma once

#include <v8.h>

class CppBindModule
{
    friend class CppBindClassBase;

    template<typename T, typename P>
    friend class CppBindClass;

private:
    v8::Local<v8::Object> handle;

    explicit CppBindModule(v8::Local<v8::Object> handle) : handle(handle) {}

    CppBindModule(const CppBindModule &that) = delete;

    CppBindModule(CppBindModule &&that) = delete;

    CppBindModule &operator=(const CppBindModule &that) = delete;

    CppBindModule &operator=(CppBindModule &&that) = delete;

public:
    static CppBindModule bind(v8::Local<v8::Object> global)
    {
        return CppBindModule(global);
    }

    CppBindModule beginModule(const char *name)
    {
        v8::HandleScope scope(v8::Isolate::GetCurrent());
        v8::Local<v8::Object> moduleHandle;
        auto key = V8Type<const char *>::set(name);
        if (handle->HasOwnProperty(key))
        {
            moduleHandle = handle->Get(key);
        }
        else
        {
            moduleHandle = v8::Object::New(v8::Isolate::GetCurrent());
            moduleHandle->Set(V8Type<const char *>::set("___parent"), handle);
            handle->Set(key, moduleHandle);
        }
        return CppBindModule(moduleHandle);
    }

    CppBindModule endModule()
    {
        return CppBindModule(handle->Get(V8Type<const char *>::set("___parent")));
    }

    template<typename V>
    CppBindModule &addConstant(const char *name, const V &v)
    {
        handle->Set(V8Type<const char *>::set(name), V8Type<V>::set(v), v8::ReadOnly);
        V8Ref r = V8Ref::fromValue(state(), v);
        if (r.isFunction())
        {
            r = V8Ref::createFunctionWith(state(), &CppBindConstant::call, r);
        }
        setGetter(name, r);
        setReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a non-const variable.
     * The value return to V8 is pass-by-value, that will create a local copy in V8.
     * This is different from addVariableRef, which is pass-by-reference, and allow direct access to the variable.
     * This apply only to the class type, the primitive types are always pass-by-value.
     */
    template<typename V>
    CppBindModule &addVariable(const char *name, V *v, bool writable = true)
    {
        setGetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableGetter<V>::call, v));
        if (writable)
        {
            setSetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableSetter<V>::call, v));
        }
        else
        {
            setReadOnly(name);
        }
        return *this;
    }

    /**
     * Add or replace a const read-only variable.
     * The value return to V8 is pass-by-value, that will create a local copy in V8.
     * This is different from addVariableRef, which is pass-by-reference, and allow direct access to the variable.
     * This apply only to the class type, the primitive types are always pass-by-value.
     */
    template<typename V>
    CppBindModule &addVariable(const char *name, const V *v)
    {
        setGetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableGetter<V>::call, v));
        setReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a non-const variable.
     * The value return to V8 is pass-by-reference, and allow direct access to the variable.
     * This is different from addVariable, which is pass-by-value, and will create a local copy upon access.
     * This apply only to the class type, the primitive types are always pass-by-value.
     */
    template<typename V>
    typename std::enable_if<std::is_copy_assignable<V>::value, CppBindModule &>::type
    addVariableRef(const char *name, V *v, bool writable = true)
    {
        setGetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableGetter<V, V &>::call, v));
        if (writable)
        {
            setSetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableSetter<V>::call, v));
        }
        else
        {
            setReadOnly(name);
        }
        return *this;
    }

    /**
     * Add or replace a non-const variable.
     * The value return to V8 is pass-by-reference, and allow direct access to the variable.
     * This is different from addVariable, which is pass-by-value, and will create a local copy upon access.
     * This apply only to the class type, the primitive types are always pass-by-value.
     */
    template<typename V>
    typename std::enable_if<!std::is_copy_assignable<V>::value, CppBindModule &>::type
    addVariableRef(const char *name, V *v)
    {
        setGetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableGetter<V, V &>::call, v));
        setReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a const read-only variable.
     * The value return to V8 is pass-by-reference, and allow direct access to the variable.
     * This is different from addVariable, which is pass-by-value, and will create a local copy upon access.
     * This apply only to the class type, the primitive types are always pass-by-value.
     */
    template<typename V>
    CppBindModule &addVariableRef(const char *name, const V *v)
    {
        setGetter(name, V8Ref::createFunctionWithPtr(state(), &CppBindVariableGetter<V, const V &>::call, v));
        setReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a read-write property.
     */
    template<typename FG, typename FS>
    CppBindModule &addProperty(const char *name, const FG &get, const FS &set)
    {
        using CppGetter = CppBindMethod<FG, FG, 1, CHK_GETTER>;
        using CppSetter = CppBindMethod<FS, FS, 1, CHK_SETTER>;
        setGetter(name, V8Ref::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setSetter(name, V8Ref::createFunction(state(), &CppSetter::call, CppSetter::function(set)));
        return *this;
    }

    /**
     * Add or replace a read-only property.
     */
    template<typename FN>
    CppBindModule &addProperty(const char *name, const FN &get)
    {
        using CppGetter = CppBindMethod<FN, FN, 1, CHK_GETTER>;
        setGetter(name, V8Ref::createFunction(state(), &CppGetter::call, CppGetter::function(get)));
        setReadOnly(name);
        return *this;
    }

    /**
     * Add or replace a function.
     */
    template<typename FN>
    CppBindModule &addFunction(const char *name, const FN &proc)
    {
        using CppProc = CppBindMethod<FN>;
        m_meta.rawset(name, V8Ref::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a function, user can specify augument spec.
     */
    template<typename FN, typename ARGS>
    CppBindModule &addFunction(const char *name, const FN &proc, ARGS)
    {
        using CppProc = CppBindMethod<FN, ARGS>;
        m_meta.rawset(name, V8Ref::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a factory function.
     */
    template<typename FN>
    CppBindModule &addFactory(const FN &proc)
    {
        using CppProc = CppBindMethod<FN, FN, 2>;
        m_meta.rawset("__call", V8Ref::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a factory function, user can specify augument spec.
     */
    template<typename FN, typename ARGS>
    CppBindModule &addFactory(const FN &proc, ARGS)
    {
        using CppProc = CppBindMethod<FN, ARGS, 2>;
        m_meta.rawset("__call", V8Ref::createFunction(state(), &CppProc::call, CppProc::function(proc)));
        return *this;
    }

    /**
     * Add or replace a factory function, that forward call to sub module factory (or class constructor).
     */
    CppBindModule &addFactory(const char *name)
    {
        m_meta.rawset("__call", V8Ref::createFunctionWith(state(), &CppBindModuleMetaMethod::forwardCall, name));
        return *this;
    }

    /**
     * Open a new or existing class for registrations.
     */
    template<typename T>
    CppBindClass<T, CppBindModule> beginClass(const char *name)
    {
        return CppBindClass<T, CppBindModule>::bind(m_meta, name);
    }

    /**
     * Open a new class to extend the base class.
     */
    template<typename T, typename SUPER>
    CppBindClass<T, CppBindModule> beginExtendClass(const char *name)
    {
        return CppBindClass<T, CppBindModule>::template extend<SUPER>(m_meta, name);
    }
};

inline CppBindModule V8Binding(v8::Local<v8::Object> global)
{
    return CppBindModule::bind(global);
}
