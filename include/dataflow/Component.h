#pragma once

namespace dataflow {

/**
 * @brief Abstract interface for a dataflow component
 */
class Component {
public:
    virtual ~Component() = default;

    /**
     * @brief Perform an action defined by the implementation (see @c ComponentAdaptor for details)
     */
    virtual void tick() = 0;
};

} // namespace dataflow
