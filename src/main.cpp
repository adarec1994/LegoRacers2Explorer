#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wincodec.h>
#include <shellapi.h>
#include <GL/gl.h>

#include "generated_paths.h"
#include "generated_blender_addon.h"
#include "ImGuiFileDialog.h"
#include "gtc_archive.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "miniaudio.h"

#include <assimp/Exporter.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr wchar_t kWindowClassName[] = L"LegoRacers2GtcBrowserWindow";
constexpr char kChooseGtcDialogKey[] = "ChooseGtcDialog";
constexpr char kChooseDumpDirectoryDialogKey[] = "ChooseDumpDirectoryDialog";
constexpr char kExportFileDialogKey[] = "ExportFileDialog";
constexpr char kBlenderAddonDialogKey[] = "BlenderAddonDialog";

constexpr const char* ICON_FA_BOX_ARCHIVE = "\xef\x86\x87";
constexpr const char* ICON_FA_BOLT = "\xef\x83\xa7";
constexpr const char* ICON_FA_CHEVRON_RIGHT = "\xef\x81\x94";
constexpr const char* ICON_FA_DOWNLOAD = "\xef\x80\x99";
constexpr const char* ICON_FA_FILE = "\xef\x85\x9b";
constexpr const char* ICON_FA_FILE_LINES = "\xef\x85\x9c";
constexpr const char* ICON_FA_FOLDER = "\xef\x81\xbb";
constexpr const char* ICON_FA_FOLDER_OPEN = "\xef\x81\xbc";
constexpr const char* ICON_FA_MAGNIFYING_GLASS = "\xef\x80\x82";
constexpr const char* ICON_FA_MUSIC = "\xef\x80\x81";
constexpr const char* ICON_FA_PAUSE = "\xef\x81\x8c";
constexpr const char* ICON_FA_PLAY = "\xef\x81\x8b";
constexpr const char* ICON_FA_REPEAT = "\xef\x8d\xa3";
constexpr const char* ICON_FA_STOP = "\xef\x81\x8d";
constexpr const char* ICON_FA_VOLUME_HIGH = "\xef\x80\xa8";

HDC gDeviceContext = nullptr;
HGLRC gOpenGlContext = nullptr;
int gClientWidth = 1;
int gClientHeight = 1;

#ifdef GL_CURRENT_PROGRAM
constexpr GLenum kGlCurrentProgram = GL_CURRENT_PROGRAM;
#else
constexpr GLenum kGlCurrentProgram = 0x8B8D;
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;
#endif
#ifndef GL_CLAMP_TO_EDGE
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
#endif
#ifndef GL_TEXTURE0
constexpr GLenum GL_TEXTURE0 = 0x84C0;
#endif
#ifndef GL_VERTEX_SHADER
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
#endif
#ifndef GL_FRAGMENT_SHADER
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
#endif
#ifndef GL_COMPILE_STATUS
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
#endif
#ifndef GL_LINK_STATUS
constexpr GLenum GL_LINK_STATUS = 0x8B82;
#endif
#ifndef GL_INFO_LOG_LENGTH
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
#endif
constexpr GLenum kGlTextureMaxAnisotropyExt = 0x84FE;
constexpr GLenum kGlMaxTextureMaxAnisotropyExt = 0x84FF;

