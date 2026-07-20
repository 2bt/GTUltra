cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


project(GTUltra
    VERSION 1.0.0
    DESCRIPTION "GTUltra - a modernized fork of GoatTracker 2 Stereo"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The original codebase relies on lax conversion rules; keep -fpermissive for
# the C++ translation units so the incremental C -> C++ migration can proceed.
# (No effect on the remaining C sources in asm/; host tools are plain C.)
add_compile_options(-Wall)
# -fpermissive: allow the lax C-style conversions the code relies on.
# -Wno-narrowing: the code initialises char[] tables with values > 127 (byte
#   graphics data), which is a hard error in C++ braced-init but fine in C.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>)

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN}")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Embedded resources (player .s + icon) via CMake HEX → embed catalog
# ---------------------------------------------------------------------------
set(EMBED_INPUTS
    player.s altplayer.s player3.s altplayer3.s
    player9.s altplayer9.s player12.s altplayer12.s
    goat32.png)
list(TRANSFORM EMBED_INPUTS PREPEND "${SRC}/")

set(EMBED_DATA_INC "${GEN}/embed_data.inc")
set(EMBED_LISTFILE "${GEN}/embed_inputs.txt")
file(WRITE "${EMBED_LISTFILE}" "")
foreach(f IN LISTS EMBED_INPUTS)
    file(APPEND "${EMBED_LISTFILE}" "${f}\n")
endforeach()

add_custom_command(
    OUTPUT "${EMBED_DATA_INC}"
    COMMAND ${CMAKE_COMMAND}
        "-DOUT=${EMBED_DATA_INC}"
        "-DLISTFILE=${EMBED_LISTFILE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedResources.cmake"
    DEPENDS ${EMBED_INPUTS}
            "${EMBED_LISTFILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedResources.cmake"
    COMMENT "Embedding resources -> embed_data.inc"
    VERBATIM)

# ---------------------------------------------------------------------------
# Vendored libraries
# ---------------------------------------------------------------------------

# reSID (cycle-exact) and reSID-fp (floating point) SID emulation - C++
add_library(resid STATIC
    "${SRC}/resid/envelope.cpp"
    "${SRC}/resid/extfilt.cpp"
    "${SRC}/resid/filter.cpp"
    "${SRC}/resid/pot.cpp"
    "${SRC}/resid/sid.cpp"
    "${SRC}/resid/wave.cpp"
    "${SRC}/resid/wave6581__ST.cpp"
    "${SRC}/resid/wave6581_P_T.cpp"
    "${SRC}/resid/wave6581_PS_.cpp"
    "${SRC}/resid/wave6581_PST.cpp"
    "${SRC}/resid/wave8580__ST.cpp"
    "${SRC}/resid/wave8580_P_T.cpp"
    "${SRC}/resid/wave8580_PS_.cpp"
    "${SRC}/resid/wave8580_PST.cpp"
    "${SRC}/resid/voice.cpp")
target_include_directories(resid PUBLIC "${SRC}/resid")

add_library(residfp STATIC
    "${SRC}/resid-fp/envelopefp.cpp"
    "${SRC}/resid-fp/extfiltfp.cpp"
    "${SRC}/resid-fp/filterfp.cpp"
    "${SRC}/resid-fp/potfp.cpp"
    "${SRC}/resid-fp/sidfp.cpp"
    "${SRC}/resid-fp/wavefp.cpp"
    "${SRC}/resid-fp/voicefp.cpp")
target_include_directories(residfp PUBLIC "${SRC}/resid-fp")

# 6502 assembler used by the relocator/exporter - kept as C for now
# (slated to be replaced wholesale later, so it is not being ported to C++).
add_library(gtasm STATIC
    "${SRC}/asm/asmtab.c"
    "${SRC}/asm/chnkpool.c"
    "${SRC}/asm/expr.c"
    "${SRC}/asm/lexyy.c"
    "${SRC}/asm/log.c"
    "${SRC}/asm/membuf.c"
    "${SRC}/asm/membufio.c"
    "${SRC}/asm/namedbuf.c"
    "${SRC}/asm/parse.c"
    "${SRC}/asm/pc.c"
    "${SRC}/asm/vec.c")
target_include_directories(gtasm PUBLIC "${SRC}/asm")

