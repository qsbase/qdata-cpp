#ifndef QDATA_FORMAT_DETAIL_QDATA_DESERIALIZER_H
#define QDATA_FORMAT_DETAIL_QDATA_DESERIALIZER_H

#include "../core_types.h"
#include "file_headers.h"
#include "memory_stream.h"
#include "read_common.h"

#include "io/block_module.h"
#include "io/filestream_module.h"
#include "io/zstd_module.h"

#ifdef QIO_HAS_TBB
#include <tbb/global_control.h>
#include "io/multithreaded_block_module.h"
#endif

#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace qdata {
namespace detail {

inline std::size_t checked_deserializer_size(const std::uint64_t value, const char* const what) {
    if(value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(what) + " exceeds this platform's size_t range");
    }
    return static_cast<std::size_t>(value);
}

inline int normalized_read_nthreads(const int value) {
    return value > 1 ? value : 1;
}

template <class BlockReader>
class qdata_deserializer {
public:
    explicit qdata_deserializer(BlockReader& reader) : reader_(reader) {}

    object read_object() {
        object out;
        read_into(out);
        return out;
    }

    void read_object_data() {
        for(auto* values : string_payloads_) {
            read_string_payloads(*values);
        }
        for(auto* values : complex_payloads_) {
            reader_.get_data(
                reinterpret_cast<char*>(values->data()),
                values->size() * sizeof(std::complex<double>)
            );
        }
        for(auto* values : real_payloads_) {
            reader_.get_data(
                reinterpret_cast<char*>(values->data()),
                values->size() * sizeof(double)
            );
        }
        for(auto* values : integer_payloads_) {
            reader_.get_data(
                reinterpret_cast<char*>(values->data()),
                values->size() * sizeof(std::int32_t)
            );
        }
        for(auto* values : raw_payloads_) {
            reader_.get_data(
                reinterpret_cast<char*>(values->data()),
                values->size() * sizeof(std::byte)
            );
        }
    }

private:
    BlockReader& reader_;
    std::vector<string_vector*> string_payloads_;
    std::vector<std::vector<std::complex<double>>*> complex_payloads_;
    std::vector<std::vector<double>*> real_payloads_;
    std::vector<std::vector<std::int32_t>*> integer_payloads_;
    std::vector<std::vector<std::byte>*> raw_payloads_;

    void read_string_payloads(string_vector& values) {
        const auto expected_strings = values.records.size();
        auto storage = std::make_shared<string_storage>();
        std::unique_ptr<char[]> current_slab;
        std::size_t slab_used = 0;
        std::size_t slab_capacity = 0;

        auto finish_current_slab = [&]() {
            if(current_slab) {
                storage->slabs.push_back(std::move(current_slab));
                slab_used = 0;
                slab_capacity = 0;
            }
        };

        for(std::size_t i = 0; i < expected_strings; ++i) {
            std::uint32_t string_length = 0;
            detail::read_string_header(reader_, string_length);

            if(string_length == NA_STRING_LENGTH) {
                values.records[i] = string_ref{};
                continue;
            }

            if(string_length == 0) {
                values.records[i] = {nullptr, 0};
                continue;
            }

            const auto payload_size = static_cast<std::size_t>(string_length);
            if(payload_size > default_string_storage_bytes) {
                finish_current_slab();
                auto large_slab = std::make_unique<char[]>(payload_size);
                reader_.get_data(large_slab.get(), payload_size);
                values.records[i] = {large_slab.get(), string_length};
                storage->slabs.push_back(std::move(large_slab));
                continue;
            }

            if(!current_slab || slab_capacity - slab_used < payload_size) {
                finish_current_slab();
                slab_capacity = default_string_storage_bytes;
                current_slab = std::make_unique<char[]>(slab_capacity);
            }

            char* const destination = current_slab.get() + slab_used;
            reader_.get_data(destination, payload_size);
            values.records[i] = {destination, string_length};
            slab_used += payload_size;
        }

        finish_current_slab();
        values.storage = std::move(storage);
    }

    void read_attributes(std::vector<box<named_object>>& attrs, const std::uint32_t attr_length) {
        attrs.reserve(attr_length);
        for(std::uint32_t i = 0; i < attr_length; ++i) {
            std::uint32_t name_length = 0;
            detail::read_string_header(reader_, name_length);
            if(name_length == NA_STRING_LENGTH) {
                reader_.cleanup_and_throw("Attribute names cannot be NA");
            }
            std::string name;
            name.resize(name_length);
            if(name_length > 0) {
                reader_.get_data(name.data(), name_length);
            }
            attrs.emplace_back(named_object{});
            auto& attr = *attrs.back();
            attr.name = std::move(name);
            read_into(attr.data);
        }
    }

