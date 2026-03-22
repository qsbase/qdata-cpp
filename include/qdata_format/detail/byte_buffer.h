#ifndef QDATA_FORMAT_DETAIL_BYTE_BUFFER_H
#define QDATA_FORMAT_DETAIL_BYTE_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace qdata {
namespace detail {

template <class Buffer, class Enable = void>
struct is_byte_input_buffer : std::false_type {};

template <class Buffer>
struct is_byte_input_buffer<Buffer, std::void_t<
    typename Buffer::value_type,
    decltype(std::declval<const Buffer&>().data()),
    decltype(std::declval<const Buffer&>().size())
>> : std::bool_constant<
    sizeof(typename Buffer::value_type) == 1 &&
    std::is_trivially_copyable<typename Buffer::value_type>::value &&
    std::is_convertible<decltype(std::declval<const Buffer&>().data()),
                        const typename Buffer::value_type*>::value &&
    std::is_convertible<decltype(std::declval<const Buffer&>().size()), std::size_t>::value
> {};

template <class Buffer, class Enable = void>
struct is_byte_output_buffer : std::false_type {};

template <class Buffer>
struct is_byte_output_buffer<Buffer, std::void_t<
    typename Buffer::value_type,
    decltype(std::declval<Buffer&>().data()),
    decltype(std::declval<const Buffer&>().size()),
    decltype(std::declval<Buffer&>().resize(std::declval<std::size_t>()))
>> : std::bool_constant<
    is_byte_input_buffer<Buffer>::value &&
    std::is_default_constructible<Buffer>::value &&
    std::is_convertible<decltype(std::declval<Buffer&>().data()),
                        typename Buffer::value_type*>::value
> {};

template <class Buffer>
inline void validate_input_buffer() {
    static_assert(
        is_byte_input_buffer<Buffer>::value,
        "Buffer must expose a contiguous 1-byte trivially copyable value_type with data() and size()"
    );
}

template <class Buffer>
inline void validate_output_buffer() {
    static_assert(
        is_byte_output_buffer<Buffer>::value,
        "Buffer must expose a contiguous 1-byte trivially copyable value_type with data(), size(), resize(), and a default constructor"
    );
}

template <class Buffer>
inline std::size_t buffer_size_bytes(const Buffer& buffer) {
    validate_input_buffer<Buffer>();
    return static_cast<std::size_t>(buffer.size()) * sizeof(typename Buffer::value_type);
}

template <class Buffer>
inline const void* buffer_data(const Buffer& buffer) {
    validate_input_buffer<Buffer>();
    return static_cast<const void*>(buffer.data());
}

inline std::size_t checked_required_capacity(const std::size_t position,
                                             const std::uint64_t additional_size) {
    if(additional_size > static_cast<std::uint64_t>(std::size_t(-1)) - position) {
        throw std::length_error("serialized qdata exceeds addressable buffer size");
    }
    return position + static_cast<std::size_t>(additional_size);
}

} // namespace detail
} // namespace qdata

#endif
