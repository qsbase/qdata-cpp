#include "io/multithreaded_block_module.h"
#include "io/filestream_module.h"
#include "io/xxhash_module.h"
#include "io/zstd_module.h"

using io_writer_t = BlockCompressWriterMT<OfStreamWriter, ZstdCompressor, xxHashEnv, StdErrorPolicy, true>;
using io_reader_t = BlockCompressReaderMT<IfStreamReader, ZstdDecompressor, StdErrorPolicy>;

int main() {
    return sizeof(io_writer_t) == 0 || sizeof(io_reader_t) == 0;
}