# gplatform - SDL2 window / present / input / audio / endian helpers
add_library(gplatform STATIC
    "${SRC}/gwin.cpp"
    "${SRC}/ggfx.cpp"
    "${SRC}/gaudio.cpp"
    "${SRC}/embed.cpp"
    "${SRC}/gendian.cpp")
target_include_directories(gplatform PUBLIC "${SRC}" ${SDL2_INCLUDE_DIRS}
    PRIVATE "${GEN}")
target_link_libraries(gplatform PUBLIC ${SDL2_LIBRARIES} ${ALSA_LIBRARIES} Threads::Threads)
# Ensure the generated include exists before compiling embed.cpp
set_source_files_properties("${SRC}/embed.cpp" PROPERTIES OBJECT_DEPENDS "${EMBED_DATA_INC}")

# ---------------------------------------------------------------------------
# Shared editor core (used by both gtultra and gt2reloc)
#
# greloc and gt2stereo are intentionally NOT here: gt2reloc.c #includes
# greloc.c itself (with GT2RELOC defined), whereas gtultra links greloc
# separately. This mirrors the original makefile exactly.
# ---------------------------------------------------------------------------
add_library(gtcore STATIC
    "${SRC}/gsong.cpp"
    "${SRC}/gorder.cpp"
    "${SRC}/gpattern.cpp"
    "${SRC}/ginstr.cpp"
    "${SRC}/gtable.cpp"
    "${SRC}/gplay.cpp"
    "${SRC}/gdisplay.cpp"
    "${SRC}/gfollow.cpp"
    "${SRC}/gfile.cpp"
    "${SRC}/gsound.cpp"
    "${SRC}/gsid.cpp"
    "${SRC}/ginfo.cpp"
    "${SRC}/gmidi.cpp"
    "${SRC}/gundo.cpp"
    "${SRC}/RtMidi.cpp"
    "${SRC}/gfkeys.cpp"
    "${SRC}/ginput.cpp")
# Project headers are quote-includes under src/; gplatform PUBLIC adds ${SRC}.
target_include_directories(gtcore PUBLIC "${SRC}/asm" ${SDL2_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS})
# RtMidi ALSA backend
target_compile_definitions(gtcore PRIVATE __LINUX_ALSA__)
target_link_libraries(gtcore PUBLIC resid residfp gtasm gplatform ${SDL2_LIBRARIES} ${ALSA_LIBRARIES} Threads::Threads)

# ---------------------------------------------------------------------------
# Dear ImGui (vendored, docking branch) + SDL2 / SDL_Renderer backends.
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp")
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends" ${SDL2_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES})

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------

# The main editor
add_executable(gtultra "${SRC}/gt2stereo.cpp" "${SRC}/greloc.cpp")
target_link_libraries(gtultra PRIVATE gtcore)
target_sources(gtultra PRIVATE
    "${SRC}/gimgui.cpp"
    "${SRC}/guicolors.cpp"
    "${SRC}/guimodel.cpp"
    "${SRC}/gactions.cpp"
    "${SRC}/ghelp.cpp"
    "${SRC}/gfiledialog.cpp"
    "${SRC}/guialert.cpp")
target_include_directories(gtultra PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/portable-file-dialogs")
target_link_libraries(gtultra PRIVATE imgui)
target_include_directories(gtultra PRIVATE "${SRC}/asm" ${SDL2_INCLUDE_DIRS})
# Stage assets next to the binary so runtime code can load them from there.
add_custom_command(TARGET gtultra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:gtultra>/assets")

# Standalone relocator/exporter (includes greloc.cpp with GT2RELOC).
# gplatform calls gimgui_*; provide empty stubs (no ImGui UI here).
add_executable(gt2reloc "${SRC}/gt2reloc.cpp" "${SRC}/gimgui_stubs.cpp")
target_link_libraries(gt2reloc PRIVATE gtcore)

# Small standalone conversion utilities (endian helpers only).
add_executable(mod2sng2 "${SRC}/mod2sng2.cpp" "${SRC}/gendian.cpp")
target_include_directories(mod2sng2 PRIVATE "${SRC}")

add_executable(ins2snd2 "${SRC}/ins2snd2.cpp" "${SRC}/gendian.cpp")
target_include_directories(ins2snd2 PRIVATE "${SRC}")

add_executable(ss2stereo "${SRC}/ss2stereo.cpp" "${SRC}/gendian.cpp")
target_include_directories(ss2stereo PRIVATE "${SRC}")
