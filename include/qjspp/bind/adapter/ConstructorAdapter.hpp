#pragma once
#include "AdaptHelper.hpp"
#include "qjspp/Forward.hpp"
#include "qjspp/concepts/BasicConcepts.hpp"
#include "qjspp/types/Arguments.hpp"

namespace qjspp::bind::adapter {


template <typename C, typename... Args>
InstanceConstructor bindInstanceConstructor() {
    return [](Arguments const& args) -> void* {
        constexpr size_t N = sizeof...(Args);
        if constexpr (N == 0) {
            static_assert(
                concepts::HasDefaultConstructor<C>,
                "Class C must have a no-argument constructor; otherwise, a constructor must be specified."
            );
            if (args.length() != 0) return nullptr; // Parameter mismatch
            return new C();

        } else {
            if (args.length() != N) return nullptr; // Parameter mismatch

            using Tuple = std::tuple<Args...>;

            auto parameters = ConvertArgsToTuple<Tuple>(args, std::make_index_sequence<N>());
            return std::apply(
                [](auto&&... unpackedArgs) { return new C(std::forward<decltype(unpackedArgs)>(unpackedArgs)...); },
                std::move(parameters)
            );
        }
    };
}


} // namespace qjspp::bind::adapter