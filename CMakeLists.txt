cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0177)
    # Policy CMP0177: install() DESTINATION paths are normalized
    # Introduced in CMake 3.31
    cmake_policy(SET CMP0177 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(identity)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_CLIENT "Build client" ON)
option(BUILD_RENDERER_VULKAN "Build the Vulkan renderer" ON)
option(BUILD_GAME_LIBRARIES "Build game module libraries" ON)
option(BUILD_STANDALONE "Build binaries for standalone games" OFF)

# Changed in MoH Arena: the renderer is linked into the client by default.
#  ON builds renderer_vulkan as a library loaded at run time (cl_renderer),
#  which the renderer's own tools (code/renderervk/tools) need.
option(USE_RENDERER_DLOPEN "Load the renderer as a library at run time" OFF)
if(BUILD_CLIENT AND NOT USE_RENDERER_DLOPEN AND NOT BUILD_RENDERER_VULKAN)
    message(FATAL_ERROR "The client needs a renderer: BUILD_RENDERER_VULKAN, or USE_RENDERER_DLOPEN with a renderer built elsewhere")
endif()

# Changed in MoH Arena: OpenAL is the default sound host (s_backend openal)
option(USE_OPENAL "OpenAL audio" ON)
option(USE_OPENAL_DLOPEN "Dynamically load OpenAL" ON)

option(USE_CODEC_VORBIS "Ogg Vorbis support" ON)
option(USE_CODEC_OPUS "Ogg Opus support" ON)
option(USE_CODEC_MAD "MP3 support" ON)
option(USE_VOIP "Voice chat" OFF)
option(USE_MUMBLE "Mumble support" OFF)
option(USE_DISCORD "Discord Rich Presence" ON)
option(USE_FREETYPE "Freetype font rendering" OFF)

option(USE_INTERNAL_LIBS "Use internally packaged libraries" ON)
option(USE_INTERNAL_SDL "Use internal SDL binary (if available)" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_ZLIB "Use internal copy of zlib" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_JPEG "Use internal copy of libjpeg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_OGG "Use internal copy of ogg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_VORBIS "Use internal copy of vorbis" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_OPUS "Use internal copy of opus" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_MAD "Use internal copy of libmad" ${USE_INTERNAL_LIBS})

# Added in MoH Arena
option(MOHARENA "Build the MoH Arena native bridge" OFF)
# Added in MoH Arena: the modern UI. Off on this branch: the original UI only.
option(MOHARENA_MODERN_UI "Build the modern UI" OFF)
if(MOHARENA_MODERN_UI)
    message(WARNING "MOHARENA_MODERN_UI: the modern UI draws through the GL renderer's UI calls, which the Vulkan renderer doesn't have")
endif()

# Release build by default, set externally if you want something else
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Try to enable LTO by default
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/code)

# Enable unit testing
include(CTest)

# Changed in MoH Arena: the test programs of the modern UI are built only when
# asked for (cmake/tests/all.cmake), so a plain build has the same targets as before.
option(MOHARENA_UI_TESTS "Build the test programs of the modern UI" OFF)

if(DEFINED ENV{SOURCE_DATE_EPOCH})
    string(TIMESTAMP PRODUCT_DATE "%b %d %Y" UTC)
    add_compile_definitions(PRODUCT_DATE="${PRODUCT_DATE}")
endif()

include(compilers/all)
include(platforms/all)
include(libraries/all)
include(deploy/all)

include(version)

include(server)
include(renderer_vulkan)
include(client)
include(basegame)
include(launcher)

include(post_configure)

# Unit tests (CTest's BUILD_TESTING, on by default)
if(BUILD_TESTING)
    include(tests/all)
endif()
