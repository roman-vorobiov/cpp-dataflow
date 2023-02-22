#pragma once

#include "dataflow/Component.h"
#include "dataflow/Consumer.h"
#include "dataflow/Producer.h"
#include "dataflow/traits.h"

#include "utils/functional.h"

#include <memory>

namespace dataflow {

/**
 * @brief Adaptor class that implements @c Component
 *
 * @tparam Traits Either `Consumer<T...>`, `Producer<T...>` or both
 * @tparam Impl A callable type that implements the behavior of the component
 */
template<typename Impl, typename... Traits>
class ComponentAdaptor final : public Component, public Traits... {
public:
    explicit ComponentAdaptor(Impl&& impl) : m_impl{std::forward<Impl>(impl)} {}

    /**
     * @brief Call the implementation with the inputs and push the returned value to the outputs
     */
    void tick() override {
        if constexpr (isConsumer<ComponentAdaptor> && isProducer<ComponentAdaptor>) {
            if (auto input = this->pullInput()) {
                this->pushOutput(invokeOrApply(m_impl, *std::move(input)));
            }
        }
        else if constexpr (isConsumer<ComponentAdaptor>) {
            if (auto input = this->pullInput()) {
                invokeOrApply(m_impl, *std::move(input));
            }
        }
        else if constexpr (isProducer<ComponentAdaptor>) {
            this->pushOutput(m_impl());
        }
        else {
            static_assert(std::is_void_v<ComponentAdaptor>, "Invalid traits");
        }
    }

private:
    Impl m_impl;
};

/**
 * @brief create a @c Component from @p impl using @tparam Traits
 */
template<typename... Traits, typename Impl>
auto
makeComponent(Impl&& impl) {
    return std::make_unique<ComponentAdaptor<Impl, Traits...>>(std::forward<Impl>(impl));
}

} // namespace dataflow
