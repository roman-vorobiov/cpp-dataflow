#define BOOST_TEST_MODULE main

#include <boost/test/unit_test.hpp>

#include "dataflow/Circuit.h"

namespace dataflow::test {

BOOST_AUTO_TEST_SUITE(dataflow)

BOOST_AUTO_TEST_SUITE(consumer)

BOOST_AUTO_TEST_CASE(unconnected_pipe) {
    Circuit circuit;

    auto& out = circuit.add<Consumer<int>>([](int) {});

    BOOST_CHECK_THROW(circuit.tick(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(single_input) {
    auto inPipe = SynchronizationMultiQueue<int>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<int>>([&](int input) {
        called = true;
        BOOST_CHECK_EQUAL(input, 123);
    });
    out.setInputPipe(inPipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    inPipe->push(123);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(single_input_optional) {
    auto inPipe = SynchronizationMultiQueue<std::optional<int>>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<std::optional<int>>>([&](const std::optional<int>& input) {
        called = true;
        BOOST_CHECK(!input.has_value());
    });
    out.setInputPipe(inPipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    inPipe->push(std::nullopt);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(bus) {
    auto in1Pipe = SynchronizationMultiQueue<int>::make();
    auto in2Pipe = SynchronizationMultiQueue<int>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<int[]>>([&](const std::vector<int>& inputs) {
        called = true;
        BOOST_REQUIRE_EQUAL(inputs.size(), 2);
        BOOST_CHECK_EQUAL(inputs[0], 123);
        BOOST_CHECK_EQUAL(inputs[1], 456);
    });
    out.addInputPipe(in1Pipe->view());
    out.addInputPipe(in2Pipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    in1Pipe->push(123);
    circuit.tick();
    BOOST_CHECK(!called);

    in2Pipe->push(456);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(bus_optional) {
    auto in1Pipe = SynchronizationMultiQueue<std::optional<int>>::make();
    auto in2Pipe = SynchronizationMultiQueue<std::optional<int>>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<std::optional<int>[]>>([&](const std::vector<std::optional<int>>& inputs) {
        called = true;
        BOOST_REQUIRE_EQUAL(inputs.size(), 2);
        BOOST_CHECK_EQUAL(*inputs[0], 123);
        BOOST_CHECK(!inputs[1].has_value());
    });
    out.addInputPipe(in1Pipe->view());
    out.addInputPipe(in2Pipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    in1Pipe->push(123);
    circuit.tick();
    BOOST_CHECK(!called);

    in2Pipe->push(std::nullopt);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(multiple_inputs) {
    auto in1Pipe = SynchronizationMultiQueue<int>::make();
    auto in2Pipe = SynchronizationMultiQueue<float>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<int, float>>([&](int l, float r) {
        called = true;
        BOOST_CHECK_EQUAL(l, 123);
        BOOST_CHECK_EQUAL(r, 0.5f);
    });
    out.setInputPipes(in1Pipe->view(), in2Pipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    in1Pipe->push(123);
    circuit.tick();
    BOOST_CHECK(!called);

    in2Pipe->push(0.5f);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(multiple_inputs_optional) {
    auto in1Pipe = SynchronizationMultiQueue<int>::make();
    auto in2Pipe = SynchronizationMultiQueue<std::optional<int>>::make();

    Circuit circuit;

    bool called = false;
    auto& out = circuit.add<Consumer<int, std::optional<int>>>([&](int l, const std::optional<int>& r) {
        called = true;
        BOOST_CHECK_EQUAL(l, 123);
        BOOST_CHECK(!r.has_value());
    });
    out.setInputPipes(in1Pipe->view(), in2Pipe->view());

    circuit.tick();
    BOOST_CHECK(!called);

    in1Pipe->push(123);
    circuit.tick();
    BOOST_CHECK(!called);

    in2Pipe->push(std::nullopt);
    circuit.tick();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_SUITE_END() // consumer

BOOST_AUTO_TEST_SUITE(producer)

BOOST_AUTO_TEST_CASE(unconnected_pipe) {
    Circuit circuit;

    bool called = false;
    auto& in = circuit.add<Producer<int>>([&] {
        called = true;
        return 123;
    });

    circuit.tick();

    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(optional) {
    auto impl = []() -> std::optional<int> { return std::nullopt; };

    Circuit circuit;

    auto& in1 = circuit.add<Producer<int>>(impl);
    auto pipe1 = in1.getOutputPipe();

    auto& in2 = circuit.add<Producer<std::optional<int>>>(impl);
    auto pipe2 = in2.getOutputPipe();

    circuit.tick();

    BOOST_CHECK_EQUAL(pipe1.size(), 0);
    BOOST_CHECK(!pipe2.pop().has_value());
}

BOOST_AUTO_TEST_CASE(bus) {
    Circuit circuit;

    auto& in = circuit.add<Producer<int[]>>([]() -> std::vector<int> { return {1, 2}; });
    auto pipe1 = in.getOutputPipe(0);
    auto pipe2 = in.getOutputPipe(1);
    auto pipe3 = in.getOutputPipe(2);

    circuit.tick();

    BOOST_CHECK_EQUAL(pipe1.pop(), 1);
    BOOST_CHECK_EQUAL(pipe2.pop(), 2);
    BOOST_CHECK_EQUAL(pipe3.size(), 0);
}

BOOST_AUTO_TEST_CASE(bus_optional) {
    auto impl = []() -> std::vector<std::optional<int>> { return {1, std::nullopt}; };

    Circuit circuit;

    auto& in1 = circuit.add<Producer<int[]>>(impl);
    auto pipe11 = in1.getOutputPipe(0);
    auto pipe12 = in1.getOutputPipe(1);
    auto pipe13 = in1.getOutputPipe(2);

    auto& in2 = circuit.add<Producer<std::optional<int>[]>>(impl);
    auto pipe21 = in2.getOutputPipe(0);
    auto pipe22 = in2.getOutputPipe(1);
    auto pipe23 = in2.getOutputPipe(2);

    circuit.tick();

    BOOST_CHECK_EQUAL(pipe11.pop(), 1);
    BOOST_CHECK_EQUAL(pipe12.size(), 0);
    BOOST_CHECK_EQUAL(pipe13.size(), 0);

    BOOST_CHECK_EQUAL(*pipe21.pop(), 1);
    BOOST_CHECK(!pipe22.pop().has_value());
    BOOST_CHECK_EQUAL(pipe23.size(), 0);
}

BOOST_AUTO_TEST_CASE(multiple_outputs) {
    Circuit circuit;

    auto& in = circuit.add<Producer<int, float>>([]() -> std::pair<int, float> { return {1, 0.5f}; });

    auto pipe1 = in.getOutputPipe<0>();
    auto pipe2 = in.getOutputPipe<1>();

    circuit.tick();

    BOOST_CHECK_EQUAL(pipe1.pop(), 1);
    BOOST_CHECK_EQUAL(pipe2.pop(), 0.5f);
}

BOOST_AUTO_TEST_CASE(multiple_outputs_optional) {
    auto impl = []() -> std::pair<int, std::optional<int>> { return {1, std::nullopt}; };

    Circuit circuit;

    auto& in1 = circuit.add<Producer<int, int>>(impl);
    auto pipe11 = in1.getOutputPipe<0>();
    auto pipe12 = in1.getOutputPipe<1>();

    auto& in2 = circuit.add<Producer<int, std::optional<int>>>(impl);
    auto pipe21 = in2.getOutputPipe<0>();
    auto pipe22 = in2.getOutputPipe<1>();

    circuit.tick();

    BOOST_CHECK_EQUAL(pipe11.pop(), 1);
    BOOST_CHECK_EQUAL(pipe12.size(), 0);

    BOOST_CHECK_EQUAL(pipe21.pop(), 1);
    BOOST_CHECK(!pipe22.pop().has_value());
}

BOOST_AUTO_TEST_CASE(divergence) {
    Circuit circuit;

    auto& in = circuit.add<Producer<int>>([counter = 1]() mutable { return counter++; });

    auto pipe1 = in.getOutputPipe();
    auto pipe2 = in.getOutputPipe();

    circuit.tick();
    circuit.tick();

    BOOST_CHECK_EQUAL(pipe1.pop(), 1);
    BOOST_CHECK_EQUAL(pipe1.pop(), 2);
    BOOST_CHECK_EQUAL(pipe2.pop(), 1);
    BOOST_CHECK_EQUAL(pipe2.pop(), 2);
}

BOOST_AUTO_TEST_SUITE_END() // producer

BOOST_AUTO_TEST_SUITE(producer_consumer)

BOOST_AUTO_TEST_CASE(connected_pipe) {
    Circuit circuit;

    auto& in = circuit.add<Producer<int>>([] { return 1; });

    auto& out = circuit.add<Consumer<int>, Producer<float>>([&](int input) { return input / 2.0f; });
    out.setInputPipe(in.getOutputPipe());

    auto outPipe = out.getOutputPipe();

    circuit.tick();

    BOOST_CHECK_EQUAL(outPipe.pop(), 0.5f);
}

BOOST_AUTO_TEST_SUITE_END() // producer_consumer

BOOST_AUTO_TEST_SUITE_END() // dataflow

} // namespace dataflow::test
