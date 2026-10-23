// CPU memory export for draws the native renderer cannot run on the host GPU. See the header.
#include "native_memexport_cpu.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <rex/graphics/format/ucode.h>
#include <rex/graphics/pipeline/shader/interpreter.h>
#include <rex/graphics/pipeline/shader/shader.h>
#include <rex/graphics/xenos.h>

namespace p2::memexport {
namespace {

using namespace rex::graphics;

// Collects eA and eM# of one shader invocation and stores them the way the SDK's SPIR-V
// translator does when the export ends (next alloc, or the end of the shader).
class Sink : public ShaderInterpreter::ExportSink {
 public:
  explicit Sink(rex::memory::Memory& memory, Result& result) : memory_(memory), result_(result) {}

  void Reset() {
    Flush();
    in_export_ = false;
  }

  void AllocExport(ucode::AllocType type, uint32_t) override {
    Flush();
    in_export_ = type == ucode::AllocType::kMemory;
    address_written_ = false;
    written_ = 0;
  }

  void Export(ucode::ExportRegister reg, const float* value, uint32_t mask) override {
    if (!in_export_) return;
    const uint32_t r = uint32_t(reg);
    if (r == uint32_t(ucode::ExportRegister::kExportAddress)) {
      for (uint32_t i = 0; i < 4; ++i) {
        if (mask & (1u << i)) std::memcpy(&address_[i], &value[i], 4);
      }
      address_written_ = true;
      return;
    }
    if (r >= uint32_t(ucode::ExportRegister::kExportData0) &&
        r < uint32_t(ucode::ExportRegister::kExportData0) + ucode::kMaxMemExportElementCount) {
      const uint32_t index = r - uint32_t(ucode::ExportRegister::kExportData0);
      for (uint32_t i = 0; i < 4; ++i) {
        if (mask & (1u << i)) data_[index][i] = value[i];
      }
      written_ |= 1u << index;
    }
  }

  void Flush() {
    if (!in_export_ || !address_written_ || !written_) {
      written_ = 0;
      return;
    }
    Store();
    written_ = 0;
  }

