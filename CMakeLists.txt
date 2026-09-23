cmake_minimum_required(VERSION 3.20)
project(StoryEditor VERSION 1.0.0 LANGUAGES CXX)

# Statische CRT: die fertige .exe laeuft ohne installiertes VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "StoryEditor currently ships a Win32/DirectX11 backend only.")
endif()

include(FetchContent)

# ---------------------------------------------------------------- dependencies
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b-docking
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

set(SE_SINGLE_DIR "${CMAKE_BINARY_DIR}/_deps/single")
file(MAKE_DIRECTORY "${SE_SINGLE_DIR}/nlohmann" "${SE_SINGLE_DIR}/stb")

if(NOT EXISTS "${SE_SINGLE_DIR}/nlohmann/json.hpp")
    message(STATUS "Downloading nlohmann/json ...")
    file(DOWNLOAD
        https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
        "${SE_SINGLE_DIR}/nlohmann/json.hpp"
        TLS_VERIFY ON STATUS _dl_json)
    list(GET _dl_json 0 _dl_json_code)
    if(NOT _dl_json_code EQUAL 0)
        file(REMOVE "${SE_SINGLE_DIR}/nlohmann/json.hpp")
        message(FATAL_ERROR "Could not download nlohmann/json: ${_dl_json}")
    endif()
endif()

if(NOT EXISTS "${SE_SINGLE_DIR}/stb/stb_image.h")
    message(STATUS "Downloading stb_image ...")
    file(DOWNLOAD
        https://raw.githubusercontent.com/nothings/stb/f0569113c93ad095470c54bf34a17b36646bbbb5/stb_image.h
        "${SE_SINGLE_DIR}/stb/stb_image.h"
        TLS_VERIFY ON STATUS _dl_stb)
    list(GET _dl_stb 0 _dl_stb_code)
    if(NOT _dl_stb_code EQUAL 0)
        file(REMOVE "${SE_SINGLE_DIR}/stb/stb_image.h")
        message(FATAL_ERROR "Could not download stb_image: ${_dl_stb}")
    endif()
endif()

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp")
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${imgui_SOURCE_DIR}/misc/cpp")
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS UNICODE _UNICODE)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi)
if(MSVC)
    target_compile_options(imgui PRIVATE /utf-8)
endif()

# ------------------------------------------------------------------ the editor
add_executable(StoryEditor WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/Platform.cpp
    src/app/TextureCache.cpp
    src/app/SpellCheck.cpp
    src/core/StoryTime.cpp
    src/core/Manuscript.cpp
    src/core/DocxExport.cpp
    src/core/Model.cpp
    src/core/Project.cpp
    src/core/VaultIO.cpp
    src/core/FileWatcher.cpp
    src/core/SelfTest.cpp
    src/ui/Lang.cpp
    src/ui/Theme.cpp
    src/ui/UiCommon.cpp
    src/ui/Editor.cpp
    src/ui/Dialogs.cpp
    src/ui/AutoCorrect.cpp
    src/ui/DocumentView.cpp
    src/ui/EditingSelfTest.cpp
    src/ui/Ribbon.cpp
    src/ui/ManuscriptWindow.cpp
    src/ui/TimelineWindow.cpp
    src/ui/GroupManagerWindow.cpp
    src/ui/ActionsWindow.cpp
    src/ui/StoryVisualizerWindow.cpp
    src/ui/ConnectionsWindow.cpp
    src/ui/FileManagerWindow.cpp)

target_include_directories(StoryEditor PRIVATE src "${SE_SINGLE_DIR}")
target_link_libraries(StoryEditor PRIVATE imgui shell32 ole32 shlwapi)
target_compile_definitions(StoryEditor PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    SE_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(StoryEditor PRIVATE /utf-8 /W4 /wd4100 /wd4244 /wd4267 /permissive-)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT StoryEditor)
endif()

set_target_properties(StoryEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
