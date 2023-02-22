#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

/// @brief Multiple producers, multiple consumers synchronization queue
template<typename T, typename Impl = std::deque<T>>
class SynchronizationMultiQueue : public std::enable_shared_from_this<SynchronizationMultiQueue<T, Impl>> {
    using Queue = SynchronizationMultiQueue<T, Impl>;

public:
    class View;
    friend class View;

private:
    SynchronizationMultiQueue() = default;

    template<typename... Args>
    explicit SynchronizationMultiQueue(std::in_place_t, Args&&... args) : m_impl(std::forward<Args>(args)...) {}

public:
    /// @brief factory method to force the use of shared_ptr
    template<typename... Args>
    static auto make(Args&&... args) {
        return std::shared_ptr<Queue>{new Queue{std::forward<Args>(args)...}};
    }

    auto begin() const {
        return m_impl.begin();
    }

    auto end() const {
        return m_impl.end();
    }

    std::size_t size() const {
        return m_impl.size();
    }

    /**
     * @brief put the value at the end of the queue
     *
     * @note does nothing if there are no consumers
     */
    template<typename Ty>
    void push(Ty&& value) {
        std::unique_lock l{m_mutex};

        // if there are no consumers, discard stale items
        constexpr auto stalenessThreshold{0};
        if (m_views.empty() && m_impl.size() >= stalenessThreshold) {
            if constexpr (stalenessThreshold == 0) {
                return;
            }
            else {
                m_impl.pop_back();
                m_referenceCounter.pop_back();
            }
        }

        m_impl.push_back(std::forward<Ty>(value));
        m_referenceCounter.push_back(m_views.size());

        l.unlock();
        m_notifier.notify_all();
    }

    /**
     * @brief consumer interface
     *
     * @note the new view will not cover elements that were added before it had been created
     */
    View view();

private:
    /// @brief register a new view (since the view is empty, no reference counting is necessary)
    void registerView(View& view) {
        std::scoped_lock l{m_mutex};
        m_views.insert(&view);
    }

    /// @brief unregister a view, decrementing rererence counts to all elements it covers
    void unregisterView(View& view) {
        std::scoped_lock l{m_mutex};

        auto it = m_views.find(&view);
        if (it == m_views.end()) {
            return;
        }

        while (view.m_idx != m_impl.size()) {
            decreaseReferenceCount(view.m_idx++);
        }

        m_views.erase(it);
    }

    /// @brief optimized combination of unregisterView() + registerView()
    void swapView(View& oldView, View& newView) {
        std::scoped_lock l{m_mutex};

        m_views.insert(&newView);
        m_views.erase(&oldView);
    }

    /**
     * @brief decrement reference counter, removing the item from the queue if it's 0
     *
     * @note adjusts indices in views if removal takes place
     */
    void decreaseReferenceCount(std::size_t idx) {
        if (--m_referenceCounter.at(idx) == 0) {
            // This can only happen when idx == 0
            m_impl.pop_front();
            m_referenceCounter.pop_front();

            for (auto view : m_views) {
                --view->m_idx;
            }
        }
    }

private:
    Impl m_impl;
    std::deque<std::size_t> m_referenceCounter;

    std::mutex m_mutex;
    std::condition_variable m_notifier;

    std::unordered_set<View*> m_views;
};

/**
 * @brief A weak view of SynchronizationMultiQueue. Each view has an independent (begin, end) pair.
 *
 * @note Modifies the underlying data only if it's the sole view.
 *       An item in the queue stays there until there are no views that cover it in their viewed range.
 */
template<typename T, typename Impl>
class SynchronizationMultiQueue<T, Impl>::View {
    using Queue = SynchronizationMultiQueue<T, Impl>;

    friend class SynchronizationMultiQueue<T, Impl>;

private:
    /**
     * @brief private constructor to ensure this class can only be created from the queue
     *
     * @note registers this view in the queue
     * @note initially doesn't cover any elements, even if there are some in the queue
     */
    explicit View(Queue& queue) : m_queue{queue.weak_from_this()}, m_idx{queue.size()} {
        queue.registerView(*this);
    }

public:
    View() = default;

