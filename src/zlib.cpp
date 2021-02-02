#include <decoco/decoco.hpp>
#include <zlib.h>
#include <assert.h>

namespace Decoco {

struct ZlibCompressorS : Compressor {
  static int compressorLevelToZlib(Compressor::Level level) {
    switch(level) {
      default: 
      case Compressor::Level::Balanced: return Z_DEFAULT_COMPRESSION;
      case Compressor::Level::Fast: return Z_BEST_SPEED;
      case Compressor::Level::Small: return Z_BEST_COMPRESSION;
    }
  }
  ZlibCompressorS(Compressor::Level level, size_t chunkSize)
  : strm()
  , chunkSize(chunkSize)
  {
    int ret = deflateInit(&strm, compressorLevelToZlib(level));
    assert(ret == Z_OK);
  }
  std::vector<uint8_t> compress(std::span<const uint8_t> in) override {
    std::vector<uint8_t> out;

    strm.avail_in = in.size();
    strm.next_in = const_cast<uint8_t*>(in.data());
    do {
      out.resize(out.size() + chunkSize);
      strm.avail_out = chunkSize;
      strm.next_out = out.data() + out.size() - chunkSize;
      int ret = deflate(&strm, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
    } while (strm.avail_out == 0);
    out.resize(out.size() - strm.avail_out);

    return out;
  }
  std::vector<uint8_t> flush() override {
    std::vector<uint8_t> out;

    do {
      out.resize(out.size() + chunkSize);
      strm.avail_in = 0;
      strm.next_in = nullptr;
      strm.avail_out = chunkSize;
      strm.next_out = out.data() + out.size() - chunkSize;
      int ret = deflate(&strm, Z_FINISH);
      assert(ret != Z_STREAM_ERROR);
    } while (strm.avail_out == 0);
    out.resize(out.size() - strm.avail_out);

    return out;
  }
  ~ZlibCompressorS() {
    deflateEnd(&strm);
  }
  z_stream strm;
  size_t chunkSize;
};

std::unique_ptr<Compressor> ZlibCompressor(Compressor::Level level, size_t chunkSize) { return std::make_unique<ZlibCompressorS>(level, chunkSize); }

struct ZlibDecompressorS : Decompressor {
  ZlibDecompressorS(size_t outputChunkSize)
  : strm()
  , outputChunkSize(outputChunkSize)
  {
    int ret = inflateInit(&strm);
    assert(ret == Z_OK);
  }
  std::vector<uint8_t> decompress(std::span<const uint8_t> in) override {
    std::vector<uint8_t> out;
    if (strm.avail_in == 0) {
      strm.next_in = const_cast<uint8_t*>(in.data());
      strm.avail_in = in.size();
    } else if (not in.empty()) {
      // you can't put more data in before the rest is taken out
      std::terminate();
    }

    out.resize(outputChunkSize);
    strm.avail_out = out.size();
    strm.next_out = out.data();
    int ret = inflate(&strm, Z_NO_FLUSH);
    assert(ret == Z_OK || ret == Z_STREAM_END);
    out.resize(outputChunkSize - strm.avail_out);
    return out;
  }
  ~ZlibDecompressorS() {
    inflateEnd(&strm);
  }
  z_stream strm;
  size_t outputChunkSize;
};

std::unique_ptr<Decompressor> ZlibDecompressor(size_t outputChunkSize) { return std::make_unique<ZlibDecompressorS>(outputChunkSize); }

}

