#include "qjspp/Binding.hpp"
#include <utility>


namespace qjspp {


StaticDefine::Property::Property(std::string name, GetterCallback getter, SetterCallback setter)
: name_(std::move(name)),
  getter_(std::move(getter)),
  setter_(std::move(setter)) {}

StaticDefine::Function::Function(std::string name, FunctionCallback callback)
: name_(std::move(name)),
  callback_(std::move(callback)) {}


StaticDefine::StaticDefine(std::vector<Property> property, std::vector<Function> functions)
: property_(std::move(property)),
  functions_(std::move(functions)) {}


InstanceDefine::Property::Property(std::string name, InstanceGetterCallback getter, InstanceSetterCallback setter)
: name_(std::move(name)),
  getter_(std::move(getter)),
  setter_(std::move(setter)) {}

InstanceDefine::Method::Method(std::string name, InstanceMethodCallback callback)
: name_(std::move(name)),
  callback_(std::move(callback)) {}


InstanceDefine::InstanceDefine(
    InstanceConstructor   constructor,
    std::vector<Property> property,
    std::vector<Method>   functions
)
: constructor_(std::move(constructor)),
  property_(std::move(property)),
  methods_(std::move(functions)) {}

WrappedResource::WrappedResource(void* resource, ResGetter getter, ResDeleter deleter)
: resource_(resource),
  getter_(getter),
  deleter_(deleter) {}

WrappedResource::~WrappedResource() {
    if (deleter_ != nullptr) {
        deleter_(resource_);
    }
}


bool ClassDefine::hasInstanceConstructor() const { return instanceDefine_.constructor_ != nullptr; }

ClassDefine::ClassDefine(
    std::string                 name,
    StaticDefine                static_,
    InstanceDefine              instance,
    ClassDefine const*          parent,
    TypedWrappedResourceFactory factory
)
: name_(std::move(name)),
  staticDefine_(std::move(static_)),
  instanceDefine_(std::move(instance)),
  extends_(parent),
  mJsNewInstanceWrapFactory(factory) {}


} // namespace qjspp