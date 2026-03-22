#ifndef QDATA_FORMAT_DETAIL_MEMORY_STREAM_H
#define QDATA_FORMAT_DETAIL_MEMORY_STREAM_H

#include "byte_buffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace qdata {
namespace detail {

class memory_reader {
public:
    memory_reader(const void* const data, const std::uint64_t size) :
    buffer_(reinterpret_cast<const char*>(data)),
    size_(size),
    position_(0) {}

    std::uint32_t read(char* const data, const std::uint64_t bytes_to_read) {
        const auto bytes_available = size_ - position_;
        const auto bytes_to_actually_read = std::min(bytes_to_read, bytes_available);
        if(bytes_to_actually_read > 0) {
            std::memcpy(data, buffer_ + position_, static_cast<std::size_t>(bytes_to_actually_read));
            position_ += bytes_to_actually_read;
        }
        return static_cast<std::uint32_t>(bytes_to_actually_read);
    }

    template <typename T>
    bool readInteger(T& value) {
        return read(reinterpret_cast<char*>(&value), sizeof(T)) == sizeof(T);
    }

    void seekg(const std::uint64_t pos) {
        position_ = std::min(pos, size_);
    }

    std::uint64_t tellg() const {
        return position_;
    }

private:
    const char* buffer_;
    std::uint64_t size_;
    std::uint64_t position_;
};

template <class Buffer>
class memory_writer {
public:
    memory_writer() : buffer_(), position_(0) {
        validate_output_buffer<Buffer>();
    }

    std::uint32_t write(const char* const data, const std::uint64_t size) {
        ensure_capacity(size);
        if(size > 0) {
            std::memcpy(buffer_.data() + position_, data, static_cast<std::size_t>(size));
            position_ += size;
        }
        return static_cast<std::uint32_t>(size);
    }

    template <typename T>
    void writeInteger(const T value) {
        write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    void seekp(const std::uint64_t pos) {
        if(pos > buffer_.size()) {
            throw std::out_of_range("Seek position is beyond buffer capacity");
        }
        position_ = pos;
    }

    std::uint64_t tellp() const {
        return position_;
    }

    Buffer take_bytes(const std::size_t size) {
        if(size > buffer_.size()) {
            throw std::out_of_range("Requested output size exceeds buffer size");
        }
        buffer_.resize(size);
        return std::move(buffer_);
    }

private:
    static constexpr std::size_t initial_capacity_bytes = 1024;

    Buffer buffer_;
    std::size_t position_;

    void ensure_capacity(const std::uint64_t additional_size) {
        const auto required_size = checked_required_capacity(position_, additional_size);
        if(required_size > buffer_.size()) {
            std::size_t grown_size = buffer_.size() > 0 ? buffer_.size() : initial_capacity_bytes;
            while(grown_size < required_size) {
                if(grown_size > std::size_t(-1) / 2) {
                    grown_size = required_size;
                    break;
                }
                grown_size *= 2;
            }
            buffer_.resize(grown_size);
        }
    }
};

} // namespace detail
} // namespace qdata

#endif
