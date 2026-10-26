# Silent Hill: Downpour (Xbox 360, 4B4E0823) for Nintendo Switch. Project layout from the Prototype port.
# SDK: masseffect-nx a85b0b4 + penettrator/prototype2-nx SDK patch (Horizon layer by StevensND),
# renderer: prototype2-nx native renderer (nfsmw-nx -> project8-sw -> Mercs2-NX -> Prototype 2).
# GPL-3.0-or-later (the native renderer is GPL-3.0).
cmake_minimum_required(VERSION 3.25)
project(shd VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REXSDK_DIR "" CACHE PATH "ReXGlue SDK source (masseffect-nx Horizon fork + P2 patch)")
if(NOT REXSDK_DIR)
    message(FATAL_ERROR "Set REXSDK_DIR")
endif()
# The presenter's FSR 1 path; gates members of rex/ui/presenter.h, so it is set for every target.
add_compile_definitions(REX_HAS_FIDELITYFX_SDK=1)
add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
# Release: basic FPS integration only (from Prototype 2 NX switch/release_perf.cpp), no SDK profiler polling the
# clock and temperature services (it coincided with Horizon OC's hoc:clk crashing, see ХОД_РАБОТЫ.md).
option(PROTO_RELEASE "Release build without the SDK profiler" ON)
if(PROTO_RELEASE)
    set_source_files_properties("${REXSDK_DIR}/src/ui/switch_perf.cpp" DIRECTORY "${REXSDK_DIR}"
        PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(rexglue_switch_startup PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/switch_release_perf.cpp")
    set_source_files_properties("${REXSDK_DIR}/src/ui/vulkan/vulkan_presenter.cpp" DIRECTORY "${REXSDK_DIR}/src/ui"
        PROPERTIES COMPILE_DEFINITIONS "REX_SWITCH_RELEASE_DIAGNOSTICS=1")
endif()

set(GEN "${CMAKE_CURRENT_SOURCE_DIR}/generated/default")
if(NOT EXISTS "${GEN}/sources.cmake")
    message(FATAL_ERROR "Run rexglue codegen shd_manifest.toml first")
endif()
include("${GEN}/sources.cmake")
set(SHD_GENERATED "")
foreach(src ${GENERATED_SOURCES})
    if(IS_ABSOLUTE "${src}")
        list(APPEND SHD_GENERATED "${src}")
    else()
        list(APPEND SHD_GENERATED "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    endif()
endforeach()
add_library(shd_recomp OBJECT ${SHD_GENERATED})
target_include_directories(shd_recomp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${GEN}")
target_link_libraries(shd_recomp PRIVATE rex::runtime)
rexglue_apply_target_settings(shd_recomp)
target_precompile_headers(shd_recomp PRIVATE "${GEN}/shd_pch.h")
target_compile_options(shd_recomp PRIVATE -fno-tree-slp-vectorize -fno-tree-vectorize)

file(GLOB SHD_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/native/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/native/masseffect/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/native_p2/*.cpp")
add_executable(shd ${SHD_APP_SOURCES})
target_include_directories(shd PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${GEN}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/native/masseffect" "${CMAKE_CURRENT_SOURCE_DIR}/src/native/masseffect/shaders"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/native_p2"
    "${REXSDK_DIR}/src/core" "${REXSDK_DIR}/src/graphics" "${REXSDK_DIR}/src/ui/shaders"
    "${REXSDK_DIR}/thirdparty/glslang" "${REXSDK_DIR}/thirdparty/xxHash")
target_link_libraries(shd PRIVATE shd_recomp rex::runtime)
rexglue_configure_target(shd GPU_PLUGINS xenos)

# SPIR-V initializers of the native renderer's compute/utility shaders (src/native/masseffect/shaders). The
# devkitA64 image has no glslang: tools/build_nro.sh generates them on the host (SPIR-V is platform independent)
# and passes SHD_PREGENERATED_SHADER_DIR.
set(SHD_PREGENERATED_SHADER_DIR "" CACHE PATH "Host-generated SPIR-V initializers (*.inc)")
if(SHD_PREGENERATED_SHADER_DIR)
    set(SHD_GENERATED_SHADER_DIR "${SHD_PREGENERATED_SHADER_DIR}")
else()
    find_program(SHD_GLSLANG_VALIDATOR glslangValidator NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    set(SHD_GENERATED_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated-shaders")
endif()
set(SHD_EDRAM_SHADER_INCLUDES)
foreach(_shader masseffect_edram_7e3_to_rgba8 masseffect_edram_rgba8_to_7e3 masseffect_edram_16f_to_16f me_resolve_exp_bias me_edram_depth_to_rgba8 me_edram_depth_to_16f me_edram_import me_edram_color_to_depth me_edram_depth_to_depth me_edram_depth_to_stencil me_edram_color_to_stencil me_edram_depth_to_depth_msaa2 me_edram_depth_msaa2_to_depth_1x me_edram_depth_to_stencil_msaa2 me_edram_depth_msaa2_to_stencil_1x me_edram_raw64_to_rgba8 me_edram_rgba8_to_raw64 me_edram_raw64_to_16f me_edram_16f_to_raw64 me_edram_raw64_to_raw64 me_edram_depth_to_raw64 me_edram_raw64_to_depth me_depth_resolve_guestspace me_depth_resolve_guestspace_msaa2 me_edram_stencil_to_buffer me_edram_stencil_msaa2_to_buffer me_edram_color_stencil_to_buffer me_edram_color_to_color me_edram_r64_to_r64 me_edram_r16g16_to_rgba8 me_edram_rgba8_to_r16g16 me_edram_r16g16_to_16f me_edram_16f_to_r16g16 me_resolve_7e3_to_unorm10 me_resolve_exp_bias_frag me_resolve_7e3_to_unorm10_frag me_resolve_7e3_word_frag me_resolve_fixed16_frag shd_edram_raw32_to_raw32)
    set(_extension comp)
    if(_shader STREQUAL "me_edram_import")
        set(_extension vert)
    elseif(_shader STREQUAL "me_edram_color_to_depth" OR _shader STREQUAL "me_edram_depth_to_depth" OR _shader STREQUAL "me_edram_depth_to_stencil" OR _shader STREQUAL "me_edram_color_to_stencil" OR _shader STREQUAL "me_edram_depth_to_depth_msaa2" OR _shader STREQUAL "me_edram_depth_msaa2_to_depth_1x" OR _shader STREQUAL "me_edram_depth_to_stencil_msaa2" OR _shader STREQUAL "me_edram_depth_msaa2_to_stencil_1x" OR _shader STREQUAL "me_edram_raw64_to_depth" OR _shader STREQUAL "me_edram_color_to_color" OR _shader STREQUAL "me_edram_r64_to_r64" OR _shader STREQUAL "me_resolve_exp_bias_frag" OR _shader STREQUAL "me_resolve_7e3_to_unorm10_frag" OR _shader STREQUAL "me_resolve_7e3_word_frag" OR _shader STREQUAL "me_resolve_fixed16_frag")
        set(_extension frag)
    endif()
    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/src/native/masseffect/shaders/${_shader}.${_extension}")
    set(_output "${SHD_GENERATED_SHADER_DIR}/${_shader}.inc")
    set(_target_flags)
    if(_shader STREQUAL "me_edram_stencil_to_buffer" OR _shader STREQUAL "me_edram_color_stencil_to_buffer" OR _shader STREQUAL "me_edram_stencil_msaa2_to_buffer" OR _shader STREQUAL "me_edram_depth_to_depth_msaa2" OR _shader STREQUAL "me_edram_depth_msaa2_to_depth_1x" OR _shader STREQUAL "me_edram_depth_to_stencil_msaa2" OR _shader STREQUAL "me_edram_depth_msaa2_to_stencil_1x" OR _shader STREQUAL "me_depth_resolve_guestspace_msaa2")
        set(_target_flags --target-env vulkan1.2)
        set(_output "${SHD_GENERATED_SHADER_DIR}/${_shader}_generated.inc")
    endif()
    if(SHD_PREGENERATED_SHADER_DIR)
        if(NOT EXISTS "${_output}")
            message(FATAL_ERROR "Missing pregenerated shader ${_output}")
        endif()
        list(APPEND SHD_EDRAM_SHADER_INCLUDES "${_output}")
        continue()
    endif()
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHD_GENERATED_SHADER_DIR}"
        COMMAND "${SHD_GLSLANG_VALIDATOR}" -V ${_target_flags} -x -o "${_output}" "${_source}"
        DEPENDS "${_source}" VERBATIM)
    list(APPEND SHD_EDRAM_SHADER_INCLUDES "${_output}")
endforeach()
add_custom_target(shd_edram_shaders DEPENDS ${SHD_EDRAM_SHADER_INCLUDES})
add_dependencies(shd shd_edram_shaders)
target_include_directories(shd PRIVATE "${SHD_GENERATED_SHADER_DIR}")

# CPU-only SDK sources the native renderer reuses (draw extent estimation, shader microcode interpretation).
target_sources(shd PRIVATE
    "${REXSDK_DIR}/src/graphics/register_file.cpp"
    "${REXSDK_DIR}/src/graphics/format/ucode.cpp"
    "${REXSDK_DIR}/src/graphics/pipeline/shader/interpreter.cpp")

set_property(TARGET shd APPEND PROPERTY LINK_DEPENDS "${REXGLUE_SWITCH_NVK_SDK}/lib/libvulkan.a")
set(PROTO_SWITCH_HEAP_MB 2048 CACHE STRING "Capped Switch heap size in MiB")
set_source_files_properties("${REXSDK_DIR}/src/ui/runtime_switch.cpp" DIRECTORY "${REXSDK_DIR}"
    PROPERTIES COMPILE_DEFINITIONS "REX_SWITCH_HEAP_MB=${PROTO_SWITCH_HEAP_MB}")
set(PROTO_SWITCH_HOST_STACK_MB 8 CACHE STRING "Host stack budget per Xbox thread in MiB")
set_source_files_properties("${REXSDK_DIR}/src/system/xthread.cpp" DIRECTORY "${REXSDK_DIR}/src/system"
    PROPERTIES COMPILE_DEFINITIONS "REX_SWITCH_HOST_STACK_MB=${PROTO_SWITCH_HOST_STACK_MB}")
set_source_files_properties("${GEN}/shd_register.cpp"
    PROPERTIES COMPILE_OPTIONS "-O0" SKIP_PRECOMPILE_HEADERS ON)
foreach(t shd shd_recomp)
    target_compile_options(${t} PRIVATE -ffp-contract=off -fno-strict-aliasing)
endforeach()
rexglue_switch_add_nro(shd NAME "Silent Hill: Downpour" AUTHOR "fungalspores" VERSION "0.3.0" ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg")
# (Prototype wrapped bqQueueBuffer for its presenter; the Mass Effect renderer does not.)

# Profile-guided optimization: later, once the game runs (see the Prototype port).
