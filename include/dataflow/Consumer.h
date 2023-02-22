#pragma once

#include "utils/synchronized_multi_queue.h"

namespace dataflow {
namespace detail {

struct ConsumerTag {};

} // namespace detail

/**
 * @brief A mixin for a component that has inputs
 *
 * @note Has different interfaces depending on the number of inputs
 */
template<typename... Ins>
class Consumer;

/**
 * @brief A mixin for a component that has a single input
 *
 * @tparam In The type of the input
 */
template<typename In>
class Consumer<In> : public detail::ConsumerTag {
public:
    /**
     * @brief Connect the input to another component's output
     */
    void setInputPipe(SynchronizationMultiQueue<In>::View inputPipe) {
        m_inputPipe = std::move(inputPipe);
    }

protected:
    /**
     * @brief Attempt to get the input from the pipe
     *
     * @note Doesn't wait for the input in case there's none
     *
     * @return Input from the pipe if there is any, null otherwise
     */
    std::optional<In> pullInput() {
        if (m_inputPipe.size() > 0) {
            return m_inputPipe.pop();
        }
        else {
            return std::nullopt;
        }
    }

private:
    SynchronizationMultiQueue<In>::View m_inputPipe;
};

/**
 * @brief A mixin for a component that has a fixed number of inputs
 *
 * @tparam Ins The types of the inputs
 */
template<typename... Ins>
class Consumer : public detail::ConsumerTag {
public:
    /**
     * @brief connect the inputs to other components' outputs
     */
    void setInputPipes(SynchronizationMultiQueue<Ins>::View... inputPipes) {
        m_inputPipes = std::tuple{std::move(inputPipes)...};
    }

protected:
    /**
     * @brief Attempt to get the inputs from the pipes
     *
     * @note Doesn't wait for the input in case there's none
     * @note Returns nothing if any of the pipes are empty
     *
     * @return Input from the pipes if all are present, null otherwise
     */
    std::optional<std::tuple<Ins...>> pullInput() {
        if (allInputsReady()) {
            return getInputs();
        }
        else {
            return std::nullopt;
        }
    }

private:
    template<std::size_t... indices>
    bool allInputsReady(std::index_sequence<indices...>) const {
        return (... && (std::get<indices>(m_inputPipes).size() > 0));
    }

    bool allInputsReady() const {
        return allInputsReady(std::index_sequence_for<Ins...>{});
    }

    template<std::size_t... indices>
    std::tuple<Ins...> getInputs(std::index_sequence<indices...>) {
        return {std::get<indices>(m_inputPipes).pop()...};
    }

    std::tuple<Ins...> getInputs() {
        return getInputs(std::index_sequence_for<Ins...>{});
    }

private:
    std::tuple<typename SynchronizationMultiQueue<Ins>::View...> m_inputPipes;
};

/**
 * @brief A mixin for a component that has a dynamic number of inputs of the same type
 *
 * @tparam In The type of the inputs
 */
template<typename In>
class Consumer<In[]> : public detail::ConsumerTag {
public:
    /**
     * @brief Add another component's output to the input bus
     */
    void addInputPipe(SynchronizationMultiQueue<In>::View inputPipe) {
        m_inputPipes.push_back(std::move(inputPipe));
    }

protected:
    /**
     * @brief Attempt to get the inputs from the pipes
     *
     * @note Doesn't wait for the input in case there's none
     * @note Returns nothing if any of the pipes are empty
     *
     * @return Input from the pipes if all are present, null otherwise
     */
    std::optional<std::vector<In>> pullInput() {
        for (auto i = m_inputs.size(); i != m_inputPipes.size(); ++i) {
            auto& pipe = m_inputPipes[i];

            if (pipe.size() > 0) {
                m_inputs.push_back(pipe.pop());
            }
            else {
                return std::nullopt;
            }
        }

        return std::exchange(m_inputs, {});
    }

private:
    std::vector<typename SynchronizationMultiQueue<In>::View> m_inputPipes;
    std::vector<In> m_inputs;
};

} // namespace dataflow