    void read_into(object& out) {
        qstype type;
        std::uint64_t object_length = 0;
        std::uint32_t attr_length = 0;
        detail::read_object_header(reader_, type, object_length, attr_length);

        switch(type) {
            case qstype::NIL:
                out.data = nil_value{};
                return;
            case qstype::LOGICAL: {
                out.data = logical_vector{};
                auto& stored = std::get<logical_vector>(out.data);
                stored.values.resize(checked_deserializer_size(object_length, "logical vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.values.empty()) {
                    integer_payloads_.push_back(&stored.values);
                }
                return;
            }
            case qstype::INTEGER: {
                out.data = integer_vector{};
                auto& stored = std::get<integer_vector>(out.data);
                stored.values.resize(checked_deserializer_size(object_length, "integer vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.values.empty()) {
                    integer_payloads_.push_back(&stored.values);
                }
                return;
            }
            case qstype::REAL: {
                out.data = real_vector{};
                auto& stored = std::get<real_vector>(out.data);
                stored.values.resize(checked_deserializer_size(object_length, "real vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.values.empty()) {
                    real_payloads_.push_back(&stored.values);
                }
                return;
            }
            case qstype::COMPLEX: {
                out.data = complex_vector{};
                auto& stored = std::get<complex_vector>(out.data);
                stored.values.resize(checked_deserializer_size(object_length, "complex vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.values.empty()) {
                    complex_payloads_.push_back(&stored.values);
                }
                return;
            }
            case qstype::CHARACTER: {
                out.data = string_vector{};
                auto& stored = std::get<string_vector>(out.data);
                stored.records.resize(checked_deserializer_size(object_length, "string vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.records.empty()) {
                    string_payloads_.push_back(&stored);
                }
                return;
            }
            case qstype::LIST: {
                out.data = list_vector{};
                auto& stored = std::get<list_vector>(out.data);
                read_attributes(stored.attrs, attr_length);
                const auto list_size = checked_deserializer_size(object_length, "list length");
                stored.values.reserve(list_size);
                for(std::size_t i = 0; i < list_size; ++i) {
                    stored.values.emplace_back(object{});
                    read_into(*stored.values.back());
                }
                return;
            }
            case qstype::RAW: {
                out.data = raw_vector{};
                auto& stored = std::get<raw_vector>(out.data);
                stored.values.resize(checked_deserializer_size(object_length, "raw vector length"));
                read_attributes(stored.attrs, attr_length);
                if(!stored.values.empty()) {
                    raw_payloads_.push_back(&stored.values);
                }
                return;
            }
            default:
                reader_.cleanup_and_throw("Unsupported qdata object type");
        }
    }
};

template <class StreamReader, class Decompressor>
inline object read_single_thread(StreamReader& stream) {
    BlockCompressReader<StreamReader, Decompressor, StdErrorPolicy> block_reader(stream);
    qdata_deserializer<decltype(block_reader)> stream_reader(block_reader);
    auto output = stream_reader.read_object();
    stream_reader.read_object_data();
    block_reader.finish();
    return output;
}

#ifdef QIO_HAS_TBB
template <class StreamReader, class Decompressor>
inline object read_multi_thread(StreamReader& stream, const int nthreads) {
    tbb::global_control gc(tbb::global_control::parameter::max_allowed_parallelism, normalized_read_nthreads(nthreads));
    BlockCompressReaderMT<StreamReader, Decompressor, StdErrorPolicy> block_reader(stream);
    qdata_deserializer<decltype(block_reader)> stream_reader(block_reader);
    auto output = stream_reader.read_object();
    stream_reader.read_object_data();
    block_reader.finish();
    return output;
}
#endif

template <class StreamReader>
inline object read_qdata_object(StreamReader& stream, const bool validate_checksum, const int nthreads) {
    bool shuffle = false;
    std::uint64_t stored_hash = 0;
    read_qdata_header(stream, shuffle, stored_hash);

    if(validate_checksum) {
        if(stored_hash == 0) {
            throw std::runtime_error("qdata input does not contain a stored checksum");
        }
        const auto computed_hash = read_qx_hash(stream);
        if(computed_hash != stored_hash) {
            throw std::runtime_error("qdata checksum mismatch");
        }
    }

    if(shuffle) {
#ifdef QIO_HAS_TBB
        if(nthreads > 1) {
            return read_multi_thread<StreamReader, ZstdShuffleDecompressor>(stream, nthreads);
        }
#endif
        return read_single_thread<StreamReader, ZstdShuffleDecompressor>(stream);
    }

#ifdef QIO_HAS_TBB
    if(nthreads > 1) {
        return read_multi_thread<StreamReader, ZstdDecompressor>(stream, nthreads);
    }
#endif
    return read_single_thread<StreamReader, ZstdDecompressor>(stream);
}

inline object read_file_impl(const std::string& file, const bool validate_checksum, const int nthreads) {
    IfStreamReader stream(file.c_str());
    if(!stream.isValid()) {
        throw std::runtime_error("failed to open file for reading: " + file);
    }
    return read_qdata_object(stream, validate_checksum, nthreads);
}

inline object deserialize_impl(const void* data,
                               const std::size_t size,
                               const bool validate_checksum,
                               const int nthreads) {
    memory_reader stream(data, static_cast<std::uint64_t>(size));
    return read_qdata_object(stream, validate_checksum, nthreads);
}

} // namespace detail
} // namespace qdata

#endif
