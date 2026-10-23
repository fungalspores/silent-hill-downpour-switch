// CPU memory export (memexport) for draws the native renderer drops.
//
// Silent Hill: Downpour unpacks part of its textures on the GPU: one vertex shader (896985CC...)
// with no rasterization writes 16_16_16_16 elements straight into texture memory (floor and wall
// textures at 168F0000, 168F8000, 173C1000, 173A0000 ...). The native renderer dropped every draw
// with memexport (skip reason 2), so those textures kept every second 8-byte block at zero: the
// lift wall turned into stripes, the corridor floor into coloured smears, characters flickered.
//
// This runs the guest vertex shader on the CPU with the SDK's ShaderInterpreter, once per vertex
// index, and packs and stores eM# the way the SDK's SPIR-V translator does
// (spirv_translator_memexport.cpp): red/blue swap, number format, element packing, 128-bit endian
// swap, element index from eA.y, count limit from eA.w.
#pragma once

#include <cstdint>

#include <rex/graphics/register_file.h>
#include <rex/memory.h>

namespace rex::graphics {
class Shader;
}

namespace p2::memexport {

struct Result {
  bool handled = false;        // the shader could be interpreted
  uint32_t vertices = 0;       // shader invocations
  uint32_t elements = 0;       // eM# stored
  uint32_t lo = UINT32_MAX;    // guest byte range written
  uint32_t hi = 0;
  // Diagnostics: elements whose memory already held non-zero bytes that we changed, and elements
  // that already held exactly what we wrote.
  uint32_t overwrote_nonzero = 0;
  uint32_t same = 0;
  uint32_t src_lo = UINT32_MAX;  // vertex fetch range the shader reads
  uint32_t src_hi = 0;
};

// Runs a memexport vertex shader for the current draw. The shader must be ucode-analyzed.
Result RunVertex(const rex::graphics::RegisterFile& regs, rex::memory::Memory& memory,
                 const rex::graphics::Shader& vertex_shader, const uint32_t* immediate_indices,
                 uint32_t immediate_index_words);

}  // namespace p2::memexport
