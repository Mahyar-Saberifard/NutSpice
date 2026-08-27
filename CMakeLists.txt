cmake_minimum_required(VERSION 3.16)
project(NutSpice VERSION 2.0.0 LANGUAGES CXX)

# ---- C++ standard ----------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build type ------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- Dependencies (SDL2, SDL2_image, SDL2_ttf, SDL2_gfx, cereal) -----------
# Linux (Debian/Ubuntu):  sudo apt install libsdl2-dev libsdl2-image-dev
#                          libsdl2-ttf-dev libsdl2-gfx-dev libcereal-dev
# macOS (Homebrew):       brew install sdl2 sdl2_image sdl2_ttf sdl2_gfx cereal
# Windows (vcpkg):        vcpkg install sdl2 sdl2-image sdl2-ttf sdl2-gfx cereal
#                         then pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

include(FindPkgConfig)

# ---- Helper: try find_package (config mode) first; fall back to pkg-config on Unix. ----
macro(nutspice_find_sdl2_component COMPONENT UPPER)
    find_package(${COMPONENT} QUIET)
    if(TARGET ${COMPONENT}::${COMPONENT})
        message(STATUS "${COMPONENT}: found via find_package (config mode)")
    else()
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(${UPPER} REQUIRED IMPORTED_TARGET ${COMPONENT})
            if(TARGET PkgConfig::${UPPER})
                add_library(${COMPONENT}::${COMPONENT} ALIAS PkgConfig::${UPPER})
                message(STATUS "${COMPONENT}: found via pkg-config")
            endif()
        else()
            message(FATAL_ERROR
                    "${COMPONENT} not found.\n"
                    "  - On Windows: install via vcpkg and re-run CMake with\n"
                    "      -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake\n"
                    "  - On macOS:    brew install $(echo ${COMPONENT} | tr A-Z a-z)\n"
                    "  - On Linux:    apt install libsdl2-...-dev (or your distro equivalent)")
        endif()
    endif()
endmacro()

find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
    message(STATUS "SDL2: found via find_package (config mode)")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        if(TARGET PkgConfig::SDL2)
            add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
            message(STATUS "SDL2: found via pkg-config")
        endif()
    else()
        message(FATAL_ERROR "SDL2 not found. Install via vcpkg.")
    endif()
endif()

nutspice_find_sdl2_component(SDL2_image SDL2_image)
nutspice_find_sdl2_component(SDL2_ttf   SDL2_ttf)

# ---- SDL2_gfx special case ------------------------------------------------
find_package(SDL2_gfx QUIET
        CONFIGS sdl2-gfx-config.cmake SDL2_gfxConfig.cmake sdl2_gfx-config.cmake SDL2_gfx-config.cmake)
if(TARGET SDL2_gfx::SDL2_gfx)
    message(STATUS "SDL2_gfx: found via find_package (config mode)")
elseif(TARGET SDL2_gfx)
    # Older vcpkg config sometimes defines plain 'SDL2_gfx' target.
    add_library(SDL2_gfx::SDL2_gfx ALIAS SDL2_gfx)
    message(STATUS "SDL2_gfx: found via find_package (aliasing plain SDL2_gfx target)")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_gfx REQUIRED IMPORTED_TARGET SDL2_gfx)
        if(TARGET PkgConfig::SDL2_gfx)
            add_library(SDL2_gfx::SDL2_gfx ALIAS PkgConfig::SDL2_gfx)
            message(STATUS "SDL2_gfx: found via pkg-config")
        endif()
    else()
        # Last-resort: build imported target manually from vcpkg install dir.
        # VCPKG_INSTALLED_DIR is set by vcpkg's toolchain file when present.
        set(_gfx_inc  "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/SDL2")
        set(_gfx_dll  "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/SDL2_gfx.dll")

        # Lib naming depends on toolchain: MSVC -> SDL2_gfx.lib, MinGW -> libSDL2_gfx.dll.a
        set(_gfx_lib_msvc  "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/SDL2_gfx.lib")
        set(_gfx_lib_mingw "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/libSDL2_gfx.dll.a")
        if(EXISTS "${_gfx_lib_msvc}")
            set(_gfx_lib "${_gfx_lib_msvc}")
        elseif(EXISTS "${_gfx_lib_mingw}")
            set(_gfx_lib "${_gfx_lib_mingw}")
        else()
            set(_gfx_lib "")
        endif()

        if(EXISTS "${_gfx_inc}/SDL2_gfxPrimitives.h" AND _gfx_lib)
            add_library(SDL2_gfx::SDL2_gfx SHARED IMPORTED)
            set_target_properties(SDL2_gfx::SDL2_gfx PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${_gfx_inc}"
                    IMPORTED_IMPLIB               "${_gfx_lib}"
                    IMPORTED_LOCATION             "${_gfx_dll}")
            message(STATUS "SDL2_gfx: found via manual vcpkg path fallback (lib: ${_gfx_lib})")
        else()
            message(FATAL_ERROR
                    "SDL2_gfx not found by any method.\n"
                    "  Looked in: ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}\n"
                    "  Header expected at: ${_gfx_inc}/SDL2_gfxPrimitives.h (exists: $(if(EXISTS \"${_gfx_inc}/SDL2_gfxPrimitives.h\"),yes,no))\n"
                    "  Lib searched: ${_gfx_lib_msvc} and ${_gfx_lib_mingw}\n"
                    "  DLL expected at: ${_gfx_dll}\n"
                    "  Re-run: vcpkg install sdl2-gfx --triplet ${VCPKG_TARGET_TRIPLET}")
        endif()
    endif()
