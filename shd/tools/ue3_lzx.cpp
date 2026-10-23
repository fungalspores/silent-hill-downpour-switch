// Silent Hill: Downpour packages: UE3 compressed chunks with Xbox 360 LZX (XMemCompress) blocks of 128 KB.
// Each block is a sequence of frames: [0xFF, u16 uncompressed, u16 compressed] or [u16 compressed] (32 KB out),
// all big-endian. The frames are concatenated and fed to libmspack's LZX decoder (window 128 KB), one stream per
// block. Used by ue3_shader_scan (UE3_LZX).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "mspack.h"
#include "lzx.h"
}

namespace {
struct MemFile {
  const uint8_t* src = nullptr;
  uint8_t* dst = nullptr;
  size_t size = 0, off = 0;
};
int Read(mspack_file* f, void* buf, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  size_t k = std::min(size_t(n), m->size - m->off);
  std::memcpy(buf, m->src + m->off, k);
  m->off += k;
  return int(k);
}
int Write(mspack_file* f, void* buf, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  size_t k = std::min(size_t(n), m->size - m->off);
  std::memcpy(m->dst + m->off, buf, k);
  m->off += k;
  return int(k);
}
void* Alloc(mspack_system*, size_t n) { return std::calloc(n, 1); }
void Free(void* p) { std::free(p); }
void Copy(void* s, void* d, size_t n) { std::memmove(d, s, n); }
}  // namespace

bool Ue3LzxBlock(const uint8_t* in, size_t in_len, uint8_t* out, size_t out_len) {
  std::vector<uint8_t> raw;
  raw.reserve(in_len);
  size_t i = 0;
  while (i < in_len) {
    size_t c;
    if (in[i] == 0xFF) {
      if (i + 5 > in_len) return false;
      c = (size_t(in[i + 3]) << 8) | in[i + 4];
      i += 5;
    } else {
      if (i + 2 > in_len) return false;
      c = (size_t(in[i]) << 8) | in[i + 1];
      i += 2;
    }
    if (c == 0) break;
    if (i + c > in_len) return false;
    raw.insert(raw.end(), in + i, in + i + c);
    i += c;
  }
  mspack_system sys = {};
  sys.read = Read;
  sys.write = Write;
  sys.alloc = Alloc;
  sys.free = Free;
  sys.copy = Copy;
  MemFile src{raw.data(), nullptr, raw.size(), 0};
  MemFile dst{nullptr, out, out_len, 0};
  lzxd_stream* s = lzxd_init(&sys, reinterpret_cast<mspack_file*>(&src), reinterpret_cast<mspack_file*>(&dst), 17,
                             0, 0x8000, off_t(out_len), 0);
  if (!s) return false;
  int r = lzxd_decompress(s, off_t(out_len));
  lzxd_free(s);
  return r == MSPACK_ERR_OK && dst.off == out_len;
}
