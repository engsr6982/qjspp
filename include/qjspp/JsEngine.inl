#pragma once
#include "JsEngine.hpp"
#include "qjspp/Binding.hpp"
#include "qjspp/Values.hpp"

namespace qjspp {


template <typename T>
std::shared_ptr<T> JsEngine::getData() const {
    return std::static_pointer_cast<T>(userData_);
}


template <typename T>
Object JsEngine::newInstanceOfRaw(ClassDefine const& def, T* instance) {
    return newInstance(def, std::move(def.wrap(instance)));
}

template <typename T>
Object JsEngine::newInstanceOfView(ClassDefine const& def, T* instance) {
    auto wrap = WrappedResource::make(instance, [](void* res) -> void* { return res; }, nullptr);
    return newInstance(def, std::move(wrap));
}

template <typename T>
Object JsEngine::newInstanceOfView(ClassDefine const& def, T* instance, Object ownerJs) {
    struct Control {
        Object ownerJsInst;
        void*  nativeInst{nullptr};
        explicit Control(Object ownerJs, void* instance) : ownerJsInst(std::move(ownerJs)), nativeInst(instance) {}
        ~Control() { ownerJsInst.reset(); }
    };
    auto control = new Control{ownerJs, instance};

    auto wrap = WrappedResource::make(
        control,
        [](void* res) -> void* { return static_cast<Control*>(res)->nativeInst; },
        [](void* res) -> void { delete static_cast<Control*>(res); }
    );
    return newInstance(def, std::move(wrap));
}

template <typename T>
Object JsEngine::newInstanceOfUnique(ClassDefine const& def, std::unique_ptr<T>&& instance) {
    return newInstanceOfRaw<T>(def, instance.release());
}

template <typename T>
Object JsEngine::newInstanceOfShared(ClassDefine const& def, std::shared_ptr<T>&& instance) {
    struct Control {
        std::shared_ptr<T> instance;
        explicit Control(std::shared_ptr<T>&& instance) : instance(std::move(instance)) {}
    };
    auto control = new Control{std::move(instance)};
    auto wrap    = WrappedResource::make(
        control,
        [](void* res) -> void* { return static_cast<Control*>(res)->instance.get(); },
        [](void* res) -> void { delete static_cast<Control*>(res); }
    );
    return newInstance(def, std::move(wrap));
}

template <typename T>
Object JsEngine::newInstanceOfWeak(ClassDefine const& def, std::weak_ptr<T>&& instance) {
    struct Control {
        std::weak_ptr<T> instance;
        explicit Control(std::weak_ptr<T>&& instance) : instance(std::move(instance)) {}
    };
    auto control = new Control{std::move(instance)};
    auto wrap    = WrappedResource::make(
        control,
        [](void* res) -> void* {
            // TODO: 临时 shared_ptr 裸指针不安全，需要持久化或在 wrapper 后清理
            return static_cast<Control*>(res)->instance.lock().get();
        },
        [](void* res) -> void { delete static_cast<Control*>(res); }
    );
    return newInstance(def, wrap);
}

template <typename T>
T* JsEngine::getNativeInstanceOf(Object const& obj, ClassDefine const& def) const {
    return static_cast<T*>(this->getNativeInstanceOf(obj, def));
}


} // namespace qjspp