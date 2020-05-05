#pragma once

// Implements IUnknown interface of an inherited base class
//
// class MyClass : IUnknownBase<BaseClass> { ...

template <class T>
class IUnknownBase : public T
{
private:
    long mnRefCount = 1;

protected:
    // ensure subclass destructor is called from Release()
    virtual ~IUnknownBase() = default;

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override
    {
        if (!ppv) {
            return E_POINTER;
        }
        if (iid == IID_IUnknown) {
            *ppv = static_cast<IUnknown*>(this);
        } else if (iid == __uuidof(T)) {
            *ppv = reinterpret_cast<T*>(this);
        } else {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&mnRefCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG uCount = InterlockedDecrement(&mnRefCount);
        if (uCount == 0) {
            delete this;
        }
        // For thread safety, return a temporary variable.
        return uCount;
    };
};
