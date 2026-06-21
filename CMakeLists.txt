cmake_minimum_required(VERSION 4.2)
project(LegoRacers2 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

if(NOT TARGET imgui)
    add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends)
endif()

FetchContent_Declare(
        imgui_file_dialog
        GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR cmake_fetch_only)
FetchContent_MakeAvailable(imgui_file_dialog)

add_library(ImGuiFileDialog STATIC
        ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp
        ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.h
        ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialogConfig.h)
target_include_directories(ImGuiFileDialog PUBLIC ${imgui_file_dialog_SOURCE_DIR})
target_link_libraries(ImGuiFileDialog PUBLIC imgui)
target_compile_definitions(ImGuiFileDialog PUBLIC USE_STD_FILESYSTEM)

FetchContent_Declare(
        fontawesome
        GIT_REPOSITORY https://github.com/FortAwesome/Font-Awesome.git
        GIT_TAG 6.x
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fontawesome)

FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG master
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(miniaudio)

if(NOT TARGET miniaudio)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})
endif()

set(FONT_AWESOME_SOLID_TTF "${fontawesome_SOURCE_DIR}/webfonts/fa-solid-900.ttf")
configure_file(src/generated_paths.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated_paths.h
        @ONLY)

add_library(gtc_core
        src/gtc_archive.cpp
        src/gtc_archive.h)
target_include_directories(gtc_core PUBLIC src)

add_executable(LegoRacers2 WIN32
        src/main.cpp)
target_include_directories(LegoRacers2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(LegoRacers2 PRIVATE UNICODE _UNICODE)
target_link_libraries(LegoRacers2 PRIVATE
        gtc_core
        imgui
        ImGuiFileDialog
        miniaudio
        opengl32
        ole32
        windowscodecs)

add_executable(gtc_list_cli
        tools/gtc_list_cli.cpp)
target_link_libraries(gtc_list_cli PRIVATE gtc_core)