    /**
     * @brief copy constructor
     *
     * @note registers this view in the queue
     * @note covers the same range as @p other
     */
    View(const View& other) : m_queue{other.m_queue}, m_idx{other.m_idx} {
        if (auto queue = m_queue.lock()) {
            queue->registerView(*this);
        }
    }

    /**
     * @brief copy assignment
     *
     * @note changes the range it covers to that of @p other
     */
    View& operator=(const View& other) {
        if (auto queue = m_queue.lock()) {
            queue->unregisterView(*this);
        }

        m_queue = other.m_queue;
        m_idx = other.m_idx;

        if (auto queue = m_queue.lock()) {
            queue->registerView(*this);
        }

        return *this;
    }

    /**
     * @brief move constructor
     *
     * @note registers this view in the queue
     * @note unregisters @p other from the queue
     * @note covers the same range as @p other
     */
    View(View&& other) : m_queue{std::exchange(other.m_queue, {})}, m_idx{other.m_idx} {
        if (auto queue = m_queue.lock()) {
            queue->swapView(other, *this);
        }
    }

    /**
     * @brief move assignment
     *
     * @note unregisters @p other from the queue
     * @note changes the range it covers to that of @p other
     */
    View& operator=(View&& other) {
        if (auto queue = m_queue.lock()) {
            queue->unregisterView(*this);
        }

        m_queue = std::exchange(other.m_queue, {});
        m_idx = other.m_idx;

        if (auto queue = m_queue.lock()) {
            queue->registerView(*this);
        }

        return *this;
    }

    /**
     * @brief destructor
     *
     * @note unregisters this view from the queue
     */
    ~View() {
        if (auto queue = m_queue.lock()) {
            queue->unregisterView(*this);
        }
    }

    /**
     * @brief pointer to the beginning of the range covered by this view
     *
     * @note may differ from the beginning of the queue
     */
    auto begin() const {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        return begin(*queue);
    }

    /**
     * @brief pointer to the end of the range covered by this view
     *
     * @note the same as queue's end
     */
    auto end() const {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        return end(*queue);
    }

    /**
     * @brief number of elements in the range covered by this view
     *
     * @note may differ from the size of the queue
     */
    std::size_t size() const {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        return std::distance(begin(*queue), end(*queue));
    }

    /// @brief reset the range covered by this view to be empty
    void clear() {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        queue->unregisterView(*this);
        queue->registerView(*this);
    }

    /**
     * @brief wait for the queue to not be empty or until timeout
     *
     * @return whether the queue is not empty
     */
    template<typename Rep, typename Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& duration) {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        std::unique_lock l{queue->m_mutex};
        return queue->m_notifier.wait_for(l, duration, [&] { return begin(*queue) != end(*queue); });
    }

    /**
     * @brief take the first element off the queue and return it
     *
     * @note blocks until the queue is not empty
     */
    T pop() {
        auto queue = m_queue.lock();
        if (!queue) {
            throw std::runtime_error{"Attempting to use a dangling synchronization queue view"};
        }

        std::unique_lock l{queue->m_mutex};
        queue->m_notifier.wait(l, [&] { return begin(*queue) != end(*queue); });

        T value = *begin(*queue);
        queue->decreaseReferenceCount(m_idx++);

        return value;
    }

private:
    auto begin(Queue& queue) const {
        return std::next(queue.m_impl.begin(), m_idx);
    }

    auto end(Queue& queue) const {
        return queue.m_impl.end();
    }

private:
    std::weak_ptr<Queue> m_queue;

    std::size_t m_idx{0};
};

template<typename T, typename Impl>
typename SynchronizationMultiQueue<T, Impl>::View SynchronizationMultiQueue<T, Impl>::view() {
    return View{*this};
}
