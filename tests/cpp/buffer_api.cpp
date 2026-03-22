#include <cstdint>
#include <string>
#include <vector>

#include "qdata.h"

namespace {

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
    const std::vector<std::int32_t> input{1, 2, 3, 4};

    const auto bytes = qdata::serialize(input);
    expect_integer_payload(qdata::deserialize(bytes), input);

    const auto chars = qdata::serialize<std::vector<char>>(input);
    expect_integer_payload(qdata::deserialize(chars), input);

    const auto uints = qdata::serialize<std::vector<std::uint8_t>>(input);
    expect_integer_payload(qdata::deserialize(uints), input);

    const auto text = qdata::serialize<std::string>(input);
    expect_integer_payload(qdata::deserialize(text), input);

    return 0;
}
