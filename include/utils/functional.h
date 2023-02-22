#pragma once

#include <functional>

template<typename F, typename T>
decltype(auto)
invokeOrApply(F& f, T&& arg) {
    if constexpr (std::is_invocable_v<F, T>) {
        return f(std::forward<T>(arg));
    }
    else {
        return std::apply(f, std::forward<T>(arg));
    }
}
