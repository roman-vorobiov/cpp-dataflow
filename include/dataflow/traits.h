#pragma once

#include <utility>

namespace dataflow {

template<typename T>
constexpr bool isConsumer = std::is_base_of_v<detail::ConsumerTag, T>;

template<typename T>
constexpr bool isProducer = std::is_base_of_v<detail::ProducerTag, T>;

} // namespace dataflow