using GlUseProgramFn = void (APIENTRY*)(GLuint);
using GlCreateShaderFn = GLuint (APIENTRY*)(GLenum);
using GlShaderSourceFn = void (APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using GlCompileShaderFn = void (APIENTRY*)(GLuint);
using GlGetShaderivFn = void (APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetShaderInfoLogFn = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using GlDeleteShaderFn = void (APIENTRY*)(GLuint);
using GlCreateProgramFn = GLuint (APIENTRY*)();
using GlAttachShaderFn = void (APIENTRY*)(GLuint, GLuint);
using GlLinkProgramFn = void (APIENTRY*)(GLuint);
using GlGetProgramivFn = void (APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetProgramInfoLogFn = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using GlDeleteProgramFn = void (APIENTRY*)(GLuint);
using GlGetUniformLocationFn = GLint (APIENTRY*)(GLuint, const char*);
using GlUniform1iFn = void (APIENTRY*)(GLint, GLint);
using GlUniform3fFn = void (APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using GlUniform1fFn = void (APIENTRY*)(GLint, GLfloat);
using GlActiveTextureFn = void (APIENTRY*)(GLenum);

GlUseProgramFn gGlUseProgram = nullptr;
GlCreateShaderFn gGlCreateShader = nullptr;
GlShaderSourceFn gGlShaderSource = nullptr;
GlCompileShaderFn gGlCompileShader = nullptr;
GlGetShaderivFn gGlGetShaderiv = nullptr;
GlGetShaderInfoLogFn gGlGetShaderInfoLog = nullptr;
GlDeleteShaderFn gGlDeleteShader = nullptr;
GlCreateProgramFn gGlCreateProgram = nullptr;
GlAttachShaderFn gGlAttachShader = nullptr;
GlLinkProgramFn gGlLinkProgram = nullptr;
GlGetProgramivFn gGlGetProgramiv = nullptr;
GlGetProgramInfoLogFn gGlGetProgramInfoLog = nullptr;
GlDeleteProgramFn gGlDeleteProgram = nullptr;
GlGetUniformLocationFn gGlGetUniformLocation = nullptr;
GlUniform1iFn gGlUniform1i = nullptr;
GlUniform3fFn gGlUniform3f = nullptr;
GlUniform1fFn gGlUniform1f = nullptr;
GlActiveTextureFn gGlActiveTexture = nullptr;
GLuint gTerrainShaderProgram = 0;
GLuint gBlackTextureId = 0;
GLint gTerrainTextureUniforms[4] = {-1, -1, -1, -1};
GLint gTerrainLightDirUniform = -1;
GLint gTerrainAmbientUniform = -1;
bool gTerrainShaderAttempted = false;

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    if (length == 0) {
        return std::filesystem::current_path();
    }

    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

#include "app/app_types.cpp"

#include "platform/opengl_context.cpp"

#include "core/common.cpp"

#include "app/archive_browser.cpp"

#include "parsers/binary_io.cpp"

#include "audio/audio_decode.cpp"

#include "texture/texture_decode.cpp"

#include "app/archive_io.cpp"

#include "model/model_parse.cpp"

#include "level/level_parse.cpp"

#include "assets/asset_filters.cpp"

#include "render/gl_resources.cpp"

#include "preview/preview_lifecycle.cpp"

#include "export/image_export.cpp"
#include "export/glb_export.cpp"
#include "export/fbx_export.cpp"
#include "export/model_export.cpp"
#include "wrl/export_loaders.cpp"
#include "wrl/heightmap_export.cpp"
#include "wrl/level_export.cpp"
#include "export/export_dialogs.cpp"
#include "plugins/blender_addon.cpp"

#include "ui/browser.cpp"

#include "ui/preview_panels.cpp"

#include "render/model_renderer.cpp"

#include "render/level_renderer.cpp"

#include "ui/model_level_panels.cpp"

#include "ui/about_window.cpp"
#include "ui/main_window.cpp"

LRESULT WINAPI WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            gClientWidth = std::max(1, static_cast<int>(LOWORD(lParam)));
            gClientHeight = std::max(1, static_cast<int>(HIWORD(lParam)));
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int commandShow) {
    ImGui_ImplWin32_EnableDpiAwareness();
    const float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;
    RegisterClassExW(&windowClass);

    HWND hwnd = CreateWindowW(
        kWindowClassName,
        L"LEGO Racers 2 GTC Browser",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        static_cast<int>(1180 * mainScale),
        static_cast<int>(760 * mainScale),
        nullptr,
        nullptr,
        instance,
        nullptr);

    SetupOpenGl(hwnd);
    ShowWindow(hwnd, commandShow);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyAppStyle();
    ImGui::GetStyle().ScaleAllSizes(mainScale);
    ImGui::GetStyle().FontScaleDpi = mainScale;
    LoadFonts();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    AppState state;
    const std::filesystem::path savedGtcPath = LoadSavedGtcPath();
    if (!savedGtcPath.empty()) {
        if (std::filesystem::exists(savedGtcPath)) {
            LoadArchive(state, savedGtcPath);
        } else {
            state.status = "Last GTC not found: " + savedGtcPath.string();
        }
    }

    bool done = false;
    while (!done) {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        BeginOpenGlFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawMainUi(state);

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        const bool minimized = drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f;
        if (!minimized) {
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
            PresentOpenGlFrame();
        }

        try {
            ProcessRetiredPreviewTextures();
        } catch (const std::exception& error) {
        }
    }

    StopAudioPreview(state.audioPreview);
    DestroyModelRenderTextureImmediate(state.modelPreview);
    StopLevelPreview(state.levelPreview);
    DestroyFxPreviewTextureImmediate(state.fxPreview);
    DestroyAllPreviewTextures(state.texturePreview);
    DeleteGlTexture(state.aboutTextureId);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupOpenGl(hwnd);
    DestroyWindow(hwnd);
    UnregisterClassW(kWindowClassName, instance);

    return 0;
}