endif()

# ---- Cereal (header-only) ---------------------------------------------------S
set(_CEREAL_CANDIDATES
        "${CMAKE_CURRENT_SOURCE_DIR}/cereal/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cereal/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/cereal/include"
)
find_path(CEREAL_INCLUDE_DIR cereal/archives/binary.hpp
        PATHS ${_CEREAL_CANDIDATES}
        NO_DEFAULT_PATH
)
if(NOT CEREAL_INCLUDE_DIR)
    find_path(CEREAL_INCLUDE_DIR cereal/archives/binary.hpp)
endif()
if(NOT CEREAL_INCLUDE_DIR)
    message(FATAL_ERROR
            "cereal headers not found.\n"
            "Either install system-wide (libcereal-dev / `brew install cereal` / `vcpkg install cereal`),\n"
            "or drop the cereal sources so that <project>/cereal/include/cereal/archives/binary.hpp exists.")
endif()
message(STATUS "Using cereal from: ${CEREAL_INCLUDE_DIR}")

# ---- Sources ---------------------------------------------------------------
set(NUTSPICE_SOURCES
        src/main.cpp
        src/Platform.cpp
        src/Theme.cpp
        src/UI.cpp
        src/Component.cpp
        src/Passives.cpp
        src/Sources.cpp
        src/DependentSources.cpp
        src/Circuit.cpp
        src/Parser.cpp
        src/Renderer.cpp
        src/Placement.cpp
)

add_executable(NutSpice ${NUTSPICE_SOURCES})

target_include_directories(NutSpice PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CEREAL_INCLUDE_DIR}
)

target_link_libraries(NutSpice PRIVATE
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        SDL2_gfx::SDL2_gfx
)

# ---- SDL2main (Windows entry-point shim) -----------------------------------
# On Windows, SDL.h #defines `main` to `SDL_main`. The SDL2main library
# provides the real WinMain that bootstraps SDL and calls SDL_main.
# Without it, the linker errors with "undefined reference to `WinMain'".
if(WIN32)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(NutSpice PRIVATE SDL2::SDL2main)
        message(STATUS "Linking SDL2::SDL2main (target mode)")
    elseif(EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/libSDL2main.a")
        target_link_libraries(NutSpice PRIVATE
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/libSDL2main.a")
        message(STATUS "Linking libSDL2main.a (direct path)")
    elseif(EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/SDL2main.lib")
        target_link_libraries(NutSpice PRIVATE
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/SDL2main.lib")
        message(STATUS "Linking SDL2main.lib (direct path)")
    else()
        message(WARNING
                "SDL2main not found. On Windows the build will fail with\n"
                "  'undefined reference to WinMain'\n"
                "  unless SDL_MAIN_HANDLED is defined before including SDL.h.")
    endif()
endif()

# ---- Install (optional) ----------------------------------------------------
install(TARGETS NutSpice RUNTIME DESTINATION bin)