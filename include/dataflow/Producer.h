#pragma once

#include "utils/synchronized_multi_queue.h"

namespace dataflow {
namespace detail {

struct ProducerTag {};

} // namespace detail

/**
 * @brief A mixin for a component that has outputs
 *
 * @note Has different interfaces depending on the number of outputs
 */
template<typename... Outs>
class Producer;

/**
 * @brief A mixin for a component that has a single output
 *
 * @tparam Out The type of the output
 */
template<typename Out>
class Producer<Out> : public detail::ProducerTag {
public:
    /**
     * @brief Get this component's output
    */
    SynchronizationMultiQueue<Out>::View getOutputPipe() const {
        return m_outputPipe->view();
    }

protected:
    Producer() : m_outputPipe{SynchronizationMultiQueue<Out>::make()} {}

    /**
     * @brief Push @p value to the output pipe
     */
    template<typename T>
    void pushOutput(T&& value) {
        m_outputPipe->push(std::forward<T>(value));
    }

    /**
     * @brief Push @p value to the output pipe if is isn't null
     */
    void pushOutput(std::optional<Out> value) {
        if (value) {
            this->pushOutput(*std::move(value));
        }
    }

private:
    std::shared_ptr<SynchronizationMultiQueue<Out>> m_outputPipe;
};

/**
 * @brief A mixin for a component that has a fixed number of outputs
 *
 * @tparam Outs The types of the outputs
 */
template<typename... Outs>
class Producer : public detail::ProducerTag {
public:
    Producer() : m_outputPipes{SynchronizationMultiQueue<Outs>::make()...} {}

    /**
     * @brief Get this component's output
    */
    template<auto idx>
    auto getOutputPipe() const {
        return getOutputPipeSource<idx>().view();
    }

protected:
    /**
     * @brief Push each of the @p values to the respective output pipes
     */
    template<typename T>
    void pushOutput(T&& values) {
        pushOutputs(std::make_index_sequence<std::tuple_size_v<T>>{}, std::forward<T>(values));
    }

private:
    template<auto... indices, typename T>
    void pushOutputs(std::index_sequence<indices...> seq, T&& values) {
        (pushOutput<indices>(std::get<indices>(std::forward<T>(values))), ...);
    }

    template<auto idx, typename T>
    void pushOutput(T&& value) {
        getOutputPipeSource<idx>().push(std::forward<T>(value));
    }

    template<auto idx>
    void pushOutput(std::optional<std::tuple_element_t<idx, std::tuple<Outs...>>> value) {
        if (value) {
            this->pushOutput<idx>(*std::move(value));
        }
    }

    template<auto idx>
    auto& getOutputPipeSource() const {
        return *std::get<idx>(m_outputPipes);
    }

private:
    std::tuple<std::shared_ptr<SynchronizationMultiQueue<Outs>>...> m_outputPipes;
};

/**
 * @brief A mixin for a component that has a dynamic number of outputs
 *
 * @tparam Out The type of the outputs
 */
template<typename Out>
class Producer<Out[]> : public detail::ProducerTag {
public:
    /**
     * @brief Get this component's output
    */
    SynchronizationMultiQueue<Out>::View getOutputPipe(int idx) const {
        return getOutputPipeSource(idx).view();
    }

protected:
    /**
     * @brief Push each of the @p values to the respective output pipe
     */
    template<typename T>
    void pushOutput(const std::vector<T>& values) {
        for (auto i = 0; i != values.size(); ++i) {
            pushOutput(i, values[i]);
        }
    }

    /**
     * @brief Push each of the @p values to the respective output pipe
     */
    template<typename T>
    void pushOutput(std::vector<T>&& values) {
        for (auto i = 0; i != values.size(); ++i) {
            pushOutput(i, std::move(values[i]));
        }
    }

private:
    template<typename T>
    void pushOutput(int idx, T&& value) {
        getOutputPipeSource(idx).push(std::forward<T>(value));
    }

    void pushOutput(int idx, std::optional<Out> value) {
        if (value) {
            this->pushOutput(idx, *std::move(value));
        }
    }

    SynchronizationMultiQueue<Out>& getOutputPipeSource(int idx) const {
        for (auto i = m_outputPipes.size(); i <= idx; ++i) {
            m_outputPipes.push_back(SynchronizationMultiQueue<Out>::make());
        }

        return *m_outputPipes[idx];
    }

private:
    mutable std::vector<std::shared_ptr<SynchronizationMultiQueue<Out>>> m_outputPipes;
};

} // namespace dataflow
