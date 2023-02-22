#pragma once

#include "dataflow/Component.h"
#include "dataflow/ComponentFactory.h"

#include <memory>
#include <vector>

namespace dataflow {

/**
 * @brief A collection of @c Component instances
 */
class Circuit final : public Component {
public:
    /**
     * @brief Recursively tick all components in order
     */
    void tick() override {
        for (auto& component : m_components) {
            component->tick();
        }
    }

    /**
     * @brief Add a new component to the circuit
     *
     * @tparam Traits Either `Producer<T...>`, `Consumer<T...>` or both, depending on the @p impl call signature
     * @tparam Impl A callable type that implements the behavior of the component
     *
     * @param impl The implementation that will be called during ticks
     *
     * @return Reference to an adaptor class that is derived from @tparam Traits
     */
    template<typename... Traits, typename Impl>
    auto& add(Impl&& impl) {
        auto component = makeComponent<Traits...>(std::forward<Impl>(impl));
        auto& ref = *component;
        m_components.push_back(std::move(component));
        return ref;
    }

private:
    std::vector<std::unique_ptr<Component>> m_components;
};

} // namespace dataflow
