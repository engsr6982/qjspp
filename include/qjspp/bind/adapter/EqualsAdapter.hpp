#pragma once
#include "qjspp/bind/meta/MemberDefine.hpp"
#include "qjspp/concepts/BasicConcepts.hpp"

namespace qjspp::bind::adapter {

template <typename C>
meta::InstanceMemberDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::false_type) {
    return [](void* lhs, void* rhs) -> bool { return lhs == rhs; };
}
template <typename C>
meta::InstanceMemberDefine::InstanceEqualsCallback bindInstanceEqualsImpl(std::true_type) {
    return [](void* lhs, void* rhs) -> bool {
        if (!lhs || !rhs) return false;
        return *static_cast<C*>(lhs) == *static_cast<C*>(rhs);
    };
}
template <typename C>
meta::InstanceMemberDefine::InstanceEqualsCallback bindInstanceEquals() {
    // use tag dispatch to fix MSVC pre name lookup or overload resolution
    return bindInstanceEqualsImpl<C>(std::bool_constant<concepts::HasEquality<C>>{});
}


} // namespace qjspp::bind::adapter