 private:
  static uint32_t Swap32(uint32_t v, uint32_t endian) {
    switch (endian) {
      case 1:  // 8in16
        return ((v & 0x00FF00FFu) << 8) | ((v & 0xFF00FF00u) >> 8);
      case 2:  // 8in32
        return (v << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | (v >> 24);
      case 3:  // 16in32
        return (v << 16) | (v >> 16);
      default:
        return v;
    }
  }

  static uint32_t PackFixed16(float f, bool is_signed, bool is_norm) {
    if (std::isnan(f)) f = 0.0f;
    if (is_signed) {
      if (is_norm) f = std::clamp(f, -1.0f, 1.0f) * 32767.0f;
      const float rounded = f + (f < 0.0f ? -0.5f : 0.5f);
      int32_t i = int32_t(std::clamp(rounded, -2147483648.0f, 2147483520.0f));
      return uint32_t(i) & 0xFFFFu;
    }
    if (is_norm) f = std::clamp(f, 0.0f, 1.0f) * 65535.0f;
    const float rounded = std::max(f + 0.5f, 0.0f);
    return uint32_t(std::min(rounded, 4294967040.0f)) & 0xFFFFu;
  }

  static uint32_t PackUnorm8(float f) {
    if (std::isnan(f)) f = 0.0f;
    return uint32_t(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f) & 0xFFu;
  }

  static uint32_t PackHalf(float f) {
    if (std::isnan(f)) f = 0.0f;
    uint32_t x;
    std::memcpy(&x, &f, 4);
    const uint32_t sign = (x >> 16) & 0x8000u;
    int32_t exp = int32_t((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = x & 0x7FFFFFu;
    if (exp <= 0) {
      if (exp < -10) return sign;
      mant |= 0x800000u;
      const uint32_t shift = uint32_t(14 - exp);
      uint32_t h = mant >> shift;
      if ((mant >> (shift - 1)) & 1u) ++h;
      return sign | h;
    }
    if (exp >= 31) return sign | 0x7C00u;
    uint32_t h = (uint32_t(exp) << 10) | (mant >> 13);
    if (mant & 0x1000u) ++h;
    return sign | h;
  }

  void Store() {
    // Validate eA as the hardware does: (x >> 30) == 1, (y/z/w >> 23) == 0x96.
    if ((address_[0] >> 30) != 1 || (address_[1] >> 23) != 0x96 || (address_[2] >> 23) != 0x96 ||
        (address_[3] >> 23) != 0x96) {
      return;
    }
    const uint32_t format_info = address_[2];
    const uint32_t endian = format_info & 7u;
    const auto format = xenos::ColorFormat((format_info >> 8) & 0x3Fu);
    const bool is_signed = (format_info >> 16) & 1u;
    const bool is_norm = !((format_info >> 17) & 1u);
    const bool swap_rb = (format_info >> 19) & 1u;
    const uint32_t eM0_index = address_[1] & 0x7FFFFFu;
    const uint32_t count = address_[3] & 0x7FFFFFu;
    const int32_t remaining =
        std::clamp(int32_t(count) - int32_t(eM0_index), 0, int32_t(ucode::kMaxMemExportElementCount));
    const uint32_t store_mask = written_ & ((1u << remaining) - 1u);
    if (!store_mask) return;

    uint32_t element_bytes_log2;
    switch (format) {
      case xenos::ColorFormat::k_8_8_8_8:
      case xenos::ColorFormat::k_2_10_10_10:
      case xenos::ColorFormat::k_16_16:
      case xenos::ColorFormat::k_16_16_FLOAT:
      case xenos::ColorFormat::k_32_FLOAT:
        element_bytes_log2 = 2;
        break;
      case xenos::ColorFormat::k_16_16_16_16:
      case xenos::ColorFormat::k_16_16_16_16_FLOAT:
      case xenos::ColorFormat::k_32_32_FLOAT:
        element_bytes_log2 = 3;
        break;
      case xenos::ColorFormat::k_32_32_32_32_FLOAT:
        element_bytes_log2 = 4;
        break;
      default:
        return;  // not a format seen from SHD's shader; leave memory alone
    }
    const uint32_t base_bytes = (address_[0] << 2) & 0x1FFFFFFFu;

    for (uint32_t e = 0; e < ucode::kMaxMemExportElementCount; ++e) {
      if (!(store_mask & (1u << e))) continue;
      float v[4] = {data_[e][0], data_[e][1], data_[e][2], data_[e][3]};
      if (swap_rb) std::swap(v[0], v[2]);
      uint32_t packed[4] = {};
      switch (format) {
        case xenos::ColorFormat::k_8_8_8_8:
          packed[0] = PackUnorm8(v[0]) | (PackUnorm8(v[1]) << 8) | (PackUnorm8(v[2]) << 16) |
                      (PackUnorm8(v[3]) << 24);
          break;
        case xenos::ColorFormat::k_16_16:
          packed[0] = PackFixed16(v[0], is_signed, is_norm) |
                      (PackFixed16(v[1], is_signed, is_norm) << 16);
          break;
        case xenos::ColorFormat::k_16_16_16_16:
          packed[0] = PackFixed16(v[0], is_signed, is_norm) |
                      (PackFixed16(v[1], is_signed, is_norm) << 16);
          packed[1] = PackFixed16(v[2], is_signed, is_norm) |
                      (PackFixed16(v[3], is_signed, is_norm) << 16);
          break;
        case xenos::ColorFormat::k_16_16_FLOAT:
          packed[0] = PackHalf(v[0]) | (PackHalf(v[1]) << 16);
          break;
        case xenos::ColorFormat::k_16_16_16_16_FLOAT:
          packed[0] = PackHalf(v[0]) | (PackHalf(v[1]) << 16);
          packed[1] = PackHalf(v[2]) | (PackHalf(v[3]) << 16);
          break;
        case xenos::ColorFormat::k_32_FLOAT:
          std::memcpy(&packed[0], &v[0], 4);
          break;
        case xenos::ColorFormat::k_32_32_FLOAT:
          std::memcpy(&packed[0], &v[0], 8);
          break;
        case xenos::ColorFormat::k_32_32_32_32_FLOAT:
          std::memcpy(&packed[0], &v[0], 16);
          break;
        case xenos::ColorFormat::k_2_10_10_10: {
          auto u10 = [&](float f) {
            if (std::isnan(f)) f = 0.0f;
            return uint32_t(std::clamp(f, 0.0f, 1.0f) * 1023.0f + 0.5f) & 0x3FFu;
          };
          float a = v[3];
          if (std::isnan(a)) a = 0.0f;
          packed[0] = u10(v[0]) | (u10(v[1]) << 10) | (u10(v[2]) << 20) |
                      ((uint32_t(std::clamp(a, 0.0f, 1.0f) * 3.0f + 0.5f) & 3u) << 30);
          break;
        }
        default:
          return;
      }
      // 128-bit endian swap: 8in64 / 8in128 reorder the words, then swap within 32 bits.
      uint32_t word_endian = endian;
      if (endian == 4) {  // 8in64
        std::swap(packed[0], packed[1]);
        std::swap(packed[2], packed[3]);
        word_endian = 2;
      } else if (endian == 5) {  // 8in128
        std::swap(packed[0], packed[3]);
        std::swap(packed[1], packed[2]);
        word_endian = 2;
      }
      const uint32_t element_bytes = 1u << element_bytes_log2;
      const uint32_t address = base_bytes + ((eM0_index + e) << element_bytes_log2);
      if (uint64_t(address) + element_bytes > 0x20000000u) continue;
      // Queued: the GPU reads every vertex's input before the draw's writes land, and this
      // shader moves texture memory (UE3 defragmentation) between overlapping ranges.
      // Writing as we go made later vertices read bytes this draw had already replaced.
      PendingStore st;
      st.address = address;
      st.bytes = element_bytes;
      for (uint32_t w = 0; w < (element_bytes + 3) / 4; ++w) st.words[w] = Swap32(packed[w], word_endian);
      pending_.push_back(st);
    }
  }

 public:
  void Apply() {
    for (const PendingStore& st : pending_) {
      uint8_t* host = memory_.TranslatePhysical<uint8_t*>(st.address);
      uint8_t before[16];
      std::memcpy(before, host, st.bytes);
      std::memcpy(host, st.words, st.bytes);
      bool any_before = false;
      for (uint32_t b = 0; b < st.bytes; ++b) any_before |= before[b] != 0;
      if (std::memcmp(before, host, st.bytes) == 0) {
        ++result_.same;
      } else if (any_before) {
        ++result_.overwrote_nonzero;
      }
      result_.lo = std::min(result_.lo, st.address);
      result_.hi = std::max(result_.hi, st.address + st.bytes);
      ++result_.elements;
    }
    pending_.clear();
  }

 private:
  struct PendingStore {
    uint32_t address;
    uint32_t bytes;
    uint32_t words[4];
  };
  std::vector<PendingStore> pending_;
  rex::memory::Memory& memory_;
  Result& result_;
  bool in_export_ = false;
  bool address_written_ = false;
  uint32_t address_[4] = {};
  float data_[ucode::kMaxMemExportElementCount][4] = {};
  uint32_t written_ = 0;
};

}  // namespace

Result RunVertex(const RegisterFile& regs, rex::memory::Memory& memory, const Shader& vs,
                 const uint32_t* immediate_indices, uint32_t immediate_index_words) {
  Result result;
  if (!vs.is_ucode_analyzed() || !ShaderInterpreter::CanInterpretShader(vs)) return result;
  auto initiator = regs.Get<reg::VGT_DRAW_INITIATOR>();
  const uint32_t count = initiator.num_indices;
  if (!count) return result;

  const uint16_t* index16 = nullptr;
  const uint32_t* index32 = nullptr;
  uint32_t index_words_available = 0;
  xenos::Endian index_endian = xenos::Endian::kNone;
  if (initiator.source_select == xenos::SourceSelect::kDMA) {
    auto dma_size = regs.Get<reg::VGT_DMA_SIZE>();
    index_endian = dma_size.swap_mode;
    uint32_t base = regs[XE_GPU_REG_VGT_DMA_BASE];
    index_words_available = dma_size.num_words;
    if (initiator.index_size == xenos::IndexFormat::kInt16) {
      if (index_endian == xenos::Endian::k8in32) {
        index_endian = xenos::Endian::k8in16;
      } else if (index_endian == xenos::Endian::k16in32) {
        index_endian = xenos::Endian::kNone;
      }
      index16 = memory.TranslatePhysical<const uint16_t*>(base & ~1u);
    } else {
      index32 = memory.TranslatePhysical<const uint32_t*>(base & ~3u);
    }
  } else if (initiator.source_select == xenos::SourceSelect::kImmediate) {
    if (!immediate_indices) return result;
    index32 = immediate_indices;
    index_words_available = immediate_index_words;
  } else if (initiator.source_select != xenos::SourceSelect::kAutoIndex) {
    return result;
  }

  const uint32_t reset_index = regs.Get<reg::VGT_MULTI_PRIM_IB_RESET_INDX>().reset_indx;
  const bool reset_enabled = regs.Get<reg::PA_SU_SC_MODE_CNTL>().multi_prim_ib_ena;
  const uint32_t index_offset = regs.Get<reg::VGT_INDX_OFFSET>().indx_offset;
  const uint32_t min_index = regs.Get<reg::VGT_MIN_VTX_INDX>().min_indx;
  const uint32_t max_index = regs.Get<reg::VGT_MAX_VTX_INDX>().max_indx;

  ShaderInterpreter interpreter(regs, memory);
  interpreter.SetShader(vs);
  Sink sink(memory, result);
  interpreter.SetExportSink(&sink);
  result.handled = true;

  for (uint32_t i = 0; i < count; ++i) {
    uint32_t vertex_index;
    if (index16 || (index32 && initiator.source_select == xenos::SourceSelect::kDMA)) {
      if (index16) {
        if (i / 2 >= index_words_available) break;
        vertex_index = xenos::GpuSwap(index16[i], index_endian);
      } else {
        if (i >= index_words_available) break;
        vertex_index = xenos::GpuSwap(index32[i], index_endian);
      }
      vertex_index &= 0xFFFFFFu;
      if (reset_enabled && vertex_index == reset_index) continue;
    } else if (index32) {
      if (i >= index_words_available) break;
      vertex_index = index32[i] & 0xFFFFFFu;
    } else {
      vertex_index = i;
    }
    vertex_index = std::min(max_index, std::max(min_index, (vertex_index + index_offset) & 0xFFFFFFu));
    sink.Reset();
    interpreter.temp_registers()[0] = float(vertex_index);
    interpreter.Execute();
    sink.Flush();
    ++result.vertices;
  }
  sink.Apply();
  // Diagnostics: where the shader reads from (vertex fetch ranges).
  for (const auto& binding : vs.vertex_bindings()) {
    const auto vf = regs.GetVertexFetch(binding.fetch_constant);
    const uint32_t a = (uint32_t(vf.address) << 2) & 0x1FFFFFFFu;
    result.src_lo = std::min(result.src_lo, a);
    result.src_hi = std::max(result.src_hi, a + uint32_t(vf.size) * 4);
  }
  return result;
}

}  // namespace p2::memexport
