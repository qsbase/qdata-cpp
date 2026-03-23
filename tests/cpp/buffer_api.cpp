#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "qdata.h"

namespace {

void debug_log(const char* message) {
    std::cerr << "[qdata_buffer_api] " << message << '\n';
    std::cerr.flush();
}

void expect_integer_payload(const qdata::object& obj, const std::vector<std::int32_t>& expected) {
    if(!qdata::holds_alternative<qdata::integer_vector>(obj)) {
        throw std::runtime_error("unexpected payload type");
    }

    const auto* ints_ptr = qdata::get_if<qdata::integer_vector>(&obj);
    if(ints_ptr == nullptr) {
        throw std::runtime_error("missing integer payload");
    }

    const auto& ints = qdata::get<qdata::integer_vector>(obj);
    if(ints.values != expected) {
        throw std::runtime_error("integer payload mismatch");
    }
}

} // namespace

int main() {
    debug_log("start");
    const std::vector<std::int32_t> input{1, 2, 3, 4};

    debug_log("serialize std::vector<std::byte>");
    const auto bytes = qdata::serialize(input);
    debug_log("deserialize std::vector<std::byte>");
    expect_integer_payload(qdata::deserialize(bytes), input);

    debug_log("serialize std::vector<char>");
    const auto chars = qdata::serialize<std::vector<char>>(input);
    debug_log("deserialize std::vector<char>");
    expect_integer_payload(qdata::deserialize(chars), input);

    debug_log("serialize std::vector<std::uint8_t>");
    const auto uints = qdata::serialize<std::vector<std::uint8_t>>(input);
    debug_log("deserialize std::vector<std::uint8_t>");
    expect_integer_payload(qdata::deserialize(uints), input);

    debug_log("serialize std::string");
    const auto text = qdata::serialize<std::string>(input);
    debug_log("deserialize std::string");
    expect_integer_payload(qdata::deserialize(text), input);

    debug_log("done");
    return 0;
}
