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

# ---- Helper: try find_package first (modern config-mode), fall back to pkg-config. ----
macro(nutspice_find_sdl2_component COMPONENT UPPER)
    # COMPONENT = SDL2_image, UPPER = SDL2_image (used for var prefix)
    find_package(${COMPONENT} QUIET)
    if(TARGET ${COMPONENT}::${COMPONENT})
        message(STATUS "${COMPONENT}: found via find_package (config mode)")
    else()
        # Fall back to pkg-config.
        pkg_check_modules(${UPPER} REQUIRED IMPORTED_TARGET ${COMPONENT})
        if(TARGET PkgConfig::${UPPER})
            add_library(${COMPONENT}::${COMPONENT} ALIAS PkgConfig::${UPPER})
            message(STATUS "${COMPONENT}: found via pkg-config")
        endif()
    endif()
endmacro()

find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
    message(STATUS "SDL2: found via find_package (config mode)")
else()
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    if(TARGET PkgConfig::SDL2)
        add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
        message(STATUS "SDL2: found via pkg-config")
    endif()
endif()

nutspice_find_sdl2_component(SDL2_image SDL2_image)
nutspice_find_sdl2_component(SDL2_ttf   SDL2_ttf)
nutspice_find_sdl2_component(SDL2_gfx   SDL2_gfx)

# ---- Cereal (header-only) ---------------------------------------------------
# Cereal may be:
#   (a) bundled inside the project tree  -> ${CMAKE_CURRENT_SOURCE_DIR}/cereal/include
#                                            ${CMAKE_CURRENT_SOURCE_DIR}/include
#                                            ${CMAKE_CURRENT_SOURCE_DIR}/cereal
#                                            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cereal/include
#   (b) installed system-wide            -> libcereal-dev / brew cereal / vcpkg cereal
set(_CEREAL_CANDIDATES
    "${CMAKE_CURRENT_SOURCE_DIR}/cereal/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/cereal"
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

# SDL2 targets may be named SDL2::SDL2 (newer) or just SDL2_INCLUDE_DIRS/LIBS (older)
if(TARGET SDL2::SDL2)
    target_link_libraries(NutSpice PRIVATE SDL2::SDL2 SDL2_image::SDL2_image
                                           SDL2_ttf::SDL2_ttf SDL2_gfx::SDL2_gfx)
else()
    target_include_directories(NutSpice PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(NutSpice PRIVATE ${SDL2_LIBRARIES}
        SDL2_image SDL2_ttf SDL2_gfx)
endif()

# ---- Install (optional) ----------------------------------------------------
install(TARGETS NutSpice RUNTIME DESTINATION bin)
