#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wincodec.h>
#include <GL/gl.h>

#include "generated_paths.h"
#include "ImGuiFileDialog.h"
#include "gtc_archive.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "miniaudio.h"

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
#include <future>
#include <iomanip>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr wchar_t kWindowClassName[] = L"LegoRacers2GtcBrowserWindow";
constexpr char kChooseGtcDialogKey[] = "ChooseGtcDialog";
constexpr char kChooseDumpDirectoryDialogKey[] = "ChooseDumpDirectoryDialog";
constexpr char kExportFileDialogKey[] = "ExportFileDialog";

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
std::ofstream gDebugLog;

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
std::string gLastLoggedModelRenderPath;
std::string gLastLoggedLevelRenderPath;

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

void LogDebug(const std::string& message) {
    if (!gDebugLog) {
        return;
    }

    SYSTEMTIME time;
    GetLocalTime(&time);
    gDebugLog
        << std::setfill('0')
        << std::setw(2) << time.wHour << ':'
        << std::setw(2) << time.wMinute << ':'
        << std::setw(2) << time.wSecond << '.'
        << std::setw(3) << time.wMilliseconds
        << "  " << message << '\n';
    gDebugLog.flush();
}

void InitializeDebugLog() {
    try {
        const auto logPath = ExecutableDirectory() / "debug.log";
        gDebugLog.open(logPath, std::ios::trunc);
        LogDebug("LegoRacers2 started");
        LogDebug("debug log: " + logPath.string());
    } catch (...) {
    }
}

struct ArchiveNode {
    std::string name;
    std::string path;
    bool directory = true;
    int parent = -1;
    std::size_t entryIndex = static_cast<std::size_t>(-1);
    bool externalFile = false;
    std::filesystem::path externalPath;
    std::vector<int> folders;
    std::vector<int> files;
    std::unordered_map<std::string, int> folderLookup;
};

struct ArchiveBrowser {
    std::vector<ArchiveNode> nodes;
    int selectedFolder = 0;
    int selectedItem = -1;
};

struct DumpSnapshot {
    bool active = false;
    bool finished = false;
    bool succeeded = false;
    std::size_t filesWritten = 0;
    std::size_t totalFiles = 0;
    std::string currentPath;
    std::string message;
};

struct DecodedTexture {
    int width = 0;
    int height = 0;
    int bitsPerPixel = 0;
    int mipLevels = 0;
    std::vector<unsigned char> rgba;
};

struct TexturePreview {
    bool open = false;
    bool animated = false;
    bool red = true;
    bool green = true;
    bool blue = true;
    bool alpha = true;
    int frameIndex = 0;
    double lastFrameTime = 0.0;
    std::string name;
    std::string path;
    std::string status;
    DecodedTexture decoded;
    std::vector<DecodedTexture> frames;
    std::vector<int> frameTicks;
    GLuint textureId = 0;
};

struct TextPreview {
    bool open = false;
    std::string name;
    std::string path;
    std::string status;
    std::string content;
};

struct FxPreview {
    bool open = false;
    bool playing = true;
    bool loop = true;
    int frameIndex = 0;
    double lastFrameTime = 0.0;
    float fps = 12.5f;
    std::string name;
    std::string path;
    std::string status;
    DecodedTexture decoded;
    std::vector<DecodedTexture> frames;
    std::vector<std::string> frameNames;
    std::vector<int> frameTicks;
    GLuint textureId = 0;
};

struct DecodedAudio {
    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::uint64_t frameCount = 0;
    std::string format;
    std::vector<float> samples;
};

struct AudioPreview {
    bool open = false;
    std::string name;
    std::string path;
    std::string status;
    DecodedAudio decoded;
    ma_device device = {};
    bool deviceInitialized = false;
    std::atomic<std::uint64_t> cursorFrame = 0;
    std::atomic<bool> playing = false;
    std::atomic<bool> loop = false;
    std::atomic<float> volume = 0.85f;
    float volumeUi = 0.85f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct SkeletonBone {
    int id = 0;
    int parent = -1;
    Quat localRotation;
    Quat bindLocalRotation;
    Vec3 localPosition;
    Quat worldRotation;
    Vec3 worldPosition;
    Vec3 bindWorldPosition;
    Quat bindWorldRotation;
};

struct SkinInfluence {
    int bone = -1;
    float weight = 0.0f;
};

struct ModelVertex {
    Vec3 position;
    Vec3 normal;
    float u = 0.0f;
    float v = 0.0f;
    Vec3 bindPosition;
    Vec3 bindNormal;
    std::array<float, 4> uSets = {};
    std::array<float, 4> vSets = {};
    unsigned char uvSetCount = 1;
    std::array<SkinInfluence, 2> skinInfluences = {};
    int skinInfluenceCount = 0;
    int skinRecord = -1;
};

struct ModelTriangle {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::uint32_t material = 0;
    std::uint32_t section = 0;
};

struct ModelMaterial {
    std::string path;
    DecodedTexture texture;
    bool loaded = false;
    GLuint textureId = 0;
};

struct ModelSection {
    std::string name;
    std::uint32_t textureIndex = 0;
    std::uint32_t materialIndex = 0;
    std::uint32_t alphaType = 0;
    unsigned char textureTiling = 3;
    unsigned char textureCoordinateIndex = 0;
    float alpha = 1.0f;
    std::size_t vertexStart = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleStart = 0;
    std::size_t triangleCount = 0;
    bool visible = true;
};

struct SkinBlendInfluence {
    Vec3 localPosition;
    int bone = -1;
    float weight = 0.0f;
};

struct SkinBlendRecord {
    std::array<SkinBlendInfluence, 2> influences = {};
    int influenceCount = 0;
};

struct SkinLookupGroup {
    std::size_t vertexStart = 0;
    std::size_t vertexCount = 0;
    std::vector<std::uint16_t> recordIndices;
};

struct ModelAnimationFrame {
    Vec3 rootPosition;
    Vec3 rootMotion;
    std::vector<Quat> localRotations;
};

struct ModelAnimationClip {
    std::string name;
    std::string path;
    std::string status;
    bool loaded = false;
    bool loop = false;
    bool rootMotion = false;
    float fps = 15.0f;
    int frameCount = 0;
    int boneCount = 0;
    std::vector<ModelAnimationFrame> frames;
};

struct ModelPreview {
    bool open = false;
    std::string name;
    std::string path;
    std::string status;
    std::vector<ModelVertex> vertices;
    std::vector<ModelVertex> skinnedVertices;
    std::vector<ModelTriangle> triangles;
    std::vector<ModelMaterial> materials;
    std::vector<ModelSection> sections;
    std::vector<SkinBlendRecord> skinRecords;
    std::vector<SkinLookupGroup> skinLookupGroups;
    std::vector<SkeletonBone> skeleton;
    std::vector<std::string> skeletonClips;
    std::vector<ModelAnimationClip> animations;
    Vec3 boundsMin;
    Vec3 boundsMax;
    Vec3 center;
    Vec3 skeletonWorldOffset;
    float radius = 1.0f;
    float yaw = 0.55f;
    float pitch = -0.25f;
    float zoom = 1.0f;
    bool showSkeleton = false;
    bool hasSkeletonWorldOffset = false;
    int selectedAnimation = -1;
    bool animationPlaying = false;
    double animationTime = 0.0;
    double animationLastUpdateTime = 0.0;
    int selectedBone = -1;
    bool rotatingBone = false;
    float boneEditAngle = 0.0f;
    Vec3 boneRotationAxis = {0.0f, 1.0f, 0.0f};
};

struct LevelVertex {
    Vec3 position;
    Vec3 normal;
    float u = 0.0f;
    float v = 0.0f;
    std::array<unsigned char, 4> mix = {255, 0, 0, 0};
    bool visible = true;
};

struct LevelTriangle {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::uint32_t materialKey = 0xffffffffU;
};

struct WorldObject {
    std::string className;
    std::string name;
    std::string binding;
    std::string assetPath;
    std::string modelPath;
    std::vector<std::string> assetPaths;
    std::uint32_t layer = 0;
    std::uint32_t recordSize = 0;
    Vec3 position;
    Quat rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    bool hasLayer = false;
    bool hasPosition = false;
    bool hasRotation = false;
    bool hasScale = false;
};

struct WrlTerrainLink {
    std::string path;
    std::uint32_t layer = 0;
    Vec3 position;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    float textureScaleX = 1.0f;
    float textureScaleY = 1.0f;
    bool hasLayer = false;
};

struct LevelTerrainSection {
    std::string path;
    std::uint32_t layer = 0;
    Vec3 position;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    float textureScaleX = 1.0f;
    float textureScaleY = 1.0f;
    bool hasLayer = false;
    bool show = true;
    std::vector<LevelVertex> vertices;
    std::vector<LevelTriangle> triangles;
    DecodedTexture texture;
    GLuint textureId = 0;
    std::array<GLuint, 256> layerTextureIds = {};
    std::array<std::string, 256> layerTexturePaths = {};
    GLuint displayListTextured = 0;
    GLuint displayListColored = 0;
    std::string texturePath;
};

struct LevelModelAsset {
    std::string path;
    std::string status;
    ModelPreview model;
    GLuint displayList = 0;
    GLuint skyboxDisplayList = 0;
    bool loaded = false;
};

struct LevelModelInstance {
    int objectIndex = -1;
    int assetIndex = -1;
    Vec3 position;
    Quat rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct LevelWaterSheet {
    int objectIndex = -1;
    std::string texturePath;
    std::string reflectionTexturePath;
    Vec3 position;
    Quat rotation;
    float width = 128.0f;
    float depth = 128.0f;
    float uScale = 4.0f;
    float vScale = 4.0f;
    float alpha = 0.74f;
    GLuint textureId = 0;
    GLuint displayList = 0;
};

struct LevelPreview {
    bool open = false;
    bool world = false;
    std::string name;
    std::string path;
    std::string status;
    std::string terrainPath;
    std::vector<WrlTerrainLink> terrainLinks;
    std::vector<LevelTerrainSection> terrainSections;
    std::vector<LevelVertex> vertices;
    std::vector<LevelTriangle> triangles;
    std::vector<WorldObject> objects;
    std::vector<LevelModelAsset> modelAssets;
    std::vector<LevelModelInstance> modelInstances;
    std::vector<LevelWaterSheet> waterSheets;
    std::vector<int> skyboxAssetIndices;
    std::vector<std::string> skyboxPaths;
    std::unordered_map<std::string, int> classCounts;
    DecodedTexture terrainTexture;
    GLuint terrainTextureId = 0;
    std::string terrainTexturePath;
    Vec3 boundsMin;
    Vec3 boundsMax;
    Vec3 center;
    Vec3 flyCameraPosition;
    float radius = 1.0f;
    float yaw = 0.65f;
    float pitch = -0.55f;
    float zoom = 1.0f;
    float flySpeed = 1.0f;
    bool showTerrain = true;
    bool showTerrainTexture = true;
    bool showObjects = true;
    bool showSkybox = true;
    bool showWater = true;
    bool showObjectMarkers = false;
    bool showCheckpoints = false;
    bool showStartPositions = false;
    bool showWeaponPickups = false;
    bool showAiObjects = false;
    bool flyCameraInitialized = false;
    bool mouseLookLocked = false;
    int mouseLookLockX = 0;
    int mouseLookLockY = 0;
    int selectedTerrainSection = -1;
    int selectedObject = -1;
    bool scrollSelectedObjectIntoView = false;
};

struct RetiredGlTexture {
    GLuint textureId = 0;
    int framesUntilDestroy = 3;
};

std::vector<RetiredGlTexture> gRetiredPreviewTextures;

enum class AssetFilter {
    All,
    Textures,
    Models,
    Levels,
    Fx,
    Audio,
};

struct AppState {
    gtc::ArchiveInfo archive;
    ArchiveBrowser browser;
    bool archiveLoaded = false;
    bool archiveDataLoaded = false;
    std::string selectedPath;
    std::string status = "Waiting for GAMEDATA.GTC.";
    std::vector<char> archiveData;
    std::mutex dumpMutex;
    bool dumpActive = false;
    bool dumpFinished = false;
    bool dumpSucceeded = false;
    std::size_t dumpFilesWritten = 0;
    std::size_t dumpTotalFiles = 0;
    std::string dumpCurrentPath;
    std::string dumpMessage;
    std::future<void> dumpFuture;
    std::array<char, 256> searchText = {};
    AssetFilter assetFilter = AssetFilter::All;
    float bottomPanelHeight = 260.0f;
    TexturePreview texturePreview;
    TextPreview textPreview;
    FxPreview fxPreview;
    ModelPreview modelPreview;
    LevelPreview levelPreview;
    AudioPreview audioPreview;
};

void DestroyModelRenderTexture(ModelPreview& preview);
void DestroyPreviewTexture(TexturePreview& preview);
void DestroyFxPreviewTextureImmediate(FxPreview& preview);
void StopFxPreview(FxPreview& preview);
void StopAudioPreview(AudioPreview& preview);
void StopLevelPreview(LevelPreview& preview);
bool HasLevelTerrainTexture(const LevelPreview& preview);
std::string LevelTerrainTextureSummary(const LevelPreview& preview);
void DeleteGlTexture(GLuint& textureId);
void DeleteGlDisplayList(GLuint& displayList);
GLuint CreateGlTextureRgba(int width,
                           int height,
                           const unsigned char* pixels,
                           bool repeat,
                           bool linear,
                           bool mipmaps = true);
ModelPreview DecodeBsbSkeleton(const std::vector<char>& bytes);
ModelAnimationClip DecodeBsaAnimation(const std::vector<char>& bytes, std::string name, std::string path);
void Sanitize(Vec3& value);
void ApplySkeletonAlignment(ModelPreview& preview);
bool ApplySkinDerivedSkeletonAlignment(ModelPreview& preview);
void BuildGeneratedSkinWeights(ModelPreview& preview);


void SetupOpenGl(HWND hwnd) {
    gDeviceContext = GetDC(hwnd);
    if (gDeviceContext == nullptr) {
        throw std::runtime_error("Could not get a window device context.");
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat(gDeviceContext, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(gDeviceContext, pixelFormat, &pfd)) {
        throw std::runtime_error("Could not set an OpenGL pixel format.");
    }

    gOpenGlContext = wglCreateContext(gDeviceContext);
    if (gOpenGlContext == nullptr || !wglMakeCurrent(gDeviceContext, gOpenGlContext)) {
        throw std::runtime_error("Could not create an OpenGL context.");
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);
    gClientWidth = std::max(1L, rect.right - rect.left);
    gClientHeight = std::max(1L, rect.bottom - rect.top);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    LogDebug(std::string("OpenGL version: ") + (version != nullptr ? version : "unknown"));

    gGlUseProgram = reinterpret_cast<GlUseProgramFn>(wglGetProcAddress("glUseProgram"));
    gGlCreateShader = reinterpret_cast<GlCreateShaderFn>(wglGetProcAddress("glCreateShader"));
    gGlShaderSource = reinterpret_cast<GlShaderSourceFn>(wglGetProcAddress("glShaderSource"));
    gGlCompileShader = reinterpret_cast<GlCompileShaderFn>(wglGetProcAddress("glCompileShader"));
    gGlGetShaderiv = reinterpret_cast<GlGetShaderivFn>(wglGetProcAddress("glGetShaderiv"));
    gGlGetShaderInfoLog = reinterpret_cast<GlGetShaderInfoLogFn>(wglGetProcAddress("glGetShaderInfoLog"));
    gGlDeleteShader = reinterpret_cast<GlDeleteShaderFn>(wglGetProcAddress("glDeleteShader"));
    gGlCreateProgram = reinterpret_cast<GlCreateProgramFn>(wglGetProcAddress("glCreateProgram"));
    gGlAttachShader = reinterpret_cast<GlAttachShaderFn>(wglGetProcAddress("glAttachShader"));
    gGlLinkProgram = reinterpret_cast<GlLinkProgramFn>(wglGetProcAddress("glLinkProgram"));
    gGlGetProgramiv = reinterpret_cast<GlGetProgramivFn>(wglGetProcAddress("glGetProgramiv"));
    gGlGetProgramInfoLog = reinterpret_cast<GlGetProgramInfoLogFn>(wglGetProcAddress("glGetProgramInfoLog"));
    gGlDeleteProgram = reinterpret_cast<GlDeleteProgramFn>(wglGetProcAddress("glDeleteProgram"));
    gGlGetUniformLocation = reinterpret_cast<GlGetUniformLocationFn>(wglGetProcAddress("glGetUniformLocation"));
    gGlUniform1i = reinterpret_cast<GlUniform1iFn>(wglGetProcAddress("glUniform1i"));
    gGlUniform3f = reinterpret_cast<GlUniform3fFn>(wglGetProcAddress("glUniform3f"));
    gGlUniform1f = reinterpret_cast<GlUniform1fFn>(wglGetProcAddress("glUniform1f"));
    gGlActiveTexture = reinterpret_cast<GlActiveTextureFn>(wglGetProcAddress("glActiveTexture"));
    const bool shaderApiLoaded =
        gGlUseProgram != nullptr &&
        gGlCreateShader != nullptr &&
        gGlShaderSource != nullptr &&
        gGlCompileShader != nullptr &&
        gGlGetShaderiv != nullptr &&
        gGlGetShaderInfoLog != nullptr &&
        gGlDeleteShader != nullptr &&
        gGlCreateProgram != nullptr &&
        gGlAttachShader != nullptr &&
        gGlLinkProgram != nullptr &&
        gGlGetProgramiv != nullptr &&
        gGlGetProgramInfoLog != nullptr &&
        gGlDeleteProgram != nullptr &&
        gGlGetUniformLocation != nullptr &&
        gGlUniform1i != nullptr &&
        gGlUniform3f != nullptr &&
        gGlUniform1f != nullptr &&
        gGlActiveTexture != nullptr;
    LogDebug(std::string("OpenGL shader API: ") + (shaderApiLoaded ? "loaded" : "unavailable"));
}

void CleanupOpenGl(HWND hwnd) {
    if (gTerrainShaderProgram != 0 && gGlDeleteProgram != nullptr) {
        gGlDeleteProgram(gTerrainShaderProgram);
        gTerrainShaderProgram = 0;
    }
    if (gBlackTextureId != 0) {
        glDeleteTextures(1, &gBlackTextureId);
        gBlackTextureId = 0;
    }
    if (gOpenGlContext != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gOpenGlContext);
        gOpenGlContext = nullptr;
    }
    if (gDeviceContext != nullptr) {
        ReleaseDC(hwnd, gDeviceContext);
        gDeviceContext = nullptr;
    }
}

void BeginOpenGlFrame() {
    glViewport(0, 0, gClientWidth, gClientHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.085f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PresentOpenGlFrame() {
    SwapBuffers(gDeviceContext);
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

Vec3 Add(Vec3 lhs, Vec3 rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 Subtract(Vec3 lhs, Vec3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 Cross(Vec3 lhs, Vec3 rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float Dot(Vec3 lhs, Vec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(Vec3 value) {
    return std::sqrt(Dot(value, value));
}

Vec3 Normalize(Vec3 value) {
    const float length = Length(value);
    if (length <= 0.00001f || !std::isfinite(length)) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 Multiply(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float DistanceSquared(Vec3 lhs, Vec3 rhs) {
    const Vec3 delta = Subtract(lhs, rhs);
    return Dot(delta, delta);
}

Quat Normalize(Quat value) {
    const float length = std::sqrt(
        value.x * value.x +
        value.y * value.y +
        value.z * value.z +
        value.w * value.w);
    if (length <= 0.00001f || !std::isfinite(length)) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length, value.w / length};
}

Quat Conjugate(Quat value) {
    return {-value.x, -value.y, -value.z, value.w};
}

Quat Inverse(Quat value) {
    return Conjugate(Normalize(value));
}

Quat QuatFromAxisAngle(Vec3 axis, float angle) {
    axis = Normalize(axis);
    const float halfAngle = angle * 0.5f;
    const float sine = std::sin(halfAngle);
    return Normalize({axis.x * sine, axis.y * sine, axis.z * sine, std::cos(halfAngle)});
}

Quat Multiply(Quat lhs, Quat rhs) {
    return Normalize({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    });
}

Quat Slerp(Quat start, Quat end, float t) {
    start = Normalize(start);
    end = Normalize(end);
    t = std::clamp(t, 0.0f, 1.0f);

    float dot =
        start.x * end.x +
        start.y * end.y +
        start.z * end.z +
        start.w * end.w;
    if (dot < 0.0f) {
        dot = -dot;
        end = {-end.x, -end.y, -end.z, -end.w};
    }

    if (dot > 0.9995f) {
        return Normalize({
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t,
            start.w + (end.w - start.w) * t,
        });
    }

    const float theta0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float theta = theta0 * t;
    const float sinTheta = std::sin(theta);
    const float sinTheta0 = std::sin(theta0);
    if (std::abs(sinTheta0) <= 0.00001f) {
        return start;
    }

    const float scaleStart = std::cos(theta) - dot * sinTheta / sinTheta0;
    const float scaleEnd = sinTheta / sinTheta0;
    return Normalize({
        start.x * scaleStart + end.x * scaleEnd,
        start.y * scaleStart + end.y * scaleEnd,
        start.z * scaleStart + end.z * scaleEnd,
        start.w * scaleStart + end.w * scaleEnd,
    });
}

Vec3 Rotate(Quat rotation, Vec3 value) {
    rotation = Normalize(rotation);
    const Vec3 q{rotation.x, rotation.y, rotation.z};
    const Vec3 t = {
        2.0f * (q.y * value.z - q.z * value.y),
        2.0f * (q.z * value.x - q.x * value.z),
        2.0f * (q.x * value.y - q.y * value.x),
    };
    return {
        value.x + rotation.w * t.x + (q.y * t.z - q.z * t.y),
        value.y + rotation.w * t.y + (q.z * t.x - q.x * t.z),
        value.z + rotation.w * t.z + (q.x * t.y - q.y * t.x),
    };
}

float GetAxis(Vec3 value, int axis) {
    switch (axis) {
    case 0:
        return value.x;
    case 1:
        return value.y;
    case 2:
    default:
        return value.z;
    }
}

void SetAxis(Vec3& value, int axis, float component) {
    switch (axis) {
    case 0:
        value.x = component;
        break;
    case 1:
        value.y = component;
        break;
    case 2:
    default:
        value.z = component;
        break;
    }
}

float DistancePointSegmentSquared(Vec3 point, Vec3 start, Vec3 end) {
    const Vec3 segment = Subtract(end, start);
    const float segmentLengthSquared = Dot(segment, segment);
    if (segmentLengthSquared <= 0.000001f) {
        return DistanceSquared(point, start);
    }

    const float t = std::clamp(Dot(Subtract(point, start), segment) / segmentLengthSquared, 0.0f, 1.0f);
    const Vec3 closest = Add(start, Multiply(segment, t));
    return DistanceSquared(point, closest);
}

std::vector<std::string> SplitArchivePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    std::vector<std::string> parts;
    std::string current;
    for (const char character : path) {
        if (character == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(character);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::string JoinArchivePath(const std::string& parent, const std::string& name) {
    if (parent.empty()) {
        return name;
    }
    return parent + "\\" + name;
}

std::string SettingsFilePathString();

std::string PreferredInitialDirectory() {
    std::ifstream settings(SettingsFilePathString());
    std::string savedPath;
    std::getline(settings, savedPath);
    const std::filesystem::path saved(savedPath);
    if (!saved.empty() && saved.has_parent_path() && std::filesystem::exists(saved.parent_path())) {
        return saved.parent_path().string();
    }

    const std::filesystem::path lr2 = L"C:\\Users\\pwd12\\OneDrive\\Documents\\lr2";
    if (std::filesystem::exists(lr2)) {
        return lr2.string();
    }
    return std::filesystem::current_path().string();
}

std::filesystem::path SettingsFilePath() {
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr && appData[0] != '\0') {
        return std::filesystem::path(appData) / "LegoRacers2GtcBrowser" / "settings.txt";
    }
    return std::filesystem::current_path() / "LegoRacers2GtcBrowser.settings.txt";
}

std::string SettingsFilePathString() {
    return SettingsFilePath().string();
}

std::string LoadSavedGtcPath() {
    std::ifstream file(SettingsFilePath());
    std::string path;
    std::getline(file, path);
    return path;
}

void SaveSavedGtcPath(const std::filesystem::path& path) {
    try {
        const auto settingsPath = SettingsFilePath();
        if (settingsPath.has_parent_path()) {
            std::filesystem::create_directories(settingsPath.parent_path());
        }

        std::ofstream file(settingsPath, std::ios::trunc);
        if (file) {
            file << path.string();
        }
    } catch (...) {
    }
}

bool NodeNameLess(const ArchiveBrowser& browser, int left, int right) {
    return ToLower(browser.nodes[left].name) < ToLower(browser.nodes[right].name);
}

void SortBrowserNode(ArchiveBrowser& browser, int nodeIndex) {
    auto& node = browser.nodes[nodeIndex];
    std::sort(node.folders.begin(), node.folders.end(), [&](int left, int right) {
        return NodeNameLess(browser, left, right);
    });
    std::sort(node.files.begin(), node.files.end(), [&](int left, int right) {
        return NodeNameLess(browser, left, right);
    });
    const auto childFolders = node.folders;
    for (const int folderIndex : childFolders) {
        SortBrowserNode(browser, folderIndex);
    }
}

int EnsureBrowserFolder(ArchiveBrowser& browser, int parentIndex, const std::string& name) {
    ArchiveNode& parent = browser.nodes[parentIndex];
    const std::string key = ToLower(name);
    const auto found = parent.folderLookup.find(key);
    if (found != parent.folderLookup.end()) {
        return found->second;
    }

    ArchiveNode folder;
    folder.name = name;
    folder.path = JoinArchivePath(parent.path, folder.name);
    folder.directory = true;
    folder.parent = parentIndex;

    const int folderIndex = static_cast<int>(browser.nodes.size());
    browser.nodes.push_back(std::move(folder));
    browser.nodes[parentIndex].folders.push_back(folderIndex);
    browser.nodes[parentIndex].folderLookup[key] = folderIndex;
    return folderIndex;
}

bool IsMusicTrackExtension(const std::string& extension) {
    return extension == ".1" ||
           extension == ".2" ||
           extension == ".3" ||
           extension == ".4" ||
           extension == ".5";
}

std::filesystem::path MusicDirectoryForArchive(const gtc::ArchiveInfo& archive) {
    return archive.gtcPath.parent_path() / "game data" / "music";
}

void AddExternalMusicFiles(AppState& state, ArchiveBrowser& browser) {
    const std::filesystem::path musicDirectory = MusicDirectoryForArchive(state.archive);
    if (!std::filesystem::exists(musicDirectory) || !std::filesystem::is_directory(musicDirectory)) {
        LogDebug("music folder not found: " + musicDirectory.string());
        return;
    }

    std::vector<std::filesystem::path> musicFiles;
    for (const auto& entry : std::filesystem::directory_iterator(musicDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsMusicTrackExtension(ToLower(entry.path().extension().string()))) {
            musicFiles.push_back(entry.path());
        }
    }

    std::sort(musicFiles.begin(), musicFiles.end(), [](const auto& left, const auto& right) {
        return ToLower(left.filename().string()) < ToLower(right.filename().string());
    });

    if (musicFiles.empty()) {
        LogDebug("music folder has no supported tracks: " + musicDirectory.string());
        return;
    }

    const int gameDataFolder = EnsureBrowserFolder(browser, 0, "GAME DATA");
    const int musicFolder = EnsureBrowserFolder(browser, gameDataFolder, "MUSIC");
    for (const std::filesystem::path& path : musicFiles) {
        ArchiveNode file;
        file.name = path.filename().string();
        file.path = JoinArchivePath(browser.nodes[musicFolder].path, file.name);
        file.directory = false;
        file.parent = musicFolder;
        file.externalFile = true;
        file.externalPath = path;

        const int fileIndex = static_cast<int>(browser.nodes.size());
        browser.nodes.push_back(std::move(file));
        browser.nodes[musicFolder].files.push_back(fileIndex);
    }

    LogDebug(
        "music folder loaded: " + musicDirectory.string() +
        " tracks=" + std::to_string(musicFiles.size()));
}

void BuildBrowser(AppState& state) {
    ArchiveBrowser browser;
    ArchiveNode root;
    root.name = state.archive.gtcPath.filename().string();
    if (root.name.empty()) {
        root.name = "GAMEDATA.GTC";
    }
    root.path.clear();
    root.directory = true;
    root.parent = -1;
    browser.nodes.push_back(std::move(root));

    for (std::size_t entryIndex = 0; entryIndex < state.archive.entries.size(); ++entryIndex) {
        const auto parts = SplitArchivePath(state.archive.entries[entryIndex].path);
        if (parts.empty()) {
            continue;
        }

        int current = 0;
        for (std::size_t partIndex = 0; partIndex + 1 < parts.size(); ++partIndex) {
            const std::string key = ToLower(parts[partIndex]);
            auto found = browser.nodes[current].folderLookup.find(key);
            if (found != browser.nodes[current].folderLookup.end()) {
                current = found->second;
                continue;
            }

            ArchiveNode folder;
            folder.name = parts[partIndex];
            folder.path = JoinArchivePath(browser.nodes[current].path, folder.name);
            folder.directory = true;
            folder.parent = current;

            const int folderIndex = static_cast<int>(browser.nodes.size());
            browser.nodes.push_back(std::move(folder));
            browser.nodes[current].folders.push_back(folderIndex);
            browser.nodes[current].folderLookup[key] = folderIndex;
            current = folderIndex;
        }

        ArchiveNode file;
        file.name = parts.back();
        file.path = JoinArchivePath(browser.nodes[current].path, file.name);
        file.directory = false;
        file.parent = current;
        file.entryIndex = entryIndex;

        const int fileIndex = static_cast<int>(browser.nodes.size());
        browser.nodes.push_back(std::move(file));
        browser.nodes[current].files.push_back(fileIndex);
    }

    AddExternalMusicFiles(state, browser);

    SortBrowserNode(browser, 0);
    browser.selectedFolder = 0;
    browser.selectedItem = 0;
    state.browser = std::move(browser);
}

void RebuildVisibleEntries(AppState&) {
}

void LoadArchive(AppState& state, const std::filesystem::path& path) {
    try {
        LogDebug("load archive: " + path.string());
        state.archive = gtc::LoadArchive(path);
        state.archiveLoaded = true;
        state.archiveDataLoaded = false;
        state.archiveData.clear();
        StopAudioPreview(state.audioPreview);
        StopFxPreview(state.fxPreview);
        DestroyPreviewTexture(state.texturePreview);
        state.texturePreview.open = false;
        state.textPreview = {};
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = {};
        StopLevelPreview(state.levelPreview);
        state.selectedPath = state.archive.gtcPath.string();
        state.status = "Loaded " + state.selectedPath;
        BuildBrowser(state);
        SaveSavedGtcPath(state.archive.gtcPath);
        LogDebug("archive loaded: " + state.selectedPath + " entries=" + std::to_string(state.archive.entries.size()));
    } catch (const std::exception& error) {
        LogDebug(std::string("archive load failed: ") + error.what());
        state.archiveLoaded = false;
        state.archiveDataLoaded = false;
        state.archiveData.clear();
        state.browser = {};
        StopAudioPreview(state.audioPreview);
        StopFxPreview(state.fxPreview);
        DestroyPreviewTexture(state.texturePreview);
        state.texturePreview.open = false;
        state.textPreview = {};
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = {};
        StopLevelPreview(state.levelPreview);
        state.status = std::string("Load failed: ") + error.what();
    }
}

DumpSnapshot GetDumpSnapshot(AppState& state) {
    std::lock_guard lock(state.dumpMutex);
    return {
        state.dumpActive,
        state.dumpFinished,
        state.dumpSucceeded,
        state.dumpFilesWritten,
        state.dumpTotalFiles,
        state.dumpCurrentPath,
        state.dumpMessage,
    };
}

bool IsDumpActive(AppState& state) {
    std::lock_guard lock(state.dumpMutex);
    return state.dumpActive;
}

void PollDumpTask(AppState& state) {
    if (state.dumpFuture.valid() &&
        state.dumpFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        state.dumpFuture.get();

        std::lock_guard lock(state.dumpMutex);
        state.dumpActive = false;
        state.status = state.dumpMessage;
    }
}

void StartDump(AppState& state, const std::filesystem::path& outputDirectory) {
    if (!state.archiveLoaded) {
        state.status = "Load a GTC archive before dumping.";
        return;
    }

    {
        std::lock_guard lock(state.dumpMutex);
        if (state.dumpActive) {
            return;
        }

        state.dumpActive = true;
        state.dumpFinished = false;
        state.dumpSucceeded = false;
        state.dumpFilesWritten = 0;
        state.dumpTotalFiles = state.archive.entries.size();
        state.dumpCurrentPath.clear();
        state.dumpMessage = "Starting dump to " + outputDirectory.string();
    }

    const gtc::ArchiveInfo archive = state.archive;
    state.dumpFuture = std::async(std::launch::async, [&state, archive, outputDirectory]() {
        try {
            gtc::DumpArchive(archive, outputDirectory, [&state](const gtc::DumpProgress& progress) {
                std::lock_guard lock(state.dumpMutex);
                state.dumpFilesWritten = progress.filesWritten;
                state.dumpTotalFiles = progress.totalFiles;
                state.dumpCurrentPath = progress.currentPath.string();
                state.dumpMessage = progress.message.empty() ? "Dumping" : progress.message;
            });

            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = true;
            state.dumpMessage = "Dumped " + std::to_string(archive.entries.size()) +
                                " files to " + outputDirectory.string();
        } catch (const std::exception& error) {
            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = false;
            state.dumpMessage = std::string("Dump failed: ") + error.what();
        } catch (...) {
            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = false;
            state.dumpMessage = "Dump failed: unknown error.";
        }
    });
}

void OpenGtcDialog() {
    IGFD::FileDialogConfig config;
    config.path = PreferredInitialDirectory();
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ReadOnlyFileNameField |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kChooseGtcDialogKey,
        "Open GAMEDATA.GTC",
        ".gtc",
        config);
}

void OpenDumpDirectoryDialog(const AppState& state) {
    IGFD::FileDialogConfig config;
    config.path = state.archiveLoaded && state.archive.gtcPath.has_parent_path()
                      ? state.archive.gtcPath.parent_path().string()
                      : PreferredInitialDirectory();
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal;

    ImGuiFileDialog::Instance()->OpenDialog(
        kChooseDumpDirectoryDialogKey,
        "Dump All To Folder",
        nullptr,
        config);
}

std::uint16_t ReadU16Le(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Texture read exceeded the file size.");
    }

    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(bytes[offset]) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 8));
}

std::int16_t ReadS16Le(const std::vector<char>& bytes, std::size_t offset) {
    return static_cast<std::int16_t>(ReadU16Le(bytes, offset));
}

std::uint32_t ReadU32Le(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Texture read exceeded the file size.");
    }

    return static_cast<std::uint32_t>(
        static_cast<unsigned char>(bytes[offset]) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 8) |
        (static_cast<unsigned char>(bytes[offset + 2]) << 16) |
        (static_cast<unsigned char>(bytes[offset + 3]) << 24));
}

float ReadF32Le(const std::vector<char>& bytes, std::size_t offset) {
    const std::uint32_t packed = ReadU32Le(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &packed, sizeof(value));
    return value;
}

std::uint16_t ReadU16Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Audio read exceeded the file size.");
    }

    return static_cast<std::uint16_t>(
        (static_cast<unsigned char>(bytes[offset]) << 8) |
        static_cast<unsigned char>(bytes[offset + 1]));
}

std::uint32_t ReadU32Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Audio read exceeded the file size.");
    }

    return static_cast<std::uint32_t>(
        (static_cast<unsigned char>(bytes[offset]) << 24) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 16) |
        (static_cast<unsigned char>(bytes[offset + 2]) << 8) |
        static_cast<unsigned char>(bytes[offset + 3]));
}

double ReadExtended80Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 10 > bytes.size()) {
        throw std::runtime_error("AIFF sample rate is truncated.");
    }

    const std::uint16_t exponentBits = ReadU16Be(bytes, offset);
    const bool negative = (exponentBits & 0x8000U) != 0;
    const int exponent = static_cast<int>(exponentBits & 0x7fffU);
    const std::uint64_t high = ReadU32Be(bytes, offset + 2);
    const std::uint64_t low = ReadU32Be(bytes, offset + 6);
    const std::uint64_t mantissa = (high << 32) | low;
    if (exponent == 0 && mantissa == 0) {
        return 0.0;
    }

    const long double value = std::ldexp(static_cast<long double>(mantissa), exponent - 16383 - 63);
    return static_cast<double>(negative ? -value : value);
}

float DecodeSignedPcmSampleBe(const std::vector<char>& bytes, std::size_t offset, int bitsPerSample) {
    switch (bitsPerSample) {
    case 8: {
        const auto value = static_cast<std::int8_t>(bytes[offset]);
        return static_cast<float>(value) / 128.0f;
    }
    case 16: {
        const auto value = static_cast<std::int16_t>(ReadU16Be(bytes, offset));
        return static_cast<float>(value) / 32768.0f;
    }
    case 24: {
        std::int32_t value =
            (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 0])) << 16) |
            (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
            static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 2]));
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xff000000);
        }
        return static_cast<float>(value) / 8388608.0f;
    }
    case 32: {
        const auto value = static_cast<std::int32_t>(ReadU32Be(bytes, offset));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    default:
        throw std::runtime_error("Unsupported AIFF bit depth: " + std::to_string(bitsPerSample));
    }
}

DecodedAudio DecodeAiffAudio(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "FORM", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "AIFF", 4) != 0) {
        throw std::runtime_error("Not a supported AIFF audio file.");
    }

    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::uint64_t declaredFrames = 0;
    std::size_t soundOffset = static_cast<std::size_t>(-1);
    std::size_t soundBytes = 0;

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const char* chunk = bytes.data() + offset;
        const std::uint32_t chunkSize = ReadU32Be(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            throw std::runtime_error("AIFF chunk is truncated.");
        }

        if (std::memcmp(chunk, "COMM", 4) == 0) {
            if (chunkSize < 18) {
                throw std::runtime_error("AIFF COMM chunk is truncated.");
            }
            channels = ReadU16Be(bytes, dataOffset);
            declaredFrames = ReadU32Be(bytes, dataOffset + 2);
            bitsPerSample = ReadU16Be(bytes, dataOffset + 6);
            sampleRate = static_cast<int>(std::lround(ReadExtended80Be(bytes, dataOffset + 8)));
        } else if (std::memcmp(chunk, "SSND", 4) == 0) {
            if (chunkSize < 8) {
                throw std::runtime_error("AIFF SSND chunk is truncated.");
            }
            const std::uint32_t dataSkip = ReadU32Be(bytes, dataOffset);
            const std::size_t pcmOffset = dataOffset + 8 + dataSkip;
            const std::size_t pcmStartLimit = dataOffset + chunkSize;
            if (pcmOffset > pcmStartLimit) {
                throw std::runtime_error("AIFF SSND data offset is invalid.");
            }
            soundOffset = pcmOffset;
            soundBytes = pcmStartLimit - pcmOffset;
        }

        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }

    if (channels <= 0 || channels > 8 || sampleRate <= 0 || bitsPerSample <= 0) {
        throw std::runtime_error("AIFF COMM metadata is invalid.");
    }
    if (soundOffset == static_cast<std::size_t>(-1) || soundBytes == 0) {
        throw std::runtime_error("AIFF SSND audio data was not found.");
    }
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        throw std::runtime_error("Unsupported AIFF bit depth: " + std::to_string(bitsPerSample));
    }

    const int bytesPerSample = bitsPerSample / 8;
    const std::size_t bytesPerFrame = static_cast<std::size_t>(channels) * bytesPerSample;
    std::uint64_t frameCount = soundBytes / bytesPerFrame;
    if (declaredFrames > 0) {
        frameCount = std::min<std::uint64_t>(frameCount, declaredFrames);
    }
    if (frameCount == 0 || frameCount > 10ULL * 60ULL * 60ULL * static_cast<std::uint64_t>(sampleRate)) {
        throw std::runtime_error("AIFF audio length is invalid.");
    }

    DecodedAudio audio;
    audio.channels = channels;
    audio.sampleRate = sampleRate;
    audio.bitsPerSample = bitsPerSample;
    audio.frameCount = frameCount;
    audio.format = "AIFF " + std::to_string(bitsPerSample) + "-bit";
    audio.samples.resize(static_cast<std::size_t>(frameCount) * channels);

    for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t sampleOffset =
                soundOffset +
                (static_cast<std::size_t>(frame) * channels + static_cast<std::size_t>(channel)) * bytesPerSample;
            audio.samples[static_cast<std::size_t>(frame) * channels + static_cast<std::size_t>(channel)] =
                DecodeSignedPcmSampleBe(bytes, sampleOffset, bitsPerSample);
        }
    }

    return audio;
}

int ReadWaveBitsPerSample(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return 0;
    }

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkSize = ReadU32Le(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            return 0;
        }
        if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
            return static_cast<int>(ReadU16Le(bytes, dataOffset + 14));
        }
        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }
    return 0;
}

std::string DescribeWaveAudio(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return "Decoded audio";
    }

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkSize = ReadU32Le(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            return "WAV";
        }
        if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
            const std::uint16_t formatTag = ReadU16Le(bytes, dataOffset);
            const std::uint16_t bitsPerSample = ReadU16Le(bytes, dataOffset + 14);
            const char* label = "WAV";
            switch (formatTag) {
            case 1:
                label = "WAV PCM";
                break;
            case 2:
                label = "WAV MS ADPCM";
                break;
            case 3:
                label = "WAV float";
                break;
            case 17:
                label = "WAV IMA ADPCM";
                break;
            default:
                break;
            }
            return std::string(label) + (bitsPerSample > 0 ? " " + std::to_string(bitsPerSample) + "-bit" : "");
        }
        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }
    return "WAV";
}

DecodedAudio DecodeMiniaudioAudio(const std::vector<char>& bytes) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder = {};
    const ma_result initResult = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder);
    if (initResult != MA_SUCCESS) {
        throw std::runtime_error("Audio format is not supported by the decoder.");
    }

    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    if (ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0) != MA_SUCCESS ||
        format != ma_format_f32 ||
        channels == 0 ||
        sampleRate == 0) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error("Decoded audio metadata is invalid.");
    }

    ma_uint64 frameCount = 0;
    std::vector<float> samples;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) == MA_SUCCESS && frameCount > 0) {
        ma_decoder_seek_to_pcm_frame(&decoder, 0);
        samples.resize(static_cast<std::size_t>(frameCount) * channels);
        ma_uint64 framesRead = 0;
        const ma_result readResult =
            ma_decoder_read_pcm_frames(&decoder, samples.data(), frameCount, &framesRead);
        if (readResult != MA_SUCCESS) {
            ma_decoder_uninit(&decoder);
            throw std::runtime_error("Could not decode audio samples.");
        }
        frameCount = framesRead;
        samples.resize(static_cast<std::size_t>(frameCount) * channels);
    } else {
        static constexpr ma_uint64 kChunkFrames = 4096;
        std::vector<float> chunk(static_cast<std::size_t>(kChunkFrames) * channels);
        for (;;) {
            ma_uint64 framesRead = 0;
            const ma_result readResult =
                ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead);
            if (readResult != MA_SUCCESS || framesRead == 0) {
                break;
            }
            samples.insert(
                samples.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(framesRead * channels));
            frameCount += framesRead;
        }
    }

    ma_decoder_uninit(&decoder);
    if (frameCount == 0 || samples.empty()) {
        throw std::runtime_error("Decoded audio contains no samples.");
    }

    DecodedAudio audio;
    audio.channels = static_cast<int>(channels);
    audio.sampleRate = static_cast<int>(sampleRate);
    audio.bitsPerSample = ReadWaveBitsPerSample(bytes);
    audio.frameCount = frameCount;
    audio.format = DescribeWaveAudio(bytes);
    audio.samples = std::move(samples);
    return audio;
}

DecodedAudio DecodeAudioBytes(const std::vector<char>& bytes) {
    if (bytes.size() >= 12 &&
        std::memcmp(bytes.data(), "FORM", 4) == 0 &&
        std::memcmp(bytes.data() + 8, "AIFF", 4) == 0) {
        return DecodeAiffAudio(bytes);
    }
    return DecodeMiniaudioAudio(bytes);
}

void RequireTextureRange(std::size_t offset,
                         std::size_t size,
                         std::size_t containerSize,
                         const char* message) {
    if (offset > containerSize || size > containerSize - offset) {
        throw std::runtime_error(message);
    }
}

void FlipTextureVertically(DecodedTexture& texture) {
    if (texture.width <= 0 || texture.height <= 1 || texture.rgba.empty()) {
        return;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(texture.width) * 4;
    std::vector<unsigned char> scratch(rowBytes);
    for (int y = 0; y < texture.height / 2; ++y) {
        const std::size_t top = static_cast<std::size_t>(y) * rowBytes;
        const std::size_t bottom = static_cast<std::size_t>(texture.height - 1 - y) * rowBytes;
        std::memcpy(scratch.data(), texture.rgba.data() + top, rowBytes);
        std::memcpy(texture.rgba.data() + top, texture.rgba.data() + bottom, rowBytes);
        std::memcpy(texture.rgba.data() + bottom, scratch.data(), rowBytes);
    }
}

DecodedTexture DecodeMipTexture(const std::vector<char>& bytes) {
    constexpr std::size_t kHeaderSize = 38;
    constexpr std::size_t kPaletteBytes = 256 * 4;
    if (bytes.size() < kHeaderSize || std::memcmp(bytes.data() + 18, "AIF1", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 MIP texture.");
    }

    DecodedTexture texture;
    texture.width = ReadU16Le(bytes, 12);
    texture.height = ReadU16Le(bytes, 14);
    texture.bitsPerPixel = ReadU16Le(bytes, 16);
    texture.mipLevels = static_cast<int>(ReadU32Le(bytes, 22));

    if (texture.width <= 0 || texture.height <= 0 ||
        texture.width > 8192 || texture.height > 8192) {
        throw std::runtime_error("Texture dimensions are invalid.");
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    texture.rgba.resize(pixelCount * 4);

    if (texture.bitsPerPixel == 8) {
        RequireTextureRange(kHeaderSize, kPaletteBytes + pixelCount, bytes.size(),
                            "Paletted MIP texture is truncated.");
        const std::size_t paletteOffset = kHeaderSize;
        const std::size_t indexOffset = kHeaderSize + kPaletteBytes;

        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const auto paletteIndex = static_cast<unsigned char>(bytes[indexOffset + pixel]);
            const std::size_t paletteEntry = paletteOffset + static_cast<std::size_t>(paletteIndex) * 4;
            texture.rgba[pixel * 4 + 0] = static_cast<unsigned char>(bytes[paletteEntry + 2]);
            texture.rgba[pixel * 4 + 1] = static_cast<unsigned char>(bytes[paletteEntry + 1]);
            texture.rgba[pixel * 4 + 2] = static_cast<unsigned char>(bytes[paletteEntry + 0]);
            texture.rgba[pixel * 4 + 3] = static_cast<unsigned char>(bytes[paletteEntry + 3]);
        }
        FlipTextureVertically(texture);
        return texture;
    }

    if (texture.bitsPerPixel == 24 || texture.bitsPerPixel == 32) {
        const std::size_t sourceStride = static_cast<std::size_t>(texture.bitsPerPixel / 8);
        RequireTextureRange(kHeaderSize, pixelCount * sourceStride, bytes.size(),
                            "MIP texture pixel data is truncated.");

        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const std::size_t source = kHeaderSize + pixel * sourceStride;
            texture.rgba[pixel * 4 + 0] = static_cast<unsigned char>(bytes[source + 2]);
            texture.rgba[pixel * 4 + 1] = static_cast<unsigned char>(bytes[source + 1]);
            texture.rgba[pixel * 4 + 2] = static_cast<unsigned char>(bytes[source + 0]);
            texture.rgba[pixel * 4 + 3] =
                texture.bitsPerPixel == 32 ? static_cast<unsigned char>(bytes[source + 3]) : 255;
        }
        FlipTextureVertically(texture);
        return texture;
    }

    throw std::runtime_error("Unsupported MIP bit depth: " + std::to_string(texture.bitsPerPixel));
}

DecodedTexture DecodeTgaTexture(const std::vector<char>& bytes) {
    constexpr std::size_t kTgaHeaderSize = 18;
    if (bytes.size() < kTgaHeaderSize) {
        throw std::runtime_error("TGA file is too small.");
    }

    const auto idLength = static_cast<unsigned char>(bytes[0]);
    const auto colorMapType = static_cast<unsigned char>(bytes[1]);
    const auto imageType = static_cast<unsigned char>(bytes[2]);
    if (colorMapType != 0 || imageType != 2) {
        throw std::runtime_error("Only uncompressed true-color TGA textures are supported.");
    }

    DecodedTexture texture;
    texture.width = ReadU16Le(bytes, 12);
    texture.height = ReadU16Le(bytes, 14);
    texture.bitsPerPixel = static_cast<unsigned char>(bytes[16]);
    texture.mipLevels = 1;

    if ((texture.bitsPerPixel != 24 && texture.bitsPerPixel != 32) ||
        texture.width <= 0 || texture.height <= 0) {
        throw std::runtime_error("Unsupported TGA texture format.");
    }

    const std::size_t sourceStride = static_cast<std::size_t>(texture.bitsPerPixel / 8);
    const std::size_t pixelCount =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    const std::size_t dataOffset = kTgaHeaderSize + idLength;
    RequireTextureRange(dataOffset, pixelCount * sourceStride, bytes.size(),
                        "TGA texture pixel data is truncated.");

    const bool topOrigin = (static_cast<unsigned char>(bytes[17]) & 0x20U) != 0;
    texture.rgba.resize(pixelCount * 4);
    for (int y = 0; y < texture.height; ++y) {
        const int sourceY = topOrigin ? y : texture.height - 1 - y;
        for (int x = 0; x < texture.width; ++x) {
            const std::size_t source = dataOffset +
                (static_cast<std::size_t>(sourceY) * texture.width + x) * sourceStride;
            const std::size_t destination =
                (static_cast<std::size_t>(y) * texture.width + x) * 4;
            texture.rgba[destination + 0] = static_cast<unsigned char>(bytes[source + 2]);
            texture.rgba[destination + 1] = static_cast<unsigned char>(bytes[source + 1]);
            texture.rgba[destination + 2] = static_cast<unsigned char>(bytes[source + 0]);
            texture.rgba[destination + 3] =
                texture.bitsPerPixel == 32 ? static_cast<unsigned char>(bytes[source + 3]) : 255;
        }
    }

    return texture;
}

std::filesystem::path BuildDumpMirrorPath(const AppState& state, const std::string& archivePath) {
    std::filesystem::path outputPath = state.archive.gtcPath.parent_path() / "dump";
    for (const auto& component : std::filesystem::path(archivePath)) {
        const auto text = component.string();
        if (text.empty() || text == ".") {
            continue;
        }
        if (text == ".." || component.has_root_directory() || component.has_root_name()) {
            throw std::runtime_error("Archive path escapes the dump folder.");
        }
        outputPath /= component;
    }
    return outputPath;
}

std::vector<char> ReadBinaryPreviewFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open " + path.string());
    }

    const auto size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Could not measure " + path.string());
    }

    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty() && !file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Could not read " + path.string());
    }
    return bytes;
}

std::vector<char> ReadEntryBytesForPreview(AppState& state, std::size_t entryIndex) {
    const gtc::FileEntry& entry = state.archive.entries[entryIndex];
    const auto dumpPath = BuildDumpMirrorPath(state, entry.path);
    if (std::filesystem::exists(dumpPath)) {
        return ReadBinaryPreviewFile(dumpPath);
    }

    if (!state.archiveDataLoaded) {
        state.status = "Preparing preview...";
        state.archiveData = gtc::ReadArchiveData(state.archive);
        state.archiveDataLoaded = true;
    }
    return gtc::ReadEntryData(state.archive, entryIndex, state.archiveData);
}

std::string NormalizeArchivePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    return ToLower(path);
}

std::size_t FindArchiveEntryIndex(const AppState& state, const std::string& archivePath) {
    const std::string wanted = NormalizeArchivePath(archivePath);
    for (std::size_t index = 0; index < state.archive.entries.size(); ++index) {
        if (NormalizeArchivePath(state.archive.entries[index].path) == wanted) {
            return index;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::string Trim(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char value) {
        return std::isspace(value) != 0;
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char value) {
        return std::isspace(value) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string FormatHex32(std::uint32_t value) {
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << value;
    return stream.str();
}

std::string ParentArchivePath(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return {};
    }
    return path.substr(0, slash);
}

std::string JoinArchivePathString(const std::string& parent, const std::string& child) {
    if (parent.empty()) {
        return child;
    }
    return parent + "\\" + child;
}

DecodedTexture DecodeTextureBytesByName(const std::vector<char>& bytes, const std::string& name) {
    const std::string extension = ToLower(std::filesystem::path(name).extension().string());
    if (extension == ".tga") {
        return DecodeTgaTexture(bytes);
    }
    return DecodeMipTexture(bytes);
}

std::size_t ResolveIflFrameEntryIndex(const AppState& state,
                                      const std::string& parentPath,
                                      const std::string& frameName) {
    const std::filesystem::path framePath(frameName);
    const std::string stem = framePath.stem().string();
    const std::string originalExtension = framePath.extension().string();
    const std::string originalName = framePath.filename().string();

    std::vector<std::string> candidates;
    candidates.push_back(stem + ".MIP");
    candidates.push_back(stem + ".mip");
    if (!originalName.empty()) {
        candidates.push_back(originalName);
    }
    if (ToLower(originalExtension) != ".tga") {
        candidates.push_back(stem + ".TGA");
        candidates.push_back(stem + ".tga");
    }

    for (const std::string& candidate : candidates) {
        const std::size_t entryIndex = FindArchiveEntryIndex(state, JoinArchivePathString(parentPath, candidate));
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }

    return static_cast<std::size_t>(-1);
}

struct LoadedIflFrames {
    std::vector<DecodedTexture> frames;
    std::vector<std::string> frameNames;
    std::vector<int> frameTicks;
    int listedFrames = 0;
    int missingFrames = 0;
};

LoadedIflFrames LoadIflFrames(AppState& state, const ArchiveNode& node, const std::vector<char>& iflBytes) {
    const std::string text(iflBytes.begin(), iflBytes.end());
    const std::string parentPath = ParentArchivePath(node.path);
    std::istringstream stream(text);
    std::string line;
    LoadedIflFrames result;

    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line.front() == ';') {
            continue;
        }

        const std::size_t comment = line.find(';');
        if (comment != std::string::npos) {
            line = Trim(line.substr(0, comment));
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream row(line);
        std::string frameName;
        int frameTicks = 1;
        row >> frameName;
        row >> frameTicks;
        if (frameName.empty()) {
            continue;
        }

        ++result.listedFrames;
        frameTicks = std::max(1, frameTicks);
        const std::size_t frameEntryIndex = ResolveIflFrameEntryIndex(state, parentPath, frameName);
        if (frameEntryIndex == static_cast<std::size_t>(-1)) {
            ++result.missingFrames;
            continue;
        }

        const gtc::FileEntry& frameEntry = state.archive.entries[frameEntryIndex];
        try {
            const std::vector<char> frameBytes = ReadEntryBytesForPreview(state, frameEntryIndex);
            result.frames.push_back(DecodeTextureBytesByName(frameBytes, frameEntry.path));
            result.frameNames.push_back(frameEntry.path);
            result.frameTicks.push_back(frameTicks);
        } catch (const std::exception& error) {
            LogDebug(std::string("IFL frame load failed: ") + frameEntry.path + " - " + error.what());
            ++result.missingFrames;
        }
    }

    if (result.frames.empty()) {
        throw std::runtime_error("IFL did not resolve any supported texture frames.");
    }

    return result;
}

DecodedTexture DecodeArchiveTextureEntryForPreview(AppState& state, std::size_t entryIndex) {
    if (entryIndex >= state.archive.entries.size()) {
        throw std::runtime_error("Texture entry index is out of range.");
    }

    const gtc::FileEntry& entry = state.archive.entries[entryIndex];
    const std::vector<char> bytes = ReadEntryBytesForPreview(state, entryIndex);
    const std::string extension = ToLower(std::filesystem::path(entry.path).extension().string());
    if (extension == ".ifl") {
        ArchiveNode node;
        node.name = std::filesystem::path(entry.path).filename().string();
        node.path = entry.path;
        node.directory = false;
        node.entryIndex = entryIndex;
        LoadedIflFrames frames = LoadIflFrames(state, node, bytes);
        return frames.frames.front();
    }
    return DecodeTextureBytesByName(bytes, entry.path);
}

std::size_t ResolveModelTextureEntryIndex(const AppState& state, const std::string& texturePath) {
    if (texturePath.empty()) {
        return static_cast<std::size_t>(-1);
    }

    std::vector<std::string> candidates;
    std::filesystem::path archivePath(texturePath);
    const auto addCandidate = [&](std::filesystem::path path) {
        std::string candidate = path.string();
        std::replace(candidate.begin(), candidate.end(), '/', '\\');
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(std::move(candidate));
        }
    };

    addCandidate(archivePath);
    const std::string extension = ToLower(archivePath.extension().string());
    if (extension == ".tga") {
        std::filesystem::path tgaPath = archivePath;
        tgaPath.replace_extension(".TGA");
        addCandidate(tgaPath);
        tgaPath.replace_extension(".tga");
        addCandidate(tgaPath);

        std::filesystem::path mipPath = archivePath;
        mipPath.replace_extension(".MIP");
        addCandidate(mipPath);
        mipPath.replace_extension(".mip");
        addCandidate(mipPath);
    } else if (extension == ".mip") {
        std::filesystem::path mipPath = archivePath;
        mipPath.replace_extension(".MIP");
        addCandidate(mipPath);
        mipPath.replace_extension(".mip");
        addCandidate(mipPath);
    } else {
        std::filesystem::path tgaPath = archivePath;
        tgaPath.replace_extension(".TGA");
        addCandidate(tgaPath);
        tgaPath.replace_extension(".tga");
        addCandidate(tgaPath);

        std::filesystem::path mipPath = archivePath;
        mipPath.replace_extension(".MIP");
        addCandidate(mipPath);
        mipPath.replace_extension(".mip");
        addCandidate(mipPath);
    }

    for (std::string candidate : candidates) {
        const std::size_t entryIndex = FindArchiveEntryIndex(state, candidate);
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }

    return static_cast<std::size_t>(-1);
}

void AddSkeletonCandidate(std::vector<std::string>& candidates, const std::string& parentPath, const std::string& stem) {
    if (stem.empty()) {
        return;
    }

    candidates.push_back(JoinArchivePathString(parentPath, stem + ".BSB"));
    candidates.push_back(JoinArchivePathString(parentPath, stem + ".bsb"));
}

std::size_t ResolveModelSkeletonEntryIndex(const AppState& state, const ArchiveNode& modelNode) {
    const std::string parentPath = ParentArchivePath(modelNode.path);
    const std::string stem = std::filesystem::path(modelNode.name).stem().string();

    std::vector<std::string> candidates;
    AddSkeletonCandidate(candidates, parentPath, stem);

    const std::string parentName = ToLower(std::filesystem::path(parentPath).filename().string());
    if (parentName == "anm") {
        AddSkeletonCandidate(candidates, ParentArchivePath(parentPath), stem);
    }

    for (std::string candidate : candidates) {
        std::replace(candidate.begin(), candidate.end(), '/', '\\');
        const std::size_t entryIndex = FindArchiveEntryIndex(state, candidate);
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }

    if (modelNode.parent >= 0 && modelNode.parent < static_cast<int>(state.browser.nodes.size())) {
        const ArchiveNode& folder = state.browser.nodes[modelNode.parent];
        int onlySkeletonNode = -1;
        for (const int fileNodeIndex : folder.files) {
            if (fileNodeIndex < 0 || fileNodeIndex >= static_cast<int>(state.browser.nodes.size())) {
                continue;
            }
            const ArchiveNode& fileNode = state.browser.nodes[fileNodeIndex];
            if (ToLower(std::filesystem::path(fileNode.name).extension().string()) == ".bsb") {
                if (onlySkeletonNode >= 0) {
                    onlySkeletonNode = -1;
                    break;
                }
                onlySkeletonNode = fileNodeIndex;
            }
        }
        if (onlySkeletonNode >= 0) {
            return state.browser.nodes[onlySkeletonNode].entryIndex;
        }
    }

    return static_cast<std::size_t>(-1);
}

std::size_t ResolveModelAnimationEntryIndex(const AppState& state,
                                            const std::string& skeletonPath,
                                            const std::string& clipName) {
    if (clipName.empty()) {
        return static_cast<std::size_t>(-1);
    }

    const std::string parentPath = ParentArchivePath(skeletonPath);
    std::filesystem::path clipPath(clipName);
    std::vector<std::string> candidates;
    if (ToLower(clipPath.extension().string()) == ".bsa") {
        candidates.push_back(JoinArchivePathString(parentPath, clipPath.string()));
    } else {
        candidates.push_back(JoinArchivePathString(parentPath, clipName + ".BSA"));
        candidates.push_back(JoinArchivePathString(parentPath, clipName + ".bsa"));
    }

    for (std::string candidate : candidates) {
        std::replace(candidate.begin(), candidate.end(), '/', '\\');
        const std::size_t entryIndex = FindArchiveEntryIndex(state, candidate);
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }

    return static_cast<std::size_t>(-1);
}

void LoadModelPreviewAnimations(AppState& state, ModelPreview& preview, const std::string& skeletonPath) {
    preview.animations.clear();
    preview.selectedAnimation = -1;
    preview.animationPlaying = false;
    preview.animationTime = 0.0;
    preview.animationLastUpdateTime = 0.0;

    for (const std::string& clipName : preview.skeletonClips) {
        ModelAnimationClip clip;
        clip.name = clipName;
        const std::size_t entryIndex = ResolveModelAnimationEntryIndex(state, skeletonPath, clipName);
        if (entryIndex == static_cast<std::size_t>(-1)) {
            clip.status = "Missing .BSA";
            preview.animations.push_back(std::move(clip));
            continue;
        }

        const gtc::FileEntry& animationEntry = state.archive.entries[entryIndex];
        try {
            const std::vector<char> bytes = ReadEntryBytesForPreview(state, entryIndex);
            clip = DecodeBsaAnimation(bytes, clipName, animationEntry.path);
            clip.loaded = !clip.frames.empty();
            preview.animations.push_back(std::move(clip));
        } catch (const std::exception& error) {
            clip.path = animationEntry.path;
            clip.status = std::string("Load failed: ") + error.what();
            LogDebug("BSA animation load failed: " + animationEntry.path + " - " + error.what());
            preview.animations.push_back(std::move(clip));
        }
    }
}

void LoadModelPreviewSkeleton(AppState& state, ModelPreview& preview, const ArchiveNode& modelNode) {
    const std::size_t skeletonEntryIndex = ResolveModelSkeletonEntryIndex(state, modelNode);
    if (skeletonEntryIndex == static_cast<std::size_t>(-1)) {
        LogDebug("model skeleton not found: " + modelNode.path);
        return;
    }

    try {
        const gtc::FileEntry& skeletonEntry = state.archive.entries[skeletonEntryIndex];
        const std::vector<char> skeletonBytes = ReadEntryBytesForPreview(state, skeletonEntryIndex);
        ModelPreview skeleton = DecodeBsbSkeleton(skeletonBytes);
        preview.skeleton = std::move(skeleton.skeleton);
        preview.skeletonClips = std::move(skeleton.skeletonClips);
        LoadModelPreviewAnimations(state, preview, skeletonEntry.path);
        const bool skinAligned = ApplySkinDerivedSkeletonAlignment(preview);
        if (!skinAligned) {
            ApplySkeletonAlignment(preview);
            BuildGeneratedSkinWeights(preview);
        } else {
            preview.skinnedVertices = preview.vertices;
        }
        preview.showSkeleton = !preview.skeleton.empty();
        if (!preview.skeleton.empty()) {
            preview.status += "  skeleton " + std::to_string(preview.skeleton.size()) + " bones";
            preview.status += skinAligned ? "  SKN0 skin" : "  generated skin weights";
            if (!preview.skeletonClips.empty()) {
                preview.status += "  clips " + std::to_string(preview.skeletonClips.size());
            }
            if (!preview.animations.empty()) {
                const auto loadedAnimations = std::count_if(
                    preview.animations.begin(),
                    preview.animations.end(),
                    [](const ModelAnimationClip& clip) {
                        return clip.loaded;
                    });
                preview.status += "  animations " + std::to_string(loadedAnimations) + "/" +
                                  std::to_string(preview.animations.size());
            }
            LogDebug(
                "model skeleton loaded: " + skeletonEntry.path +
                " bones=" + std::to_string(preview.skeleton.size()) +
                " clips=" + std::to_string(preview.skeletonClips.size()) +
                " animations=" + std::to_string(preview.animations.size()) +
                " skn0_skin=" + std::to_string(skinAligned ? 1 : 0) +
                " generated_skin_weights=" + std::to_string(skinAligned ? 0 : 1));
        }
    } catch (const std::exception& error) {
        preview.status += "  skeleton failed";
        LogDebug(std::string("model skeleton load failed: ") + modelNode.path + " - " + error.what());
    }
}

void LoadModelPreviewTextures(AppState& state, ModelPreview& preview) {
    int loaded = 0;
    int missing = 0;
    for (ModelMaterial& material : preview.materials) {
        const std::size_t textureEntryIndex = ResolveModelTextureEntryIndex(state, material.path);
        if (textureEntryIndex == static_cast<std::size_t>(-1)) {
            ++missing;
            continue;
        }

        try {
            material.texture = DecodeArchiveTextureEntryForPreview(state, textureEntryIndex);
            FlipTextureVertically(material.texture);
            material.loaded = material.texture.width > 0 &&
                              material.texture.height > 0 &&
                              !material.texture.rgba.empty();
            if (material.loaded) {
                DeleteGlTexture(material.textureId);
                material.textureId = CreateGlTextureRgba(
                    material.texture.width,
                    material.texture.height,
                    material.texture.rgba.data(),
                    true,
                    true);
                material.loaded = material.textureId != 0;
                ++loaded;
            }
        } catch (const std::exception& error) {
            LogDebug(std::string("model texture load failed: ") + material.path + " - " + error.what());
            ++missing;
        }
    }

    if (!preview.materials.empty()) {
        preview.status += "  textures " + std::to_string(loaded) + "/" +
                          std::to_string(preview.materials.size()) + " loaded";
        LogDebug(
            "model textures loaded: " + preview.path +
            " loaded=" + std::to_string(loaded) +
            " missing=" + std::to_string(missing) +
            " total=" + std::to_string(preview.materials.size()));
    }
}

void LoadIflPreview(AppState& state, TexturePreview& preview, const ArchiveNode& node, const std::vector<char>& iflBytes) {
    LoadedIflFrames loadedFrames = LoadIflFrames(state, node, iflBytes);

    preview.animated = true;
    preview.frameIndex = 0;
    preview.lastFrameTime = ImGui::GetTime();
    preview.frames = std::move(loadedFrames.frames);
    preview.frameTicks = std::move(loadedFrames.frameTicks);
    preview.decoded = preview.frames.front();
    if (loadedFrames.missingFrames > 0) {
        preview.status = "Loaded " + std::to_string(preview.frames.size()) + " of " +
                         std::to_string(loadedFrames.listedFrames) + " IFL frames.";
    }
}

DecodedTexture DecodeArchiveTextureEntryForWhirledRender(AppState& state, std::size_t entryIndex) {
    if (entryIndex >= state.archive.entries.size()) {
        throw std::runtime_error("Texture entry index is out of range.");
    }

    DecodedTexture texture = DecodeArchiveTextureEntryForPreview(state, entryIndex);
    FlipTextureVertically(texture);
    return texture;
}

void RequireModelRange(std::size_t offset,
                       std::size_t size,
                       std::size_t limit,
                       const char* message) {
    if (offset > limit || size > limit - offset) {
        throw std::runtime_error(message);
    }
}

std::size_t FindFourCc(const std::vector<char>& bytes, const char* fourCc) {
    const auto found = std::search(bytes.begin(), bytes.end(), fourCc, fourCc + 4);
    if (found == bytes.end()) {
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(std::distance(bytes.begin(), found));
}

ModelAnimationClip DecodeBsaAnimation(const std::vector<char>& bytes, std::string name, std::string path) {
    if (bytes.size() < 32 || std::memcmp(bytes.data(), "ANIMBIN", 7) != 0) {
        throw std::runtime_error("Not a supported LR2 BSA animation.");
    }

    const std::uint32_t version = ReadU32Le(bytes, 8);
    const std::uint32_t fileSize = ReadU32Le(bytes, 12);
    const std::uint16_t frameCount = ReadU16Le(bytes, 16);
    const std::uint16_t flags = ReadU16Le(bytes, 18);
    float fps = ReadF32Le(bytes, 20);
    const std::uint32_t boneCount = ReadU32Le(bytes, 24);

    if (version != 2) {
        throw std::runtime_error("BSA animation version is unsupported.");
    }
    if (frameCount == 0 || frameCount > 10000 || boneCount == 0 || boneCount > 512) {
        throw std::runtime_error("BSA animation dimensions are unsupported.");
    }
    if (!std::isfinite(fps) || fps <= 0.0f) {
        fps = 15.0f;
    }

    const std::size_t streamLimit = fileSize > 0 && fileSize <= bytes.size() ? fileSize : bytes.size();
    constexpr std::size_t kHeaderSize = 32;
    const std::size_t frameStride = 24 + static_cast<std::size_t>(boneCount) * 16;
    RequireModelRange(kHeaderSize, static_cast<std::size_t>(frameCount) * frameStride, streamLimit,
                      "BSA animation frames are truncated.");

    ModelAnimationClip clip;
    clip.name = std::move(name);
    clip.path = std::move(path);
    clip.loaded = true;
    clip.rootMotion = (flags & 1U) != 0;
    clip.loop = clip.rootMotion || ToLower(clip.name).find("loop") != std::string::npos;
    clip.fps = fps;
    clip.frameCount = frameCount;
    clip.boneCount = static_cast<int>(boneCount);
    clip.frames.reserve(frameCount);

    for (std::uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const std::size_t frameOffset = kHeaderSize + static_cast<std::size_t>(frameIndex) * frameStride;
        ModelAnimationFrame frame;
        frame.rootPosition = {
            ReadF32Le(bytes, frameOffset + 0),
            ReadF32Le(bytes, frameOffset + 4),
            ReadF32Le(bytes, frameOffset + 8),
        };
        frame.rootMotion = {
            ReadF32Le(bytes, frameOffset + 12),
            ReadF32Le(bytes, frameOffset + 16),
            ReadF32Le(bytes, frameOffset + 20),
        };
        Sanitize(frame.rootPosition);
        Sanitize(frame.rootMotion);

        frame.localRotations.reserve(boneCount);
        const std::size_t rotationsOffset = frameOffset + 24;
        for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            const std::size_t rotationOffset = rotationsOffset + static_cast<std::size_t>(boneIndex) * 16;
            Quat rotation = Normalize({
                ReadF32Le(bytes, rotationOffset + 0),
                ReadF32Le(bytes, rotationOffset + 4),
                ReadF32Le(bytes, rotationOffset + 8),
                ReadF32Le(bytes, rotationOffset + 12),
            });
            if (!std::isfinite(rotation.x) ||
                !std::isfinite(rotation.y) ||
                !std::isfinite(rotation.z) ||
                !std::isfinite(rotation.w)) {
                rotation = {};
            }
            frame.localRotations.push_back(rotation);
        }
        clip.frames.push_back(std::move(frame));
    }

    clip.status =
        std::to_string(clip.frameCount) + " frames @ " +
        std::to_string(static_cast<int>(std::round(clip.fps))) + " fps";
    LogDebug(
        "BSA animation parsed: " + clip.path +
        " frames=" + std::to_string(clip.frameCount) +
        " bones=" + std::to_string(clip.boneCount) +
        " fps=" + std::to_string(clip.fps) +
        " root_motion=" + std::to_string(clip.rootMotion ? 1 : 0));
    return clip;
}

void Sanitize(Vec3& value) {
    if (!std::isfinite(value.x)) {
        value.x = 0.0f;
    }
    if (!std::isfinite(value.y)) {
        value.y = 0.0f;
    }
    if (!std::isfinite(value.z)) {
        value.z = 0.0f;
    }
}

void IncludeInBounds(ModelPreview& model, Vec3 position) {
    model.boundsMin.x = std::min(model.boundsMin.x, position.x);
    model.boundsMin.y = std::min(model.boundsMin.y, position.y);
    model.boundsMin.z = std::min(model.boundsMin.z, position.z);
    model.boundsMax.x = std::max(model.boundsMax.x, position.x);
    model.boundsMax.y = std::max(model.boundsMax.y, position.y);
    model.boundsMax.z = std::max(model.boundsMax.z, position.z);
}

std::string ReadFixedString(const std::vector<char>& bytes, std::size_t offset, std::size_t maxLength) {
    if (offset >= bytes.size()) {
        return {};
    }

    maxLength = std::min(maxLength, bytes.size() - offset);
    std::size_t length = 0;
    while (length < maxLength && bytes[offset + length] != '\0') {
        ++length;
    }
    return Trim(std::string(bytes.data() + offset, bytes.data() + offset + length));
}

std::string ReadBoundedString(const std::vector<char>& bytes,
                              std::size_t offset,
                              std::size_t end,
                              std::size_t maxLength) {
    if (offset >= end || offset >= bytes.size()) {
        return {};
    }

    end = std::min(end, bytes.size());
    maxLength = std::min(maxLength, end - offset);
    std::size_t length = 0;
    while (length < maxLength && bytes[offset + length] != '\0') {
        const auto character = static_cast<unsigned char>(bytes[offset + length]);
        if (character < 32 && character != '\t') {
            break;
        }
        ++length;
    }
    return Trim(std::string(bytes.data() + offset, bytes.data() + offset + length));
}

void UpdateSkeletonWorldPose(std::vector<SkeletonBone>& bones) {
    for (SkeletonBone& bone : bones) {
        const bool hasParent =
            bone.parent >= 0 &&
            bone.parent < static_cast<int>(bones.size()) &&
            bone.parent != bone.id;
        if (!hasParent) {
            bone.worldRotation = Normalize(bone.localRotation);
            bone.worldPosition = bone.localPosition;
            continue;
        }

        const SkeletonBone& parent = bones[bone.parent];
        bone.worldRotation = Multiply(parent.worldRotation, bone.localRotation);
        bone.worldPosition = Add(parent.worldPosition, Rotate(parent.worldRotation, bone.localPosition));
    }
}

ModelPreview DecodeBsbSkeleton(const std::vector<char>& bytes) {
    if (bytes.size() < 0x30 || std::memcmp(bytes.data(), "SKELBIN", 7) != 0) {
        throw std::runtime_error("Not a supported LR2 BSB skeleton.");
    }

    const std::uint32_t fileSize = ReadU32Le(bytes, 12);
    const std::uint32_t boneCount = ReadU32Le(bytes, 16);
    const std::uint32_t boneTableOffset = ReadU32Le(bytes, 20);
    const std::uint32_t clipCount = ReadU32Le(bytes, 24);
    const std::uint32_t clipNameOffset = ReadU32Le(bytes, 28);
    constexpr std::size_t kBoneRecordSize = 32;
    constexpr std::size_t kClipNameSize = 32;

    if (fileSize != 0 && fileSize > bytes.size()) {
        throw std::runtime_error("BSB skeleton file size is invalid.");
    }
    if (boneCount == 0 || boneCount > 512) {
        throw std::runtime_error("BSB skeleton bone count is unsupported.");
    }
    RequireModelRange(
        boneTableOffset,
        static_cast<std::size_t>(boneCount) * kBoneRecordSize,
        bytes.size(),
        "BSB skeleton bone table is truncated.");

    std::vector<SkeletonBone> rawBones;
    rawBones.reserve(boneCount);
    for (std::uint32_t recordIndex = 0; recordIndex < boneCount; ++recordIndex) {
        const std::size_t offset = boneTableOffset + static_cast<std::size_t>(recordIndex) * kBoneRecordSize;
        SkeletonBone bone;
        bone.id = static_cast<int>(ReadU16Le(bytes, offset));
        const std::uint16_t parent = ReadU16Le(bytes, offset + 2);
        bone.parent = parent == 0xffffU ? -1 : static_cast<int>(parent);
        bone.localRotation = Normalize({
            ReadF32Le(bytes, offset + 4),
            ReadF32Le(bytes, offset + 8),
            ReadF32Le(bytes, offset + 12),
            ReadF32Le(bytes, offset + 16),
        });
        bone.bindLocalRotation = bone.localRotation;
        bone.localPosition = {
            ReadF32Le(bytes, offset + 20),
            ReadF32Le(bytes, offset + 24),
            ReadF32Le(bytes, offset + 28),
        };
        Sanitize(bone.localPosition);
        rawBones.push_back(bone);
    }

    std::unordered_map<int, int> idToIndex;
    idToIndex.reserve(rawBones.size());
    for (int index = 0; index < static_cast<int>(rawBones.size()); ++index) {
        idToIndex[rawBones[index].id] = index;
    }

    std::vector<SkeletonBone> bones = rawBones;
    for (SkeletonBone& bone : bones) {
        if (bone.parent < 0) {
            continue;
        }
        const auto found = idToIndex.find(bone.parent);
        bone.parent = found == idToIndex.end() ? -1 : found->second;
    }
    for (int index = 0; index < static_cast<int>(bones.size()); ++index) {
        bones[index].id = index;
    }
    UpdateSkeletonWorldPose(bones);
    for (SkeletonBone& bone : bones) {
        bone.bindWorldPosition = bone.worldPosition;
        bone.bindWorldRotation = bone.worldRotation;
    }

    ModelPreview skeleton;
    skeleton.skeleton = std::move(bones);
    if (clipCount > 0 && clipCount <= 256 && clipNameOffset < bytes.size()) {
        for (std::uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex) {
            const std::size_t offset = clipNameOffset + static_cast<std::size_t>(clipIndex) * kClipNameSize;
            const std::string clipName = ReadFixedString(bytes, offset, kClipNameSize);
            if (!clipName.empty()) {
                skeleton.skeletonClips.push_back(clipName);
            }
        }
    }
    return skeleton;
}

std::array<int, 3> AxesSortedByExtent(Vec3 extent) {
    std::array<int, 3> axes = {0, 1, 2};
    std::sort(axes.begin(), axes.end(), [&](int left, int right) {
        return GetAxis(extent, left) > GetAxis(extent, right);
    });
    return axes;
}

void ApplySkeletonAlignment(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        return;
    }

    Vec3 skeletonMin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 skeletonMax{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    for (const SkeletonBone& bone : preview.skeleton) {
        skeletonMin.x = std::min(skeletonMin.x, bone.worldPosition.x);
        skeletonMin.y = std::min(skeletonMin.y, bone.worldPosition.y);
        skeletonMin.z = std::min(skeletonMin.z, bone.worldPosition.z);
        skeletonMax.x = std::max(skeletonMax.x, bone.worldPosition.x);
        skeletonMax.y = std::max(skeletonMax.y, bone.worldPosition.y);
        skeletonMax.z = std::max(skeletonMax.z, bone.worldPosition.z);
    }

    const Vec3 skeletonExtent = Subtract(skeletonMax, skeletonMin);
    const Vec3 modelExtent = Subtract(preview.boundsMax, preview.boundsMin);
    const std::array<int, 3> skeletonAxes = AxesSortedByExtent(skeletonExtent);
    const std::array<int, 3> modelAxes = AxesSortedByExtent(modelExtent);

    std::array<int, 3> sourceAxisForModel = {0, 1, 2};
    for (int rank = 0; rank < 3; ++rank) {
        sourceAxisForModel[modelAxes[rank]] = skeletonAxes[rank];
    }

    for (SkeletonBone& bone : preview.skeleton) {
        Vec3 aligned{};
        for (int modelAxis = 0; modelAxis < 3; ++modelAxis) {
            const int sourceAxis = sourceAxisForModel[modelAxis];
            const float sourceExtent = std::max(0.0001f, GetAxis(skeletonExtent, sourceAxis));
            const float modelExtentAxis = std::max(0.0001f, GetAxis(modelExtent, modelAxis));
            const float normalized =
                (GetAxis(bone.worldPosition, sourceAxis) - GetAxis(skeletonMin, sourceAxis)) / sourceExtent;
            const float component = GetAxis(preview.boundsMin, modelAxis) + normalized * modelExtentAxis;
            SetAxis(aligned, modelAxis, component);
        }
        bone.worldPosition = aligned;
        bone.bindWorldPosition = aligned;
    }
}

bool HasModelSkinning(const ModelPreview& preview) {
    return !preview.skinRecords.empty() && !preview.skinLookupGroups.empty();
}

Vec3 TransformSkinInfluence(const SkeletonBone& bone, const SkinBlendInfluence& influence, bool bindPose) {
    const Vec3 bonePosition = bindPose ? bone.bindWorldPosition : bone.worldPosition;
    const Quat boneRotation = bindPose ? bone.bindWorldRotation : bone.worldRotation;
    return Add(bonePosition, Rotate(boneRotation, influence.localPosition));
}

Vec3 ComputeSkinRecordPosition(const ModelPreview& preview, const SkinBlendRecord& record, bool bindPose) {
    Vec3 position{};
    float totalWeight = 0.0f;
    for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
        const SkinBlendInfluence& influence = record.influences[influenceIndex];
        if (influence.bone < 0 ||
            influence.bone >= static_cast<int>(preview.skeleton.size()) ||
            influence.weight <= 0.0f) {
            continue;
        }
        position = Add(
            position,
            Multiply(TransformSkinInfluence(preview.skeleton[influence.bone], influence, bindPose), influence.weight));
        totalWeight += influence.weight;
    }

    if (totalWeight <= 0.0001f) {
        return {};
    }
    return Multiply(position, 1.0f / totalWeight);
}

bool ApplySkinDerivedSkeletonAlignment(ModelPreview& preview) {
    if (preview.skeleton.empty() || !HasModelSkinning(preview)) {
        return false;
    }

    std::vector<Vec3> bindRecordPositions(preview.skinRecords.size());
    std::vector<bool> validRecord(preview.skinRecords.size(), false);
    for (std::size_t recordIndex = 0; recordIndex < preview.skinRecords.size(); ++recordIndex) {
        const Vec3 position = ComputeSkinRecordPosition(preview, preview.skinRecords[recordIndex], true);
        if (std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z)) {
            bindRecordPositions[recordIndex] = position;
            validRecord[recordIndex] = preview.skinRecords[recordIndex].influenceCount > 0;
        }
    }

    Vec3 accumulatedOffset{};
    std::size_t sampleCount = 0;
    for (const SkinLookupGroup& group : preview.skinLookupGroups) {
        const std::size_t vertexCount = std::min(group.vertexCount, group.recordIndices.size());
        for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
            const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
            if (modelVertexIndex >= preview.vertices.size() ||
                recordIndex >= bindRecordPositions.size() ||
                !validRecord[recordIndex]) {
                continue;
            }
            accumulatedOffset = Add(
                accumulatedOffset,
                Subtract(preview.vertices[modelVertexIndex].bindPosition, bindRecordPositions[recordIndex]));
            ++sampleCount;
        }
    }

    if (sampleCount == 0) {
        return false;
    }

    const Vec3 offset = Multiply(accumulatedOffset, 1.0f / static_cast<float>(sampleCount));
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z)) {
        return false;
    }

    for (SkeletonBone& bone : preview.skeleton) {
        bone.bindWorldPosition = Add(bone.bindWorldPosition, offset);
        bone.worldPosition = bone.bindWorldPosition;
    }
    preview.skeletonWorldOffset = offset;
    preview.hasSkeletonWorldOffset = true;
    LogDebug(
        "skin-derived skeleton alignment: samples=" + std::to_string(sampleCount) +
        " offset=(" + std::to_string(offset.x) + "," +
        std::to_string(offset.y) + "," +
        std::to_string(offset.z) + ")");
    return true;
}

bool IsBoneAffectedBySelection(const ModelPreview& preview, int boneIndex, int selectedBone) {
    int current = boneIndex;
    while (current >= 0 && current < static_cast<int>(preview.skeleton.size())) {
        if (current == selectedBone) {
            return true;
        }
        current = preview.skeleton[current].parent;
    }
    return false;
}

void BuildGeneratedSkinWeights(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        preview.skinnedVertices = preview.vertices;
        return;
    }

    for (ModelVertex& vertex : preview.vertices) {
        vertex.skinInfluenceCount = 0;
        vertex.skinInfluences = {};

        std::array<std::pair<float, int>, 2> best = {
            std::make_pair(std::numeric_limits<float>::max(), -1),
            std::make_pair(std::numeric_limits<float>::max(), -1),
        };

        for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
            const SkeletonBone& bone = preview.skeleton[boneIndex];
            const float distanceSquared =
                bone.parent >= 0 && bone.parent < static_cast<int>(preview.skeleton.size())
                    ? DistancePointSegmentSquared(
                          vertex.bindPosition,
                          preview.skeleton[bone.parent].bindWorldPosition,
                          bone.bindWorldPosition)
                    : DistanceSquared(vertex.bindPosition, bone.bindWorldPosition);

            if (distanceSquared < best[0].first) {
                best[1] = best[0];
                best[0] = {distanceSquared, boneIndex};
            } else if (distanceSquared < best[1].first) {
                best[1] = {distanceSquared, boneIndex};
            }
        }

        if (best[0].second < 0) {
            continue;
        }

        const float firstDistance = std::sqrt(std::max(0.0f, best[0].first));
        const float secondDistance = best[1].second >= 0 ? std::sqrt(std::max(0.0f, best[1].first)) : std::numeric_limits<float>::max();
        const bool useSecond = best[1].second >= 0 && secondDistance <= firstDistance * 2.25f + 0.35f;
        const float firstInfluence = 1.0f / (firstDistance + 0.05f);
        const float secondInfluence = useSecond ? 1.0f / (secondDistance + 0.05f) : 0.0f;
        const float totalInfluence = std::max(0.0001f, firstInfluence + secondInfluence);

        vertex.skinInfluences[0] = {best[0].second, firstInfluence / totalInfluence};
        vertex.skinInfluenceCount = 1;
        if (useSecond) {
            vertex.skinInfluences[1] = {best[1].second, secondInfluence / totalInfluence};
            vertex.skinInfluenceCount = 2;
        }
    }

    preview.skinnedVertices = preview.vertices;
}

bool IsAnimationPlayable(const ModelAnimationClip& clip) {
    return clip.loaded && !clip.frames.empty() && clip.frameCount > 0 && clip.boneCount > 0;
}

bool HasActiveModelAnimation(const ModelPreview& preview) {
    return preview.selectedAnimation >= 0 &&
           preview.selectedAnimation < static_cast<int>(preview.animations.size()) &&
           IsAnimationPlayable(preview.animations[preview.selectedAnimation]);
}

void PlayModelAnimation(ModelPreview& preview, int animationIndex) {
    if (animationIndex < 0 || animationIndex >= static_cast<int>(preview.animations.size()) ||
        !IsAnimationPlayable(preview.animations[animationIndex])) {
        return;
    }

    preview.selectedAnimation = animationIndex;
    preview.animationPlaying = true;
    preview.animationTime = 0.0;
    preview.animationLastUpdateTime = ImGui::GetTime();
    preview.boneEditAngle = 0.0f;
    preview.rotatingBone = false;
}

void AdvanceModelAnimation(ModelPreview& preview) {
    const double now = ImGui::GetTime();
    if (!HasActiveModelAnimation(preview)) {
        preview.animationLastUpdateTime = now;
        return;
    }

    if (preview.animationLastUpdateTime <= 0.0) {
        preview.animationLastUpdateTime = now;
    }
    const double delta = std::max(0.0, now - preview.animationLastUpdateTime);
    preview.animationLastUpdateTime = now;

    if (!preview.animationPlaying) {
        return;
    }

    const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
    preview.animationTime += delta;
    const double duration = clip.fps > 0.0f
                                ? static_cast<double>(clip.frameCount) / static_cast<double>(clip.fps)
                                : 0.0;
    if (duration > 0.0 && preview.animationTime >= duration) {
        if (clip.loop) {
            preview.animationTime = std::fmod(preview.animationTime, duration);
        } else {
            preview.animationTime = duration;
            preview.animationPlaying = false;
        }
    }
}

Quat SampleAnimationRotation(const ModelAnimationClip& clip, int boneIndex, double animationTime) {
    if (!IsAnimationPlayable(clip) || boneIndex < 0 || boneIndex >= clip.boneCount) {
        return {};
    }

    double frameValue = animationTime * static_cast<double>(clip.fps);
    if (clip.loop) {
        frameValue = std::fmod(frameValue, static_cast<double>(clip.frameCount));
        if (frameValue < 0.0) {
            frameValue += static_cast<double>(clip.frameCount);
        }
    } else {
        frameValue = std::clamp(frameValue, 0.0, static_cast<double>(std::max(0, clip.frameCount - 1)));
    }

    int firstFrame = static_cast<int>(std::floor(frameValue));
    firstFrame = std::clamp(firstFrame, 0, clip.frameCount - 1);
    int secondFrame = firstFrame + 1;
    if (secondFrame >= clip.frameCount) {
        secondFrame = clip.loop ? 0 : firstFrame;
    }
    const float t = static_cast<float>(frameValue - static_cast<double>(firstFrame));
    if (boneIndex >= static_cast<int>(clip.frames[firstFrame].localRotations.size()) ||
        boneIndex >= static_cast<int>(clip.frames[secondFrame].localRotations.size())) {
        return {};
    }

    return Slerp(
        clip.frames[firstFrame].localRotations[boneIndex],
        clip.frames[secondFrame].localRotations[boneIndex],
        t);
}

bool ApplyModelAnimationPose(ModelPreview& preview) {
    if (!HasActiveModelAnimation(preview) || preview.skeleton.empty()) {
        return false;
    }

    const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
    for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
        SkeletonBone& bone = preview.skeleton[boneIndex];
        if (boneIndex < clip.boneCount) {
            bone.localRotation = SampleAnimationRotation(clip, boneIndex, preview.animationTime);
        } else {
            bone.localRotation = bone.bindLocalRotation;
        }

        const bool hasParent =
            bone.parent >= 0 &&
            bone.parent < static_cast<int>(preview.skeleton.size()) &&
            bone.parent != bone.id;
        if (!hasParent) {
            bone.worldRotation = Normalize(bone.localRotation);
            bone.worldPosition = Add(bone.localPosition, preview.hasSkeletonWorldOffset ? preview.skeletonWorldOffset : Vec3{});
            continue;
        }

        const SkeletonBone& parent = preview.skeleton[bone.parent];
        bone.worldRotation = Multiply(parent.worldRotation, bone.localRotation);
        bone.worldPosition = Add(parent.worldPosition, Rotate(parent.worldRotation, bone.localPosition));
    }

    return true;
}

void UpdateModelSkinning(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        preview.skinnedVertices = preview.vertices;
        return;
    }

    const bool hasAnimation = ApplyModelAnimationPose(preview);
    if (!hasAnimation) {
        for (SkeletonBone& bone : preview.skeleton) {
            bone.localRotation = bone.bindLocalRotation;
            bone.worldPosition = bone.bindWorldPosition;
            bone.worldRotation = bone.bindWorldRotation;
        }
    }

    const bool hasEdit =
        preview.selectedBone >= 0 &&
        preview.selectedBone < static_cast<int>(preview.skeleton.size()) &&
        std::abs(preview.boneEditAngle) > 0.0001f;
    if (preview.skinnedVertices.size() != preview.vertices.size()) {
        preview.skinnedVertices = preview.vertices;
    }

    std::vector<bool> affected(preview.skeleton.size(), false);
    if (!hasEdit && !hasAnimation) {
        for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
            preview.skinnedVertices[index].position = preview.vertices[index].bindPosition;
            preview.skinnedVertices[index].normal = preview.vertices[index].bindNormal;
        }
        return;
    }

    if (hasEdit) {
        const Vec3 pivot = preview.skeleton[preview.selectedBone].worldPosition;
        const Quat editRotation = QuatFromAxisAngle(preview.boneRotationAxis, preview.boneEditAngle);
        for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
            affected[boneIndex] = IsBoneAffectedBySelection(preview, boneIndex, preview.selectedBone);
            if (affected[boneIndex]) {
                preview.skeleton[boneIndex].worldPosition =
                    Add(pivot, Rotate(editRotation, Subtract(preview.skeleton[boneIndex].worldPosition, pivot)));
                preview.skeleton[boneIndex].worldRotation =
                    Multiply(editRotation, preview.skeleton[boneIndex].worldRotation);
            }
        }
    }

    if (HasModelSkinning(preview)) {
        std::vector<Vec3> recordPositions(preview.skinRecords.size());
        for (std::size_t recordIndex = 0; recordIndex < preview.skinRecords.size(); ++recordIndex) {
            recordPositions[recordIndex] =
                ComputeSkinRecordPosition(preview, preview.skinRecords[recordIndex], false);
        }

        for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
            preview.skinnedVertices[index] = preview.vertices[index];
        }

        for (const SkinLookupGroup& group : preview.skinLookupGroups) {
            const std::size_t vertexCount = std::min(group.vertexCount, group.recordIndices.size());
            for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
                const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
                if (modelVertexIndex >= preview.vertices.size() ||
                    recordIndex >= preview.skinRecords.size() ||
                    recordIndex >= recordPositions.size() ||
                    preview.skinRecords[recordIndex].influenceCount <= 0) {
                    continue;
                }

                const ModelVertex& bindVertex = preview.vertices[modelVertexIndex];
                ModelVertex& skinnedVertex = preview.skinnedVertices[modelVertexIndex];
                skinnedVertex = bindVertex;
                skinnedVertex.position = recordPositions[recordIndex];

                Vec3 normal{};
                float totalWeight = 0.0f;
                const SkinBlendRecord& record = preview.skinRecords[recordIndex];
                for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
                    const SkinBlendInfluence& influence = record.influences[influenceIndex];
                    if (influence.bone < 0 ||
                        influence.bone >= static_cast<int>(preview.skeleton.size()) ||
                        influence.weight <= 0.0f) {
                        continue;
                    }
                    const SkeletonBone& bone = preview.skeleton[influence.bone];
                    const Quat bindToCurrent = Multiply(bone.worldRotation, Inverse(bone.bindWorldRotation));
                    normal = Add(normal, Multiply(Rotate(bindToCurrent, bindVertex.bindNormal), influence.weight));
                    totalWeight += influence.weight;
                }
                if (totalWeight > 0.0001f) {
                    skinnedVertex.normal = Normalize(Multiply(normal, 1.0f / totalWeight));
                }
            }
        }
        return;
    }

    for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
        const ModelVertex& bindVertex = preview.vertices[index];
        ModelVertex& skinnedVertex = preview.skinnedVertices[index];
        skinnedVertex = bindVertex;

        if (bindVertex.skinInfluenceCount <= 0) {
            continue;
        }

        Vec3 position{};
        Vec3 normal{};
        float totalWeight = 0.0f;
        for (int influenceIndex = 0; influenceIndex < bindVertex.skinInfluenceCount; ++influenceIndex) {
            const SkinInfluence& influence = bindVertex.skinInfluences[influenceIndex];
            if (influence.bone < 0 || influence.bone >= static_cast<int>(affected.size()) || influence.weight <= 0.0f) {
                continue;
            }

            const SkeletonBone& bone = preview.skeleton[influence.bone];
            const Quat bindToCurrent = Multiply(bone.worldRotation, Inverse(bone.bindWorldRotation));
            const Vec3 influencedPosition =
                Add(bone.worldPosition, Rotate(bindToCurrent, Subtract(bindVertex.bindPosition, bone.bindWorldPosition)));
            const Vec3 influencedNormal = Rotate(bindToCurrent, bindVertex.bindNormal);
            position = Add(position, Multiply(influencedPosition, influence.weight));
            normal = Add(normal, Multiply(influencedNormal, influence.weight));
            totalWeight += influence.weight;
        }

        if (totalWeight > 0.0001f) {
            skinnedVertex.position = Multiply(position, 1.0f / totalWeight);
            skinnedVertex.normal = Normalize(Multiply(normal, 1.0f / totalWeight));
        }
    }
}

void AppendMd2GeometryBlocks(const std::vector<char>& bytes,
                             std::size_t chunkEnd,
                             std::size_t& blockOffset,
                             std::uint32_t blockCount,
                             ModelPreview& model,
                             const std::vector<std::uint32_t>& materialAlphaTypes,
                             const std::vector<float>& materialAlphas) {
    constexpr std::size_t kBlockHeaderSize = 0x64;
    constexpr std::size_t kMaxPreviewVertices = 100000;
    constexpr std::size_t kMaxPreviewTriangles = 100000;
    constexpr std::uint16_t kVertexFlagVector = 1U << 0U;
    constexpr std::uint16_t kVertexFlagNormal = 1U << 1U;
    constexpr std::uint16_t kVertexFlagUv = 1U << 3U;

    for (std::uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
        RequireModelRange(blockOffset, kBlockHeaderSize, chunkEnd, "MD2 geometry block is truncated.");

        const std::uint32_t vectorOffset = ReadU32Le(bytes, blockOffset + 0x3C);
        const std::uint32_t normalOffset = ReadU32Le(bytes, blockOffset + 0x40);
        const std::uint32_t texcoordOffset = ReadU32Le(bytes, blockOffset + 0x48);
        const std::uint32_t vertexStride = ReadU32Le(bytes, blockOffset + 0x4c);
        const std::uint32_t texcoordCount = ReadU32Le(bytes, blockOffset + 0x50);
        const std::uint16_t vertexFlags = ReadU16Le(bytes, blockOffset + 0x54);
        const std::uint16_t vertexCount = ReadU16Le(bytes, blockOffset + 0x56);

        if (vertexCount == 0 ||
            vertexStride < 12 || vertexStride > 256 ||
            (vertexFlags & kVertexFlagVector) == 0 ||
            vectorOffset + 12 > vertexStride) {
            throw std::runtime_error("MD2 geometry block has an unsupported layout.");
        }
        if (model.vertices.size() + vertexCount > kMaxPreviewVertices) {
            throw std::runtime_error("MD2 model is too large for the preview renderer.");
        }

        const std::uint32_t materialIndex = ReadU16Le(bytes, blockOffset + 0x04);
        std::uint32_t textureIndex = ReadU16Le(bytes, blockOffset + 0x20);
        if (textureIndex >= model.materials.size()) {
            textureIndex = model.materials.empty() ? 0 : textureIndex % static_cast<std::uint32_t>(model.materials.size());
        }
        const auto textureCoordinateIndex = static_cast<unsigned char>(bytes[blockOffset + 0x22]);
        const auto textureTiling = static_cast<unsigned char>(bytes[blockOffset + 0x23]);

        const std::size_t vertexStart = blockOffset + kBlockHeaderSize;
        const std::size_t vertexBytes = static_cast<std::size_t>(vertexCount) * vertexStride;
        RequireModelRange(vertexStart, vertexBytes, chunkEnd, "MD2 vertex data is truncated.");

        const std::uint32_t baseVertex = static_cast<std::uint32_t>(model.vertices.size());
        model.vertices.reserve(model.vertices.size() + vertexCount);
        for (std::uint16_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t vertexOffset = vertexStart + static_cast<std::size_t>(vertexIndex) * vertexStride;
            Vec3 position{
                ReadF32Le(bytes, vertexOffset + vectorOffset + 0),
                ReadF32Le(bytes, vertexOffset + vectorOffset + 4),
                ReadF32Le(bytes, vertexOffset + vectorOffset + 8),
            };
            Sanitize(position);

            Vec3 normal{0.0f, 1.0f, 0.0f};
            if ((vertexFlags & kVertexFlagNormal) != 0 && normalOffset + 12 <= vertexStride) {
                normal = {
                    ReadF32Le(bytes, vertexOffset + normalOffset + 0),
                    ReadF32Le(bytes, vertexOffset + normalOffset + 4),
                    ReadF32Le(bytes, vertexOffset + normalOffset + 8),
                };
                Sanitize(normal);
                normal = Normalize(normal);
                if (Length(normal) <= 0.0001f) {
                    normal = {0.0f, 1.0f, 0.0f};
                }
            }

            float u = 0.0f;
            float v = 0.0f;
            std::array<float, 4> uSets = {};
            std::array<float, 4> vSets = {};
            unsigned char uvSetCount = 1;
            if ((vertexFlags & kVertexFlagUv) != 0 && texcoordCount > 0 && texcoordOffset + 8 <= vertexStride) {
                uvSetCount = static_cast<unsigned char>(std::clamp<std::uint32_t>(texcoordCount, 1, 4));
                for (unsigned char uvSet = 0; uvSet < uvSetCount; ++uvSet) {
                    const std::uint32_t uvOffset = texcoordOffset + static_cast<std::uint32_t>(uvSet) * 8U;
                    if (uvOffset + 8 > vertexStride) {
                        break;
                    }
                    float setU = ReadF32Le(bytes, vertexOffset + uvOffset + 0);
                    float setV = ReadF32Le(bytes, vertexOffset + uvOffset + 4);
                    if (!std::isfinite(setU)) {
                        setU = 0.0f;
                    }
                    if (!std::isfinite(setV)) {
                        setV = 0.0f;
                    }
                    uSets[uvSet] = setU;
                    vSets[uvSet] = setV;
                }
                u = uSets[0];
                v = vSets[0];
            }

            ModelVertex vertex{position, normal, u, v, position, normal};
            vertex.uSets = uSets;
            vertex.vSets = vSets;
            vertex.uvSetCount = uvSetCount;
            model.vertices.push_back(vertex);
            IncludeInBounds(model, position);
        }

        const std::uint32_t sectionIndex = static_cast<std::uint32_t>(model.sections.size());
        const std::size_t triangleStart = model.triangles.size();
        std::size_t indexOffset = vertexStart + vertexBytes;
        RequireModelRange(indexOffset, 12, chunkEnd, "MD2 index header is truncated.");
        (void)ReadU32Le(bytes, indexOffset);
        const std::uint32_t fillType = ReadU32Le(bytes, indexOffset + 4);
        const std::uint32_t indexCount = ReadU32Le(bytes, indexOffset + 8);
        indexOffset += 12;

        if (indexCount == 0 || indexOffset + static_cast<std::size_t>(indexCount) * 2 > chunkEnd) {
            throw std::runtime_error("MD2 index data is truncated.");
        }
        if (model.triangles.size() + indexCount > kMaxPreviewTriangles) {
            throw std::runtime_error("MD2 model has too many triangles for the preview renderer.");
        }

        std::vector<std::uint32_t> indices(indexCount);
        for (std::uint32_t index = 0; index < indexCount; ++index) {
            indices[indexCount - 1U - index] = ReadU16Le(bytes, indexOffset + static_cast<std::size_t>(index) * 2);
        }

        const std::uint32_t material = textureIndex;
        if (fillType == 0) {
            for (std::uint32_t index = 0; index + 2 < indexCount; index += 3) {
                const std::uint32_t a = indices[index + 0];
                const std::uint32_t b = indices[index + 1];
                const std::uint32_t c = indices[index + 2];
                if (a < vertexCount && b < vertexCount && c < vertexCount) {
                    model.triangles.push_back({baseVertex + a, baseVertex + b, baseVertex + c, material, sectionIndex});
                }
            }
        } else {
            for (std::uint32_t index = 0; index + 2 < indexCount; ++index) {
                const std::uint32_t a = (index % 2U) == 0U ? indices[index + 0] : indices[index + 1];
                const std::uint32_t b = (index % 2U) == 0U ? indices[index + 1] : indices[index + 0];
                const std::uint32_t c = indices[index + 2];
                if (a == b || b == c || a == c) {
                    continue;
                }
                if (a < vertexCount && b < vertexCount && c < vertexCount) {
                    model.triangles.push_back({baseVertex + a, baseVertex + b, baseVertex + c, material, sectionIndex});
                }
            }
        }
        indexOffset += static_cast<std::size_t>(indexCount) * 2;

        ModelSection section;
        section.textureIndex = textureIndex;
        section.materialIndex = materialIndex;
        section.textureTiling = textureTiling;
        section.textureCoordinateIndex = textureCoordinateIndex;
        if (materialIndex < materialAlphaTypes.size()) {
            section.alphaType = materialAlphaTypes[materialIndex];
        }
        if (materialIndex < materialAlphas.size()) {
            const float alpha = 1.0f - materialAlphas[materialIndex];
            if (std::isfinite(alpha)) {
                section.alpha = std::clamp(alpha, 0.0f, 1.0f);
            }
        }
        section.vertexStart = baseVertex;
        section.vertexCount = vertexCount;
        section.triangleStart = triangleStart;
        section.triangleCount = model.triangles.size() - triangleStart;
        section.name = "Mesh " + std::to_string(model.sections.size() + 1);
        if (textureIndex < model.materials.size()) {
            const std::filesystem::path textureName(model.materials[textureIndex].path);
            if (!textureName.stem().string().empty()) {
                section.name += " - " + textureName.stem().string();
            }
        }
        model.sections.push_back(std::move(section));
        blockOffset = indexOffset;
    }
}

void NormalizeSkinBlendRecord(SkinBlendRecord& record) {
    float totalWeight = 0.0f;
    for (int index = 0; index < record.influenceCount; ++index) {
        if (record.influences[index].bone < 0 || record.influences[index].weight <= 0.0f) {
            record.influences[index].weight = 0.0f;
            continue;
        }
        totalWeight += record.influences[index].weight;
    }

    if (totalWeight <= 0.0001f) {
        record = {};
        return;
    }

    for (int index = 0; index < record.influenceCount; ++index) {
        record.influences[index].weight /= totalWeight;
    }
}

void AssignSkinRecordToVertex(ModelVertex& vertex, const SkinBlendRecord& record, int recordIndex) {
    vertex.skinRecord = recordIndex;
    vertex.skinInfluences = {};
    vertex.skinInfluenceCount = 0;
    for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
        const SkinBlendInfluence& influence = record.influences[influenceIndex];
        if (influence.bone < 0 || influence.weight <= 0.0f) {
            continue;
        }
        vertex.skinInfluences[vertex.skinInfluenceCount++] = {influence.bone, influence.weight};
        if (vertex.skinInfluenceCount >= static_cast<int>(vertex.skinInfluences.size())) {
            break;
        }
    }
}

void DecodeMd2SkinChunk(const std::vector<char>& bytes, std::size_t skinOffset, ModelPreview& model) {
    constexpr std::size_t kBlendRecordSize = 48;
    RequireModelRange(skinOffset, 8, bytes.size(), "MD2 SKN0 chunk header is truncated.");
    const std::size_t payloadSize = ReadU32Le(bytes, skinOffset + 4);
    const std::size_t chunkEnd = skinOffset + 8 + payloadSize;
    RequireModelRange(skinOffset, 8 + payloadSize, bytes.size(), "MD2 SKN0 chunk is truncated.");

    std::size_t cursor = skinOffset + 8;
    RequireModelRange(cursor, 8, chunkEnd, "MD2 SKN0 blend header is truncated.");
    const std::uint32_t recordCount = ReadU32Le(bytes, cursor);
    const std::uint32_t recordBytes = ReadU32Le(bytes, cursor + 4);
    cursor += 8;

    if (recordCount == 0 || recordCount > 200000 ||
        recordBytes != static_cast<std::uint64_t>(recordCount) * kBlendRecordSize) {
        throw std::runtime_error("MD2 SKN0 blend records have an unsupported layout.");
    }
    RequireModelRange(cursor, recordBytes, chunkEnd, "MD2 SKN0 blend records are truncated.");

    model.skinRecords.clear();
    model.skinRecords.reserve(recordCount);
    for (std::uint32_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::size_t recordOffset = cursor + static_cast<std::size_t>(recordIndex) * kBlendRecordSize;
        const std::uint16_t influenceCount = ReadU16Le(bytes, recordOffset + 44);
        if (influenceCount < 1 || influenceCount > 2) {
            throw std::runtime_error("MD2 SKN0 blend record influence count is unsupported.");
        }

        SkinBlendRecord record;
        record.influenceCount = static_cast<int>(influenceCount);
        record.influences[0] = {
            {
                ReadF32Le(bytes, recordOffset + 0),
                ReadF32Le(bytes, recordOffset + 4),
                ReadF32Le(bytes, recordOffset + 8),
            },
            static_cast<int>(ReadS16Le(bytes, recordOffset + 14)),
            influenceCount == 1 ? 1.0f : static_cast<float>(ReadS16Le(bytes, recordOffset + 12)) / 256.0f,
        };
        if (influenceCount > 1) {
            record.influences[1] = {
                {
                    ReadF32Le(bytes, recordOffset + 16),
                    ReadF32Le(bytes, recordOffset + 20),
                    ReadF32Le(bytes, recordOffset + 24),
                },
                static_cast<int>(ReadS16Le(bytes, recordOffset + 30)),
                static_cast<float>(ReadS16Le(bytes, recordOffset + 28)) / 256.0f,
            };
        }
        for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
            Sanitize(record.influences[influenceIndex].localPosition);
        }
        NormalizeSkinBlendRecord(record);
        model.skinRecords.push_back(record);
    }
    cursor += recordBytes;

    RequireModelRange(cursor, 4, chunkEnd, "MD2 SKN0 lookup header is truncated.");
    const std::uint32_t groupCount = ReadU32Le(bytes, cursor);
    cursor += 4;
    if (groupCount == 0 || groupCount > 4096) {
        throw std::runtime_error("MD2 SKN0 lookup group count is unsupported.");
    }

    model.skinLookupGroups.clear();
    model.skinLookupGroups.reserve(groupCount);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        RequireModelRange(cursor, 8, chunkEnd, "MD2 SKN0 lookup group is truncated.");
        const std::uint32_t declaredVertexCount = ReadU32Le(bytes, cursor);
        const std::uint32_t lookupBytes = ReadU32Le(bytes, cursor + 4);
        cursor += 8;

        if (lookupBytes % 2 != 0 || lookupBytes / 2 < declaredVertexCount) {
            throw std::runtime_error("MD2 SKN0 lookup table has an unsupported layout.");
        }
        RequireModelRange(cursor, lookupBytes, chunkEnd, "MD2 SKN0 lookup table is truncated.");

        SkinLookupGroup group;
        if (groupIndex < model.sections.size()) {
            const ModelSection& section = model.sections[groupIndex];
            group.vertexStart = section.vertexStart;
            group.vertexCount = std::min<std::size_t>(declaredVertexCount, section.vertexCount);
        }
        const std::size_t lookupCount = lookupBytes / 2;
        group.recordIndices.reserve(lookupCount);
        for (std::size_t lookupIndex = 0; lookupIndex < lookupCount; ++lookupIndex) {
            group.recordIndices.push_back(ReadU16Le(bytes, cursor + lookupIndex * 2));
        }

        if (group.vertexCount > group.recordIndices.size()) {
            group.vertexCount = group.recordIndices.size();
        }
        for (std::size_t vertexIndex = 0; vertexIndex < group.vertexCount; ++vertexIndex) {
            const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
            const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
            if (modelVertexIndex < model.vertices.size() && recordIndex < model.skinRecords.size()) {
                AssignSkinRecordToVertex(model.vertices[modelVertexIndex], model.skinRecords[recordIndex], recordIndex);
            }
        }

        model.skinLookupGroups.push_back(std::move(group));
        cursor += lookupBytes;
    }
}

std::size_t FindChunkAfter(const std::vector<char>& bytes, std::size_t offset, const char* fourCc) {
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t payloadSize = ReadU32Le(bytes, offset + 4);
        const std::size_t nextOffset = offset + 8 + static_cast<std::size_t>(payloadSize);
        if (nextOffset > bytes.size() || nextOffset <= offset) {
            break;
        }
        if (std::memcmp(bytes.data() + offset, fourCc, 4) == 0) {
            return offset;
        }
        offset = nextOffset;
    }

    return static_cast<std::size_t>(-1);
}

void DecodeMdlHeaderTables(const std::vector<char>& bytes,
                           std::size_t geoOffset,
                           ModelPreview& model,
                           std::vector<std::uint32_t>& materialAlphaTypes,
                           std::vector<float>& materialAlphas) {
    if (bytes.size() < 8) {
        throw std::runtime_error("MD2 model header is truncated.");
    }

    const bool mdl1 = std::memcmp(bytes.data(), "MDL1", 4) == 0;
    const bool mdl2 = std::memcmp(bytes.data(), "MDL2", 4) == 0;
    if (!mdl1 && !mdl2) {
        throw std::runtime_error("Not a supported LR2 MD2 model.");
    }

    const std::size_t payloadSize = ReadU32Le(bytes, 4);
    std::size_t chunkEnd = 8 + payloadSize;
    if (chunkEnd > bytes.size()) {
        throw std::runtime_error("MD2 model header chunk is truncated.");
    }
    if (geoOffset != static_cast<std::size_t>(-1) && geoOffset > 8) {
        chunkEnd = std::min(chunkEnd, geoOffset);
    }

    std::size_t cursor = 8 + 12 + 8;
    RequireModelRange(cursor, 4, chunkEnd, "MD2 model header is truncated.");
    const std::uint32_t hasBoundingBox = ReadU32Le(bytes, cursor);
    cursor += 4;

    if (hasBoundingBox != 0) {
        RequireModelRange(cursor, 12 + 12 + 12 + 4, chunkEnd, "MD2 bounding box header is truncated.");
        cursor += 12 + 12 + 12 + 4;
    }

    RequireModelRange(cursor, 16 + 48, chunkEnd, "MD2 material header is truncated.");
    cursor += 16 + 48;

    RequireModelRange(cursor, 4, chunkEnd, "MD2 texture table count is truncated.");
    const std::uint32_t textureCount = ReadU32Le(bytes, cursor);
    cursor += 4;

    constexpr std::size_t kTextureRecordSize = 256 + 8;
    if (textureCount > 2048) {
        throw std::runtime_error("MD2 texture table count is unsupported.");
    }
    RequireModelRange(
        cursor,
        static_cast<std::size_t>(textureCount) * kTextureRecordSize,
        chunkEnd,
        "MD2 texture table is truncated.");

    model.materials.reserve(textureCount);
    for (std::uint32_t textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
        const std::size_t recordOffset = cursor + static_cast<std::size_t>(textureIndex) * kTextureRecordSize;
        std::string path = ReadFixedString(bytes, recordOffset, 256);
        std::replace(path.begin(), path.end(), '/', '\\');
        model.materials.push_back({Trim(path), {}, false});
    }
    cursor += static_cast<std::size_t>(textureCount) * kTextureRecordSize;

    if (cursor + 4 > chunkEnd) {
        return;
    }

    const std::uint32_t materialCount = ReadU32Le(bytes, cursor);
    cursor += 4;
    if (materialCount > 4096) {
        throw std::runtime_error("MD2 material table count is unsupported.");
    }

    const std::size_t materialRecordSize = mdl2 ? 88 : 28;
    RequireModelRange(
        cursor,
        static_cast<std::size_t>(materialCount) * materialRecordSize,
        chunkEnd,
        "MD2 material table is truncated.");

    materialAlphaTypes.reserve(materialCount);
    materialAlphas.reserve(materialCount);
    for (std::uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        const std::size_t materialOffset = cursor + static_cast<std::size_t>(materialIndex) * materialRecordSize;
        if (mdl2) {
            materialAlphas.push_back(ReadF32Le(bytes, materialOffset + 68));
            materialAlphaTypes.push_back(ReadU32Le(bytes, materialOffset + 72));
        } else {
            materialAlphas.push_back(0.0f);
            materialAlphaTypes.push_back(ReadU32Le(bytes, materialOffset));
        }
    }
}

ModelPreview DecodeMd2Model(const std::vector<char>& bytes) {
    if (bytes.size() >= 12 && std::memcmp(bytes.data(), "MDL0", 4) == 0) {
        throw std::runtime_error("MDL0/GEO0 models are not supported yet.");
    }
    if (bytes.size() < 12 ||
        (std::memcmp(bytes.data(), "MDL1", 4) != 0 &&
         std::memcmp(bytes.data(), "MDL2", 4) != 0)) {
        throw std::runtime_error("Not a supported LR2 MD2 model.");
    }

    std::size_t geoOffset = FindChunkAfter(bytes, 0, "GEO1");
    if (geoOffset == static_cast<std::size_t>(-1)) {
        geoOffset = FindFourCc(bytes, "GEO1");
    }
    if (geoOffset == static_cast<std::size_t>(-1)) {
        if (FindFourCc(bytes, "GEO0") != static_cast<std::size_t>(-1)) {
            throw std::runtime_error("MD2 model uses GEO0, which is not supported yet.");
        }
        throw std::runtime_error("MD2 model does not contain a GEO1 render mesh.");
    }
    RequireModelRange(geoOffset, 0x20, bytes.size(), "MD2 GEO1 header is truncated.");

    const std::size_t payloadSize = ReadU32Le(bytes, geoOffset + 4);
    const std::size_t chunkEnd = geoOffset + 8 + payloadSize;
    RequireModelRange(geoOffset, 8 + payloadSize, bytes.size(), "MD2 GEO1 chunk is truncated.");

    const std::uint32_t primaryBlockCount = ReadU32Le(bytes, geoOffset + 0x14);
    if (primaryBlockCount == 0 || primaryBlockCount > 1024) {
        throw std::runtime_error("MD2 GEO1 block count is unsupported.");
    }

    ModelPreview model;
    const float maxFloat = std::numeric_limits<float>::max();
    model.boundsMin = {maxFloat, maxFloat, maxFloat};
    model.boundsMax = {-maxFloat, -maxFloat, -maxFloat};
    std::vector<std::uint32_t> materialAlphaTypes;
    std::vector<float> materialAlphas;

    DecodeMdlHeaderTables(bytes, geoOffset, model, materialAlphaTypes, materialAlphas);

    std::size_t blockOffset = geoOffset + 0x20;
    AppendMd2GeometryBlocks(bytes, chunkEnd, blockOffset, primaryBlockCount, model, materialAlphaTypes, materialAlphas);

    const std::size_t skinOffset = FindChunkAfter(bytes, chunkEnd, "SKN0");
    if (skinOffset != static_cast<std::size_t>(-1)) {
        try {
            DecodeMd2SkinChunk(bytes, skinOffset, model);
            LogDebug(
                "MD2 SKN0 parsed: records=" + std::to_string(model.skinRecords.size()) +
                " groups=" + std::to_string(model.skinLookupGroups.size()));
        } catch (const std::exception& error) {
            model.skinRecords.clear();
            model.skinLookupGroups.clear();
            for (ModelVertex& vertex : model.vertices) {
                vertex.skinRecord = -1;
                vertex.skinInfluenceCount = 0;
                vertex.skinInfluences = {};
            }
            LogDebug(std::string("MD2 SKN0 parse failed: ") + error.what());
        }
    }

    if (model.vertices.empty() || model.triangles.empty()) {
        throw std::runtime_error("MD2 model did not contain drawable triangles.");
    }

    model.center = {
        (model.boundsMin.x + model.boundsMax.x) * 0.5f,
        (model.boundsMin.y + model.boundsMax.y) * 0.5f,
        (model.boundsMin.z + model.boundsMax.z) * 0.5f,
    };

    model.radius = 0.0f;
    for (const ModelVertex& vertex : model.vertices) {
        model.radius = std::max(model.radius, Length(Subtract(vertex.position, model.center)));
    }
    if (model.radius <= 0.0001f || !std::isfinite(model.radius)) {
        model.radius = 1.0f;
    }

    return model;
}

void IncludeInLevelBounds(LevelPreview& preview, Vec3 position, bool& initialized) {
    if (!initialized) {
        preview.boundsMin = position;
        preview.boundsMax = position;
        initialized = true;
        return;
    }

    preview.boundsMin.x = std::min(preview.boundsMin.x, position.x);
    preview.boundsMin.y = std::min(preview.boundsMin.y, position.y);
    preview.boundsMin.z = std::min(preview.boundsMin.z, position.z);
    preview.boundsMax.x = std::max(preview.boundsMax.x, position.x);
    preview.boundsMax.y = std::max(preview.boundsMax.y, position.y);
    preview.boundsMax.z = std::max(preview.boundsMax.z, position.z);
}

void RecomputeLevelBounds(LevelPreview& preview) {
    bool initialized = false;
    for (const LevelVertex& vertex : preview.vertices) {
        IncludeInLevelBounds(preview, vertex.position, initialized);
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition) {
            IncludeInLevelBounds(preview, object.position, initialized);
        }
    }

    if (!initialized) {
        preview.boundsMin = {-1.0f, -1.0f, -1.0f};
        preview.boundsMax = {1.0f, 1.0f, 1.0f};
    }

    preview.center = {
        (preview.boundsMin.x + preview.boundsMax.x) * 0.5f,
        (preview.boundsMin.y + preview.boundsMax.y) * 0.5f,
        (preview.boundsMin.z + preview.boundsMax.z) * 0.5f,
    };

    preview.radius = 0.0f;
    for (const LevelVertex& vertex : preview.vertices) {
        preview.radius = std::max(preview.radius, Length(Subtract(vertex.position, preview.center)));
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition) {
            preview.radius = std::max(preview.radius, Length(Subtract(object.position, preview.center)));
        }
    }
    if (preview.radius <= 0.0001f || !std::isfinite(preview.radius)) {
        preview.radius = 1.0f;
    }
}

LevelPreview DecodeTdfTerrain(const std::vector<char>& bytes, std::string name, std::string path) {
    constexpr int kGridLine = 32;
    constexpr int kGridCount = kGridLine * kGridLine;
    constexpr std::size_t kHeaderSize = 0x20;
    constexpr std::size_t kHeightPointSize = 8;
    constexpr std::size_t kTerrainGridInfoSize = 0x120;
    constexpr std::size_t kGridStartOffset = 0x0C;
    constexpr std::size_t kGridMapInfoOffset = 0x90;

    if (bytes.size() < kHeaderSize || std::memcmp(bytes.data(), "TDF1", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 TDF terrain file.");
    }

    const int terrainWidth = static_cast<int>(ReadU32Le(bytes, 0x08));
    const int terrainDepth = static_cast<int>(ReadU32Le(bytes, 0x0C));
    float filterScale = ReadF32Le(bytes, 0x10);
    int allocatedMipLevels = static_cast<int>(ReadU32Le(bytes, 0x14));
    int mipLevels = static_cast<int>(ReadU32Le(bytes, 0x18));

    if (terrainWidth <= 0 || terrainDepth <= 0 || allocatedMipLevels <= 0 || mipLevels <= 0) {
        throw std::runtime_error("TDF terrain header dimensions are unsupported.");
    }
    if (!std::isfinite(filterScale) || std::abs(filterScale) < 0.000001f) {
        filterScale = 1.0f / 64.0f;
    }

    allocatedMipLevels = std::clamp(allocatedMipLevels, 1, 4);
    mipLevels = std::clamp(mipLevels, 1, 4);

    const int stepX = terrainWidth / kGridLine;
    const int stepY = terrainDepth / kGridLine;
    if (stepX <= 0 || stepY <= 0) {
        throw std::runtime_error("TDF terrain grid dimensions are unsupported.");
    }

    std::array<std::size_t, 4> pointOffsets = {};
    std::size_t cursor = kHeaderSize;
    int mipWidth = stepX + 1;
    int mipHeight = stepY + 1;
    for (int mipLevel = 0; mipLevel < allocatedMipLevels; ++mipLevel) {
        pointOffsets[static_cast<std::size_t>(mipLevel)] = cursor;
        const std::size_t pointCount =
            static_cast<std::size_t>(mipWidth) *
            static_cast<std::size_t>(mipHeight) *
            static_cast<std::size_t>(kGridCount);
        const std::size_t pointBytes = pointCount * kHeightPointSize;
        if (cursor > bytes.size() || pointBytes > bytes.size() - cursor) {
            throw std::runtime_error("TDF terrain height points are truncated.");
        }
        cursor += pointBytes;
        mipWidth = ((mipWidth - 1) / 2) + 1;
        mipHeight = ((mipHeight - 1) / 2) + 1;
    }

    mipWidth = stepX + 1;
    mipHeight = stepY + 1;
    for (int mipLevel = 0; mipLevel < 4; ++mipLevel) {
        if (mipLevel + 1 >= mipLevels) {
            break;
        }
        const std::size_t edgeCount =
            static_cast<std::size_t>(mipWidth + mipHeight) *
            2U *
            static_cast<std::size_t>(kGridCount);
        const std::size_t edgeBytes = edgeCount * sizeof(std::uint16_t);
        if (cursor > bytes.size() || edgeBytes > bytes.size() - cursor) {
            throw std::runtime_error("TDF terrain edge data is truncated.");
        }
        cursor += edgeBytes;
        mipWidth = ((mipWidth - 1) / 2) + 1;
        mipHeight = ((mipHeight - 1) / 2) + 1;
    }

    const std::size_t gridInfoOffset = cursor;
    const std::size_t gridInfoBytes = static_cast<std::size_t>(kGridCount) * kTerrainGridInfoSize;
    if (gridInfoOffset > bytes.size() || gridInfoBytes > bytes.size() - gridInfoOffset) {
        throw std::runtime_error("TDF terrain grid info is truncated.");
    }
    const std::size_t gridOffsetTableOffset = gridInfoOffset + gridInfoBytes;
    const std::size_t gridOffsetTableBytes = static_cast<std::size_t>(kGridCount) * sizeof(std::uint32_t);
    if (gridOffsetTableOffset > bytes.size() || gridOffsetTableBytes > bytes.size() - gridOffsetTableOffset) {
        throw std::runtime_error("TDF terrain grid offset table is truncated.");
    }

    LevelPreview preview;
    preview.open = true;
    preview.world = false;
    preview.name = std::move(name);
    preview.path = std::move(path);

    const std::size_t estimatedVertices =
        static_cast<std::size_t>(kGridCount) *
        static_cast<std::size_t>(stepX + 1) *
        static_cast<std::size_t>(stepY + 1);
    preview.vertices.reserve(estimatedVertices);
    preview.triangles.reserve(static_cast<std::size_t>(kGridCount) * stepX * stepY * 2U);

    int skippedTriangles = 0;
    const float terrainCenterX = static_cast<float>(stepX * kGridLine) * 0.5f;
    const float terrainCenterZ = static_cast<float>(stepY * kGridLine) * 0.5f;
    const float uDenominator = static_cast<float>(std::max(1, stepX));
    const float vDenominator = static_cast<float>(std::max(1, stepY));
    for (int chunkIndex = 0; chunkIndex < kGridCount; ++chunkIndex) {
            const std::uint32_t gridRelativeOffset =
                ReadU32Le(bytes, gridOffsetTableOffset + static_cast<std::size_t>(chunkIndex) * sizeof(std::uint32_t));
            const std::size_t gridOffset = gridInfoOffset + static_cast<std::size_t>(gridRelativeOffset);
            if (gridOffset < gridInfoOffset || gridOffset + kTerrainGridInfoSize > gridOffsetTableOffset) {
                continue;
            }
            const int startX = static_cast<int>(ReadS16Le(bytes, gridOffset + kGridStartOffset + 0));
            const int startY = static_cast<int>(ReadS16Le(bytes, gridOffset + kGridStartOffset + 2));
            const std::uint32_t materialKey =
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x118])) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x119])) << 8) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x11A])) << 16) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x11B])) << 24);

            const std::size_t mapOffset = gridOffset + kGridMapInfoOffset;
            const std::uint32_t pointByteOffset = ReadU32Le(bytes, mapOffset + 0);
            const int numX = static_cast<int>(ReadS16Le(bytes, mapOffset + 4));
            const int numY = static_cast<int>(ReadS16Le(bytes, mapOffset + 6));
            if (numX < 2 || numY < 2 || numX > 128 || numY > 128 || pointByteOffset % kHeightPointSize != 0) {
                continue;
            }

            const std::size_t pointBaseOffset = pointOffsets[0] + static_cast<std::size_t>(pointByteOffset);
            const std::size_t pointBytes =
                static_cast<std::size_t>(numX) *
                static_cast<std::size_t>(numY) *
                kHeightPointSize;
            if (pointBaseOffset > bytes.size() || pointBytes > bytes.size() - pointBaseOffset) {
                continue;
            }

            const std::uint32_t baseVertex = static_cast<std::uint32_t>(preview.vertices.size());
            for (int y = 0; y < numY; ++y) {
                for (int x = 0; x < numX; ++x) {
                    const std::size_t pointOffset =
                        pointBaseOffset +
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(numX) + static_cast<std::size_t>(x)) *
                            kHeightPointSize;

                    const std::uint16_t rawHeight = ReadU16Le(bytes, pointOffset + 0);
                    const auto normalX = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 2])));
                    const auto normalY = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 3])));
                    const auto normalZ = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 4])));
                    const unsigned char flags = static_cast<unsigned char>(bytes[pointOffset + 5]);
                    const std::uint16_t mixRatios = ReadU16Le(bytes, pointOffset + 6);

                    const float sourceX = static_cast<float>(startX + x);
                    const float sourceZ = static_cast<float>(startY + y);
                    const float worldX = sourceX - terrainCenterX;
                    const float worldZ = sourceZ - terrainCenterZ;
                    const float worldY = static_cast<float>(rawHeight) * filterScale;

                    LevelVertex vertex;
                    vertex.position = {worldX, worldY, worldZ};
                    vertex.normal = Normalize(Vec3{normalX, normalY, normalZ});
                    if (Length(vertex.normal) <= 0.0001f) {
                        vertex.normal = {0.0f, 1.0f, 0.0f};
                    }
                    vertex.u = sourceX / uDenominator;
                    vertex.v = sourceZ / vDenominator;
                    vertex.mix = {
                        static_cast<unsigned char>(((mixRatios >> 0x0) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0x4) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0x8) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0xc) & 0xfU) * 0x11U),
                    };
                    vertex.visible = (flags & 0x80U) == 0;
                    preview.vertices.push_back(vertex);
                }
            }

            for (int y = 0; y + 1 < numY; ++y) {
                for (int x = 0; x + 1 < numX; ++x) {
                    const std::uint32_t a = baseVertex + static_cast<std::uint32_t>(y * numX + x);
                    const std::uint32_t b = a + 1;
                    const std::uint32_t c = baseVertex + static_cast<std::uint32_t>((y + 1) * numX + x);
                    const std::uint32_t d = c + 1;
                    if (!preview.vertices[d].visible) {
                        skippedTriangles += 2;
                        continue;
                    }
                    if (y % 2 == 0) {
                        preview.triangles.push_back({a, b, c, materialKey});
                        preview.triangles.push_back({c, b, d, materialKey});
                    } else {
                        preview.triangles.push_back({a, b, d, materialKey});
                        preview.triangles.push_back({a, d, c, materialKey});
                    }
                }
            }
    }

    RecomputeLevelBounds(preview);
    preview.status =
        std::to_string(kGridLine) + "x" + std::to_string(kGridLine) +
        " tiled LOD0 terrain, " + std::to_string(preview.vertices.size()) + " vertices, " +
        std::to_string(preview.triangles.size()) + " triangles";
    if (skippedTriangles > 0) {
        preview.status += ", " + std::to_string(skippedTriangles) + " cutout triangles skipped";
    }
    LogDebug(
        "TDF terrain decoded: " + preview.path +
        " width=" + std::to_string(terrainWidth) +
        " depth=" + std::to_string(terrainDepth) +
        " filter_scale=" + std::to_string(filterScale) +
        " grid_info=" + FormatHex32(static_cast<std::uint32_t>(gridInfoOffset)) +
        " grid_offsets=" + FormatHex32(static_cast<std::uint32_t>(gridOffsetTableOffset)) +
        " vertices=" + std::to_string(preview.vertices.size()) +
        " triangles=" + std::to_string(preview.triangles.size()) +
        " skipped=" + std::to_string(skippedTriangles));
    return preview;
}

bool IsPlausibleWorldPosition(Vec3 position) {
    return std::isfinite(position.x) &&
           std::isfinite(position.y) &&
           std::isfinite(position.z) &&
           std::abs(position.x) < 5000.0f &&
           std::abs(position.y) < 2500.0f &&
           std::abs(position.z) < 5000.0f &&
           (std::abs(position.x) + std::abs(position.y) + std::abs(position.z)) > 0.001f;
}

std::string NormalizeWorldAssetPath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    path = Trim(std::move(path));
    while (!path.empty() && (path.back() == '\\' || path.back() == '/')) {
        path.pop_back();
    }
    return path;
}

bool IsWorldAssetPath(const std::string& path) {
    const std::string lower = ToLower(path);
    return lower == "game data" || lower.rfind("game data\\", 0) == 0;
}

bool IsWorldMd2Path(const std::string& path) {
    if (!IsWorldAssetPath(path)) {
        return false;
    }
    return ToLower(std::filesystem::path(path).extension().string()) == ".md2";
}

void AddUniqueWorldAssetPath(std::vector<std::string>& paths, std::string path) {
    path = NormalizeWorldAssetPath(std::move(path));
    if (!IsWorldAssetPath(path)) {
        return;
    }

    const std::string wanted = ToLower(path);
    const auto found = std::find_if(paths.begin(), paths.end(), [&](const std::string& existing) {
        return ToLower(existing) == wanted;
    });
    if (found == paths.end()) {
        paths.push_back(std::move(path));
    }
}

std::vector<std::string> ExtractWorldRecordPaths(const std::vector<char>& bytes,
                                                 std::size_t start,
                                                 std::size_t end) {
    std::vector<std::string> paths;
    if (start >= end || start >= bytes.size()) {
        return paths;
    }

    end = std::min(end, bytes.size());
    constexpr char kNeedle[] = "game data";
    for (std::size_t offset = start; offset + 9 <= end; ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < 9; ++index) {
            const char source = static_cast<char>(std::tolower(static_cast<unsigned char>(bytes[offset + index])));
            if (source != kNeedle[index]) {
                matched = false;
                break;
            }
        }
        if (!matched || offset + 9 >= end || (bytes[offset + 9] != '\\' && bytes[offset + 9] != '/')) {
            continue;
        }

        std::string path;
        for (std::size_t pathOffset = offset; pathOffset < end && path.size() < 260; ++pathOffset) {
            const auto character = static_cast<unsigned char>(bytes[pathOffset]);
            if (character == '\0') {
                break;
            }
            if (character < 32 || character > 126) {
                break;
            }
            path.push_back(character == '/' ? '\\' : static_cast<char>(character));
        }
        const std::size_t consumed = path.size();
        AddUniqueWorldAssetPath(paths, std::move(path));
        if (consumed > 0) {
            offset += consumed - 1;
        }
    }
    return paths;
}

std::string FirstWorldMd2Path(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        if (IsWorldMd2Path(path)) {
            return path;
        }
    }
    return {};
}

bool TryReadWorldVec3(const std::vector<char>& bytes,
                      std::size_t recordOffset,
                      std::size_t recordEnd,
                      std::size_t relativeOffset,
                      Vec3& position,
                      bool allowZero = false) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return false;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 12) {
        return false;
    }

    const std::size_t offset = recordOffset + relativeOffset;
    position = {
        ReadF32Le(bytes, offset + 0),
        ReadF32Le(bytes, offset + 4),
        ReadF32Le(bytes, offset + 8),
    };
    Sanitize(position);
    if (IsPlausibleWorldPosition(position)) {
        return true;
    }
    return allowZero &&
           std::isfinite(position.x) &&
           std::isfinite(position.y) &&
           std::isfinite(position.z) &&
           std::abs(position.x) < 5000.0f &&
           std::abs(position.y) < 2500.0f &&
           std::abs(position.z) < 5000.0f;
}

bool TryReadAiSectionPosition(const std::vector<char>& bytes,
                              std::size_t recordOffset,
                              std::size_t recordEnd,
                              Vec3& position) {
    Vec3 sum = {};
    int count = 0;
    for (int pointIndex = 0; pointIndex < 8; ++pointIndex) {
        Vec3 point = {};
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, 0xA4 + static_cast<std::size_t>(pointIndex) * 0x24, point)) {
            sum.x += point.x;
            sum.y += point.y;
            sum.z += point.z;
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }

    position = {
        sum.x / static_cast<float>(count),
        sum.y / static_cast<float>(count),
        sum.z / static_cast<float>(count),
    };
    return true;
}

bool TryReadSplinePosition(const std::vector<char>& bytes,
                           std::size_t recordOffset,
                           std::size_t recordEnd,
                           Vec3& position) {
    if (recordOffset + 0x60 > recordEnd || recordOffset + 0x60 > bytes.size()) {
        return false;
    }

    const std::uint32_t pointCount = std::min<std::uint32_t>(ReadU32Le(bytes, recordOffset + 0x5C), 64);
    if (pointCount == 0) {
        return false;
    }

    Vec3 sum = {};
    int count = 0;
    for (std::uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        Vec3 point = {};
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, 0x68 + static_cast<std::size_t>(pointIndex) * 0x20, point)) {
            sum.x += point.x;
            sum.y += point.y;
            sum.z += point.z;
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }

    position = {
        sum.x / static_cast<float>(count),
        sum.y / static_cast<float>(count),
        sum.z / static_cast<float>(count),
    };
    return true;
}

float ReadWrlFloatOrDefault(const std::vector<char>& bytes,
                            std::size_t recordOffset,
                            std::size_t recordEnd,
                            std::size_t relativeOffset,
                            float fallback) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return fallback;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 4) {
        return fallback;
    }

    const float value = ReadF32Le(bytes, recordOffset + relativeOffset);
    if (!std::isfinite(value) || std::abs(value) < 0.0001f || std::abs(value) > 1024.0f) {
        return fallback;
    }
    return value;
}

void AddUniqueTerrainLink(std::vector<WrlTerrainLink>& links, WrlTerrainLink link) {
    link.path = NormalizeWorldAssetPath(std::move(link.path));
    if (!IsWorldAssetPath(link.path)) {
        return;
    }

    const std::string wantedPath = ToLower(link.path);
    const auto found = std::find_if(links.begin(), links.end(), [&](const WrlTerrainLink& existing) {
        return ToLower(existing.path) == wantedPath &&
               existing.hasLayer == link.hasLayer &&
               (!existing.hasLayer || existing.layer == link.layer);
    });
    if (found == links.end()) {
        links.push_back(std::move(link));
    }
}

bool ClassUsesStandardWrlPosition(const std::string& classLower) {
    return classLower == "cracestartpos" ||
           classLower == "cfoyerstartpos" ||
           classLower == "cgeneralstatic" ||
           classLower == "cgoldenbrick" ||
           classLower == "cgeneralmobile" ||
           classLower == "cbonuspickup" ||
           classLower == "cbonusvortex" ||
           classLower == "cawarddisplay" ||
           classLower == "cfirework" ||
           classLower == "cmapmarker" ||
           classLower == "ccheckpoint" ||
           classLower == "cthrustermobile" ||
           classLower == "cskeleton" ||
           classLower == "clegostompermech" ||
           classLower == "cwheel" ||
           classLower == "cthepits" ||
           classLower == "cweaponpickup" ||
           classLower == "ccinematiccamera" ||
           classLower == "cpatch" ||
           classLower == "clegoterrain" ||
           classLower == "cfoyerhotspot" ||
           classLower == "csteampoint" ||
           classLower == "ckillbox" ||
           classLower == "ckillsphere" ||
           classLower == "cscripttrigger";
}

bool TryReadWorldObjectPosition(const std::vector<char>& bytes,
                                std::size_t recordOffset,
                                std::size_t recordEnd,
                                const std::string& className,
                                Vec3& position) {
    const std::string classLower = ToLower(className);
    if (classLower == "caisection") {
        return TryReadAiSectionPosition(bytes, recordOffset, recordEnd, position);
    }
    if (classLower == "cspline") {
        return TryReadSplinePosition(bytes, recordOffset, recordEnd, position);
    }

    std::vector<std::size_t> candidates;
    if (classLower == "cwatersheet" || classLower == "cwatercourse") {
        candidates.push_back(0x158);
    } else if (classLower == "cboulderspitter") {
        candidates.push_back(0x68);
    } else if (classLower == "cpointlight") {
        candidates.push_back(0x88);
    } else if (classLower == "cmodelandspotlight") {
        candidates.push_back(0x88);
        candidates.push_back(0x68);
    } else if (classLower == "caiquad") {
        candidates.push_back(0x88);
    } else if (classLower == "cminifig") {
        candidates.push_back(0x5C);
    } else if (classLower == "ccamera") {
        candidates.push_back(0x70);
    } else if (ClassUsesStandardWrlPosition(classLower)) {
        candidates.push_back(0x58);
    }

    for (std::size_t relativeOffset : candidates) {
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, relativeOffset, position, ClassUsesStandardWrlPosition(classLower))) {
            return true;
        }
    }
    return false;
}

bool IsWrlRecordAt(const std::vector<char>& bytes, std::size_t offset) {
    return offset + 4 <= bytes.size() &&
           bytes[offset + 0] == 'O' &&
           bytes[offset + 1] == 'B' &&
           bytes[offset + 2] == 'M' &&
           bytes[offset + 3] == 'G';
}

std::size_t FindNextWrlRecord(const std::vector<char>& bytes, std::size_t start) {
    for (std::size_t offset = start; offset + 4 <= bytes.size(); ++offset) {
        if (IsWrlRecordAt(bytes, offset)) {
            return offset;
        }
    }
    return static_cast<std::size_t>(-1);
}

struct WrlObjectRecord {
    std::size_t offset = 0;
    std::size_t end = 0;
    std::uint32_t payloadSize = 0;
    bool lengthTrusted = false;
};

std::vector<WrlObjectRecord> FindWrlObjectRecords(const std::vector<char>& bytes) {
    std::vector<WrlObjectRecord> records;
    std::size_t offset = 8;
    while (offset + 0x24 <= bytes.size()) {
        if (!IsWrlRecordAt(bytes, offset)) {
            offset = FindNextWrlRecord(bytes, offset + 1);
            if (offset == static_cast<std::size_t>(-1)) {
                break;
            }
        }

        const std::uint32_t payloadSize = ReadU32Le(bytes, offset + 0x20);
        std::size_t recordEnd = offset + 0x24 + static_cast<std::size_t>(payloadSize);
        bool lengthTrusted =
            payloadSize > 0 &&
            payloadSize < 0x20000 &&
            recordEnd <= bytes.size();

        if (!lengthTrusted) {
            const std::size_t next = FindNextWrlRecord(bytes, offset + 4);
            recordEnd = next == static_cast<std::size_t>(-1) ? bytes.size() : next;
        }
        if (recordEnd <= offset || recordEnd > bytes.size()) {
            break;
        }

        records.push_back({offset, recordEnd, payloadSize, lengthTrusted});
        offset = recordEnd;
    }
    return records;
}

std::string ReadWrlAssetPathAt(const std::vector<char>& bytes,
                               std::size_t recordOffset,
                               std::size_t recordEnd,
                               std::size_t relativeOffset,
                               std::size_t maxLength) {
    return NormalizeWorldAssetPath(ReadBoundedString(bytes, recordOffset + relativeOffset, recordEnd, maxLength));
}

bool TryReadWorldQuat(const std::vector<char>& bytes,
                      std::size_t recordOffset,
                      std::size_t recordEnd,
                      std::size_t relativeOffset,
                      Quat& rotation) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return false;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 16) {
        return false;
    }

    const std::size_t offset = recordOffset + relativeOffset;
    rotation = Normalize({
        ReadF32Le(bytes, offset + 0),
        ReadF32Le(bytes, offset + 4),
        ReadF32Le(bytes, offset + 8),
        ReadF32Le(bytes, offset + 12),
    });
    return std::isfinite(rotation.x) &&
           std::isfinite(rotation.y) &&
           std::isfinite(rotation.z) &&
           std::isfinite(rotation.w);
}

std::size_t WrlModelPathOffsetForClass(const std::string& classLower) {
    if (classLower == "cgeneralstatic" ||
        classLower == "cgoldenbrick" ||
        classLower == "cbonusvortex" ||
        classLower == "cawarddisplay" ||
        classLower == "ccheckpoint" ||
        classLower == "cpatch" ||
        classLower == "cthepits" ||
        classLower == "cweaponpickup") {
        return 0x80;
    }
    if (classLower == "cgeneralmobile" ||
        classLower == "cbonuspickup" ||
        classLower == "cthrustermobile" ||
        classLower == "cskeleton" ||
        classLower == "clegostompermech" ||
        classLower == "cwheel") {
        return 0x90;
    }
    if (classLower == "cmodelandspotlight") {
        return 0xBC;
    }
    if (classLower == "cskybox") {
        return 0x58;
    }
    return static_cast<std::size_t>(-1);
}

LevelPreview DecodeWrlWorld(const std::vector<char>& bytes, std::string name, std::string path) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RC2W", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 WRL saved world.");
    }

    LevelPreview preview;
    preview.open = true;
    preview.world = true;
    preview.name = std::move(name);
    preview.path = std::move(path);

    const std::vector<WrlObjectRecord> records = FindWrlObjectRecords(bytes);
    LogDebug(
        "WRL decode: " + preview.path +
        " bytes=" + std::to_string(bytes.size()) +
        " records=" + std::to_string(records.size()));
    preview.objects.reserve(records.size());
    int untrustedLengths = 0;
    for (const WrlObjectRecord& record : records) {
        if (record.end <= record.offset + 0x24) {
            continue;
        }
        if (!record.lengthTrusted) {
            ++untrustedLengths;
        }

        WorldObject object;
        object.className = ReadBoundedString(bytes, record.offset + 4, record.end, 24);
        object.recordSize = static_cast<std::uint32_t>(std::min<std::size_t>(
            record.end - record.offset,
            std::numeric_limits<std::uint32_t>::max()));
        if (record.offset + 0x28 <= record.end) {
            object.layer = ReadU32Le(bytes, record.offset + 0x24);
            object.hasLayer = true;
        }
        object.name = ReadBoundedString(bytes, record.offset + 0x28, record.end, 24);
        object.binding = ReadBoundedString(bytes, record.offset + 0x40, record.end, 24);
        object.assetPaths = ExtractWorldRecordPaths(bytes, record.offset, record.end);

        if (object.className.empty()) {
            object.className = "Unknown";
        }
        if (object.name.empty()) {
            object.name = object.className;
        }
        if (!object.assetPaths.empty()) {
            object.assetPath = object.assetPaths.front();
        }

        const std::string classLower = ToLower(object.className);
        object.hasPosition =
            TryReadWorldObjectPosition(bytes, record.offset, record.end, object.className, object.position);
        object.hasRotation = TryReadWorldQuat(
            bytes,
            record.offset,
            record.end,
            classLower == "cwatersheet" ? 0x164 : 0x64,
            object.rotation);
        const std::size_t modelPathOffset = WrlModelPathOffsetForClass(classLower);
        if (modelPathOffset != static_cast<std::size_t>(-1)) {
            object.modelPath = ReadWrlAssetPathAt(bytes, record.offset, record.end, modelPathOffset, 0x80);
            if (IsWorldAssetPath(object.modelPath)) {
                AddUniqueWorldAssetPath(object.assetPaths, object.modelPath);
                object.assetPath = object.modelPath;
            } else {
                object.modelPath.clear();
            }
        }
        if (object.modelPath.empty()) {
            object.modelPath = FirstWorldMd2Path(object.assetPaths);
            if (!object.modelPath.empty()) {
                object.assetPath = object.modelPath;
            }
        }
        if (classLower == "clegoterrain") {
            const std::string terrainDirectory = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0x80, 128);
            WrlTerrainLink terrainLink;
            terrainLink.layer = object.layer;
            terrainLink.hasLayer = object.hasLayer;
            terrainLink.position = object.hasPosition ? object.position : Vec3{};
            if (TryReadWorldVec3(bytes, record.offset, record.end, 0x104, terrainLink.scale)) {
                terrainLink.scale.x = std::abs(terrainLink.scale.x) < 0.0001f ? 1.0f : terrainLink.scale.x;
                terrainLink.scale.y = std::abs(terrainLink.scale.y) < 0.0001f ? 1.0f : terrainLink.scale.y;
                terrainLink.scale.z = std::abs(terrainLink.scale.z) < 0.0001f ? 1.0f : terrainLink.scale.z;
            }
            terrainLink.textureScaleX = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x120, 1.0f);
            terrainLink.textureScaleY = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x124, 1.0f);
            if (IsWorldAssetPath(terrainDirectory)) {
                terrainLink.path = terrainDirectory;
                AddUniqueWorldAssetPath(object.assetPaths, terrainDirectory);
                object.assetPath = terrainDirectory;
            } else if (!object.assetPath.empty()) {
                terrainLink.path = object.assetPath;
            }
            AddUniqueTerrainLink(preview.terrainLinks, std::move(terrainLink));
            if (preview.terrainPath.empty() && !preview.terrainLinks.empty()) {
                preview.terrainPath = preview.terrainLinks.front().path;
            }
        } else if (classLower == "cwatersheet") {
            LevelWaterSheet waterSheet;
            waterSheet.objectIndex = static_cast<int>(preview.objects.size());
            waterSheet.position = object.hasPosition ? object.position : Vec3{};
            waterSheet.rotation = object.hasRotation ? object.rotation : Quat{};
            waterSheet.texturePath = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0x58, 0x80);
            waterSheet.reflectionTexturePath = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0xD8, 0x80);

            const float width = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x17C, waterSheet.width);
            const float depth = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x180, waterSheet.depth);
            const float uScale = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x184, waterSheet.uScale);
            const float vScale = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x188, waterSheet.vScale);
            const float alpha = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x194, waterSheet.alpha);
            if (std::isfinite(width) && width > 0.001f && width < 10000.0f) {
                waterSheet.width = width;
            }
            if (std::isfinite(depth) && depth > 0.001f && depth < 10000.0f) {
                waterSheet.depth = depth;
            }
            if (std::isfinite(uScale) && uScale > 0.001f && uScale < 1024.0f) {
                waterSheet.uScale = uScale;
            }
            if (std::isfinite(vScale) && vScale > 0.001f && vScale < 1024.0f) {
                waterSheet.vScale = vScale;
            }
            if (std::isfinite(alpha) && alpha >= 0.15f && alpha <= 0.90f) {
                waterSheet.alpha = alpha;
            }

            if (IsWorldAssetPath(waterSheet.texturePath)) {
                AddUniqueWorldAssetPath(object.assetPaths, waterSheet.texturePath);
                object.assetPath = waterSheet.texturePath;
            } else {
                waterSheet.texturePath.clear();
            }
            if (IsWorldAssetPath(waterSheet.reflectionTexturePath)) {
                AddUniqueWorldAssetPath(object.assetPaths, waterSheet.reflectionTexturePath);
            } else {
                waterSheet.reflectionTexturePath.clear();
            }
            preview.waterSheets.push_back(std::move(waterSheet));
        }

        ++preview.classCounts[object.className];
        preview.objects.push_back(std::move(object));
    }

    RecomputeLevelBounds(preview);
    preview.status = std::to_string(preview.objects.size()) + " world objects";
    if (!preview.terrainLinks.empty()) {
        preview.status += ", " + std::to_string(preview.terrainLinks.size()) + " terrain link";
        if (preview.terrainLinks.size() != 1) {
            preview.status += "s";
        }
    }
    if (!preview.waterSheets.empty()) {
        preview.status += ", " + std::to_string(preview.waterSheets.size()) + " water sheet";
        if (preview.waterSheets.size() != 1) {
            preview.status += "s";
        }
    }
    if (untrustedLengths > 0) {
        preview.status += ", " + std::to_string(untrustedLengths) + " guessed records";
    }
    LogDebug(
        "WRL decode complete: " + preview.path +
        " objects=" + std::to_string(preview.objects.size()) +
        " terrain=" + preview.terrainPath +
        " water_sheets=" + std::to_string(preview.waterSheets.size()) +
        " untrusted_lengths=" + std::to_string(untrustedLengths));
    return preview;
}

void MergeTerrainIntoWorld(LevelPreview& world, LevelPreview terrain) {
    world.vertices = std::move(terrain.vertices);
    world.triangles = std::move(terrain.triangles);
    RecomputeLevelBounds(world);
}

void AddTerrainSectionToWorld(LevelPreview& world, LevelPreview terrain, const WrlTerrainLink& link) {
    LevelTerrainSection section;
    section.path = terrain.path;
    section.layer = link.layer;
    section.hasLayer = link.hasLayer;
    section.position = link.position;
    section.scale = link.scale;
    section.textureScaleX = link.textureScaleX;
    section.textureScaleY = link.textureScaleY;
    section.vertices = std::move(terrain.vertices);
    section.triangles = std::move(terrain.triangles);
    for (LevelVertex& vertex : section.vertices) {
        vertex.position = {
            section.position.x + vertex.position.x * section.scale.x,
            section.position.y + vertex.position.y * section.scale.y,
            section.position.z + vertex.position.z * section.scale.z,
        };
        vertex.normal = Normalize(Vec3{
            vertex.normal.x / section.scale.x,
            vertex.normal.y / section.scale.y,
            vertex.normal.z / section.scale.z,
        });
        if (Length(vertex.normal) <= 0.0001f) {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
    }

    const std::uint32_t baseVertex = static_cast<std::uint32_t>(world.vertices.size());
    world.vertices.insert(world.vertices.end(), section.vertices.begin(), section.vertices.end());
    for (const LevelTriangle& triangle : section.triangles) {
        world.triangles.push_back({
            triangle.a + baseVertex,
            triangle.b + baseVertex,
            triangle.c + baseVertex,
            triangle.materialKey,
        });
    }
    world.terrainSections.push_back(std::move(section));
    RecomputeLevelBounds(world);
}

std::size_t ResolveTerrainTextureEntryIndex(const AppState& state, const std::string& terrainPath) {
    const std::string parentPath = ParentArchivePath(terrainPath);
    std::vector<std::string> candidates;
    candidates.push_back(JoinArchivePathString(parentPath, "TerrainMipped.TGA"));
    candidates.push_back(JoinArchivePathString(parentPath, "TERRAINMIPPED.TGA"));
    candidates.push_back(JoinArchivePathString(parentPath, "terrainmipped.tga"));
    candidates.push_back(JoinArchivePathString(parentPath, "TERRAINMIPPED.MIP"));
    candidates.push_back(JoinArchivePathString(parentPath, "TerrainMipped.MIP"));
    candidates.push_back(JoinArchivePathString(parentPath, "terrainmipped.mip"));

    for (std::string candidate : candidates) {
        std::replace(candidate.begin(), candidate.end(), '/', '\\');
        const std::size_t entryIndex = FindArchiveEntryIndex(state, candidate);
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::size_t ResolveTerrainLayerTextureEntryIndex(const AppState& state,
                                                 const std::string& terrainPath,
                                                 unsigned char textureIndex) {
    const std::string parentPath = ParentArchivePath(terrainPath);
    const int textureNumber = static_cast<int>(textureIndex) + 1;
    std::vector<std::string> candidates;
    candidates.push_back("TEXTURE" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("Texture" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".tga");
    candidates.push_back("TEXTURE" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("Texture" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".mip");

    for (const std::string& candidateName : candidates) {
        const std::size_t entryIndex = FindArchiveEntryIndex(state, JoinArchivePathString(parentPath, candidateName));
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::vector<unsigned char> CollectTerrainTextureIndices(const LevelTerrainSection& section) {
    std::vector<unsigned char> indices;
    for (const LevelTriangle& triangle : section.triangles) {
        for (int layer = 0; layer < 4; ++layer) {
            const auto textureIndex = static_cast<unsigned char>((triangle.materialKey >> (layer * 8)) & 0xffU);
            if (textureIndex == 0xffU) {
                continue;
            }
            if (std::find(indices.begin(), indices.end(), textureIndex) == indices.end()) {
                indices.push_back(textureIndex);
            }
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void LoadTerrainPreviewTexture(AppState& state, LevelPreview& preview, const std::string& terrainPath) {
    const std::size_t textureEntryIndex = ResolveTerrainTextureEntryIndex(state, terrainPath);
    if (textureEntryIndex == static_cast<std::size_t>(-1)) {
        LogDebug("terrain baked texture not found for: " + terrainPath);
        return;
    }

    try {
        const gtc::FileEntry& textureEntry = state.archive.entries[textureEntryIndex];
        preview.terrainTexture = DecodeArchiveTextureEntryForWhirledRender(state, textureEntryIndex);
        DeleteGlTexture(preview.terrainTextureId);
        preview.terrainTextureId = CreateGlTextureRgba(
            preview.terrainTexture.width,
            preview.terrainTexture.height,
            preview.terrainTexture.rgba.data(),
            true,
            true,
            true);
        preview.terrainTexturePath = textureEntry.path;
        preview.status +=
            ", texture " +
            std::to_string(preview.terrainTexture.width) + "x" +
            std::to_string(preview.terrainTexture.height);
        LogDebug(
            "terrain texture loaded: " + textureEntry.path +
            " " + std::to_string(preview.terrainTexture.width) +
            "x" + std::to_string(preview.terrainTexture.height));
    } catch (const std::exception& error) {
        preview.status += ", texture failed";
        LogDebug(std::string("terrain texture load failed: ") + terrainPath + " - " + error.what());
    }
}

void LoadTerrainSectionTexture(AppState& state, LevelTerrainSection& section) {
    int whirledTextureCount = 0;
    for (const unsigned char textureIndex : CollectTerrainTextureIndices(section)) {
        const std::size_t layerEntryIndex = ResolveTerrainLayerTextureEntryIndex(state, section.path, textureIndex);
        if (layerEntryIndex == static_cast<std::size_t>(-1)) {
            LogDebug(
                "terrain layer texture not found: " + section.path +
                " texture" + std::to_string(static_cast<int>(textureIndex) + 1));
            continue;
        }

        try {
            const gtc::FileEntry& textureEntry = state.archive.entries[layerEntryIndex];
            DecodedTexture texture = DecodeArchiveTextureEntryForWhirledRender(state, layerEntryIndex);
            DeleteGlTexture(section.layerTextureIds[textureIndex]);
            section.layerTextureIds[textureIndex] = CreateGlTextureRgba(
                texture.width,
                texture.height,
                texture.rgba.data(),
                true,
                true,
                true);
            if (section.layerTextureIds[textureIndex] != 0) {
                section.layerTexturePaths[textureIndex] = textureEntry.path;
                ++whirledTextureCount;
            }
        } catch (const std::exception& error) {
            LogDebug(
                std::string("terrain layer texture load failed: ") +
                section.path +
                " texture" + std::to_string(static_cast<int>(textureIndex) + 1) +
                " - " + error.what());
        }
    }

    if (whirledTextureCount > 0) {
        LogDebug(
            "terrain Whirled layer textures loaded: " + section.path +
            " count=" + std::to_string(whirledTextureCount));
        return;
    }

    const std::size_t textureEntryIndex = ResolveTerrainTextureEntryIndex(state, section.path);
    if (textureEntryIndex == static_cast<std::size_t>(-1)) {
        LogDebug("terrain section texture not found for: " + section.path);
        return;
    }

    try {
        const gtc::FileEntry& textureEntry = state.archive.entries[textureEntryIndex];
        section.texture = DecodeArchiveTextureEntryForWhirledRender(state, textureEntryIndex);
        DeleteGlTexture(section.textureId);
        section.textureId = CreateGlTextureRgba(
            section.texture.width,
            section.texture.height,
            section.texture.rgba.data(),
            true,
            true,
            true);
        section.texturePath = textureEntry.path;
        LogDebug(
            "terrain section texture loaded: " + textureEntry.path +
            " " + std::to_string(section.texture.width) +
            "x" + std::to_string(section.texture.height));
    } catch (const std::exception& error) {
        LogDebug(std::string("terrain section texture load failed: ") + section.path + " - " + error.what());
    }
}

bool IsTextureNode(const ArchiveNode& node) {
    if (node.directory || node.entryIndex == static_cast<std::size_t>(-1)) {
        return false;
    }

    const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
    return extension == ".mip" || extension == ".tga" || extension == ".ifl";
}

bool IsModelNode(const ArchiveNode& node) {
    if (node.directory || node.entryIndex == static_cast<std::size_t>(-1)) {
        return false;
    }

    return ToLower(std::filesystem::path(node.name).extension().string()) == ".md2";
}

bool IsLevelNode(const ArchiveNode& node) {
    if (node.directory || node.entryIndex == static_cast<std::size_t>(-1)) {
        return false;
    }

    const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
    return extension == ".tdf" || extension == ".wrl";
}

bool IsAudioNode(const ArchiveNode& node) {
    if (node.directory) {
        return false;
    }

    const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
    return extension == ".aif" || (node.externalFile && IsMusicTrackExtension(extension));
}

bool IsTextNode(const ArchiveNode& node) {
    if (node.directory || node.entryIndex == static_cast<std::size_t>(-1)) {
        return false;
    }

    const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
    return extension == ".txt" || extension == ".inf";
}

bool IsFxPath(std::string path) {
    path = ToLower(std::move(path));
    return path.find("\\effects\\") != std::string::npos ||
           path.find("effect") != std::string::npos ||
           path.find("explosion") != std::string::npos ||
           path.find("shockwave") != std::string::npos ||
           path.find("smoke") != std::string::npos ||
           path.find("spark") != std::string::npos ||
           path.find("flare") != std::string::npos ||
           path.find("tornado") != std::string::npos;
}

bool IsFxNode(const ArchiveNode& node) {
    return IsFxPath(node.path);
}

bool HasPreviewSupport(const ArchiveNode& node) {
    return IsTextureNode(node) || IsModelNode(node) || IsLevelNode(node) || IsAudioNode(node) || IsTextNode(node);
}

std::string NodeExtensionLower(const ArchiveNode& node) {
    return ToLower(std::filesystem::path(node.name).extension().string());
}

const char* AssetFilterLabel(AssetFilter filter) {
    switch (filter) {
    case AssetFilter::Textures:
        return "Textures";
    case AssetFilter::Models:
        return "Models";
    case AssetFilter::Levels:
        return "Levels";
    case AssetFilter::Fx:
        return "FX";
    case AssetFilter::Audio:
        return "Audio";
    case AssetFilter::All:
    default:
        return "All";
    }
}

bool NodeMatchesAssetFilter(const ArchiveNode& node, AssetFilter filter) {
    if (filter == AssetFilter::All) {
        return true;
    }
    if (node.directory) {
        return filter == AssetFilter::Fx && IsFxNode(node);
    }

    const std::string extension = NodeExtensionLower(node);
    switch (filter) {
    case AssetFilter::Textures:
        return extension == ".mip" || extension == ".tga";
    case AssetFilter::Models:
        return extension == ".md2";
    case AssetFilter::Levels:
        return extension == ".wrl";
    case AssetFilter::Fx:
        return IsFxNode(node);
    case AssetFilter::Audio:
        return IsAudioNode(node);
    case AssetFilter::All:
    default:
        return true;
    }
}


void DeleteGlTexture(GLuint& textureId) {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
}

void DeleteGlDisplayList(GLuint& displayList) {
    if (displayList != 0) {
        glDeleteLists(displayList, 1);
        displayList = 0;
    }
}

RetiredGlTexture TakePreviewTexture(TexturePreview& preview) {
    RetiredGlTexture texture;
    texture.textureId = preview.textureId;
    texture.framesUntilDestroy = 3;
    preview.textureId = 0;
    return texture;
}

RetiredGlTexture TakeFxPreviewTexture(FxPreview& preview) {
    RetiredGlTexture texture;
    texture.textureId = preview.textureId;
    texture.framesUntilDestroy = 3;
    preview.textureId = 0;
    return texture;
}

bool HasRetiredTexture(const RetiredGlTexture& texture) {
    return texture.textureId != 0;
}

void DestroyRetiredTextureNow(RetiredGlTexture& texture) {
    DeleteGlTexture(texture.textureId);
}

void DestroyPreviewTextureImmediate(TexturePreview& preview) {
    RetiredGlTexture texture = TakePreviewTexture(preview);
    DestroyRetiredTextureNow(texture);
}

void DestroyFxPreviewTextureImmediate(FxPreview& preview) {
    RetiredGlTexture texture = TakeFxPreviewTexture(preview);
    DestroyRetiredTextureNow(texture);
}

void RetirePreviewTexture(RetiredGlTexture texture) {
    if (!HasRetiredTexture(texture)) {
        return;
    }
    gRetiredPreviewTextures.push_back(texture);
}

void DestroyPreviewTexture(TexturePreview& preview) {
    RetirePreviewTexture(TakePreviewTexture(preview));
}

void DestroyFxPreviewTexture(FxPreview& preview) {
    RetirePreviewTexture(TakeFxPreviewTexture(preview));
}

void DestroyModelTextures(ModelPreview& preview) {
    for (ModelMaterial& material : preview.materials) {
        DeleteGlTexture(material.textureId);
        material.loaded = false;
    }
}

void DestroyModelRenderTextureImmediate(ModelPreview& preview) {
    DestroyModelTextures(preview);
}

void DestroyModelRenderTexture(ModelPreview& preview) {
    DestroyModelRenderTextureImmediate(preview);
}

void ProcessRetiredPreviewTextures() {
    for (auto& texture : gRetiredPreviewTextures) {
        --texture.framesUntilDestroy;
    }

    auto writeIt = gRetiredPreviewTextures.begin();
    for (auto readIt = gRetiredPreviewTextures.begin(); readIt != gRetiredPreviewTextures.end(); ++readIt) {
        if (readIt->framesUntilDestroy <= 0) {
            DestroyRetiredTextureNow(*readIt);
        } else {
            *writeIt++ = *readIt;
        }
    }
    gRetiredPreviewTextures.erase(writeIt, gRetiredPreviewTextures.end());
}

void DestroyAllPreviewTextures(TexturePreview& preview) {
    DestroyPreviewTextureImmediate(preview);
    for (auto& texture : gRetiredPreviewTextures) {
        DestroyRetiredTextureNow(texture);
    }
    gRetiredPreviewTextures.clear();
}

void StopFxPreview(FxPreview& preview) {
    DestroyFxPreviewTexture(preview);
    preview.open = false;
    preview.playing = false;
    preview.loop = true;
    preview.frameIndex = 0;
    preview.lastFrameTime = 0.0;
    preview.fps = 12.5f;
    preview.name.clear();
    preview.path.clear();
    preview.status.clear();
    preview.decoded = {};
    preview.frames.clear();
    preview.frameNames.clear();
    preview.frameTicks.clear();
}

void StopLevelPreview(LevelPreview& preview) {
    DeleteGlTexture(preview.terrainTextureId);
    for (LevelTerrainSection& section : preview.terrainSections) {
        DeleteGlTexture(section.textureId);
        for (GLuint& textureId : section.layerTextureIds) {
            DeleteGlTexture(textureId);
        }
        DeleteGlDisplayList(section.displayListTextured);
        DeleteGlDisplayList(section.displayListColored);
    }
    for (LevelWaterSheet& waterSheet : preview.waterSheets) {
        DeleteGlTexture(waterSheet.textureId);
        DeleteGlDisplayList(waterSheet.displayList);
    }
    for (LevelModelAsset& asset : preview.modelAssets) {
        DeleteGlDisplayList(asset.displayList);
        DeleteGlDisplayList(asset.skyboxDisplayList);
        DestroyModelRenderTexture(asset.model);
    }
    preview = {};
}

std::vector<unsigned char> BuildNextRgbaMipLevel(const std::vector<unsigned char>& previous, int width, int height) {
    const int nextWidth = std::max(1, width / 2);
    const int nextHeight = std::max(1, height / 2);
    std::vector<unsigned char> next(static_cast<std::size_t>(nextWidth) * static_cast<std::size_t>(nextHeight) * 4);

    for (int y = 0; y < nextHeight; ++y) {
        for (int x = 0; x < nextWidth; ++x) {
            std::array<unsigned int, 4> sum = {};
            int samples = 0;
            for (int dy = 0; dy < 2; ++dy) {
                const int sourceY = std::min(height - 1, y * 2 + dy);
                for (int dx = 0; dx < 2; ++dx) {
                    const int sourceX = std::min(width - 1, x * 2 + dx);
                    const std::size_t source =
                        (static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(sourceX)) * 4;
                    for (int channel = 0; channel < 4; ++channel) {
                        sum[static_cast<std::size_t>(channel)] += previous[source + channel];
                    }
                    ++samples;
                }
            }

            const std::size_t destination =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(nextWidth) + static_cast<std::size_t>(x)) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                next[destination + channel] =
                    static_cast<unsigned char>((sum[static_cast<std::size_t>(channel)] + samples / 2) / samples);
            }
        }
    }

    return next;
}

bool HasOpenGlExtension(const char* name) {
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions == nullptr || name == nullptr || name[0] == '\0') {
        return false;
    }

    const std::string allExtensions(extensions);
    std::size_t offset = 0;
    while ((offset = allExtensions.find(name, offset)) != std::string::npos) {
        const bool startOk = offset == 0 || allExtensions[offset - 1] == ' ';
        const std::size_t end = offset + std::strlen(name);
        const bool endOk = end == allExtensions.size() || allExtensions[end] == ' ';
        if (startOk && endOk) {
            return true;
        }
        offset = end;
    }
    return false;
}

float MaxSupportedAnisotropy() {
    static bool checked = false;
    static float maxAnisotropy = 1.0f;
    if (!checked) {
        checked = true;
        if (HasOpenGlExtension("GL_EXT_texture_filter_anisotropic")) {
            glGetFloatv(kGlMaxTextureMaxAnisotropyExt, &maxAnisotropy);
            if (!std::isfinite(maxAnisotropy) || maxAnisotropy < 1.0f) {
                maxAnisotropy = 1.0f;
            }
        }
        LogDebug("OpenGL max anisotropy: " + std::to_string(maxAnisotropy));
    }
    return maxAnisotropy;
}

GLuint CreateGlTextureRgba(int width,
                           int height,
                           const unsigned char* pixels,
                           bool repeat,
                           bool linear,
                           bool mipmaps) {
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        return 0;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const bool buildMipmaps = mipmaps && repeat && linear && (width > 1 || height > 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, buildMipmaps ? GL_LINEAR_MIPMAP_LINEAR : (linear ? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    if (buildMipmaps) {
        std::vector<unsigned char> previous(
            pixels,
            pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        int mipWidth = width;
        int mipHeight = height;
        int level = 1;
        while (mipWidth > 1 || mipHeight > 1) {
            std::vector<unsigned char> next = BuildNextRgbaMipLevel(previous, mipWidth, mipHeight);
            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);
            glTexImage2D(
                GL_TEXTURE_2D,
                level,
                GL_RGBA,
                mipWidth,
                mipHeight,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                next.data());
            previous = std::move(next);
            ++level;
        }

        const float maxAnisotropy = MaxSupportedAnisotropy();
        if (maxAnisotropy > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, kGlTextureMaxAnisotropyExt, std::min(8.0f, maxAnisotropy));
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureId;
}

GLuint BlackTextureId() {
    if (gBlackTextureId != 0) {
        return gBlackTextureId;
    }

    const unsigned char blackPixel[4] = {0, 0, 0, 255};
    gBlackTextureId = CreateGlTextureRgba(1, 1, blackPixel, true, true, false);
    return gBlackTextureId;
}

bool HasTerrainShaderApi() {
    return gGlUseProgram != nullptr &&
           gGlCreateShader != nullptr &&
           gGlShaderSource != nullptr &&
           gGlCompileShader != nullptr &&
           gGlGetShaderiv != nullptr &&
           gGlGetShaderInfoLog != nullptr &&
           gGlDeleteShader != nullptr &&
           gGlCreateProgram != nullptr &&
           gGlAttachShader != nullptr &&
           gGlLinkProgram != nullptr &&
           gGlGetProgramiv != nullptr &&
           gGlGetProgramInfoLog != nullptr &&
           gGlDeleteProgram != nullptr &&
           gGlGetUniformLocation != nullptr &&
           gGlUniform1i != nullptr &&
           gGlUniform3f != nullptr &&
           gGlUniform1f != nullptr &&
           gGlActiveTexture != nullptr;
}

std::string GetShaderInfoLog(GLuint shader) {
    GLint length = 0;
    gGlGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    gGlGetShaderInfoLog(shader, length, &written, log.data());
    if (written > 0 && written <= length) {
        log.resize(static_cast<std::size_t>(written));
    }
    return log;
}

std::string GetProgramInfoLog(GLuint program) {
    GLint length = 0;
    gGlGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    gGlGetProgramInfoLog(program, length, &written, log.data());
    if (written > 0 && written <= length) {
        log.resize(static_cast<std::size_t>(written));
    }
    return log;
}

GLuint CompileTerrainShader(GLenum shaderType, const char* source, const char* label) {
    const GLuint shader = gGlCreateShader(shaderType);
    if (shader == 0) {
        return 0;
    }
    const char* sources[] = {source};
    gGlShaderSource(shader, 1, sources, nullptr);
    gGlCompileShader(shader);

    GLint compiled = 0;
    gGlGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        LogDebug(std::string("terrain shader compile failed (") + label + "): " + GetShaderInfoLog(shader));
        gGlDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint EnsureTerrainShaderProgram() {
    if (gTerrainShaderProgram != 0) {
        return gTerrainShaderProgram;
    }
    if (gTerrainShaderAttempted || !HasTerrainShaderApi()) {
        return 0;
    }
    gTerrainShaderAttempted = true;

    static constexpr const char* kTerrainVertexShader = R"GLSL(
#version 120
varying vec2 vUv;
varying vec4 vMix;
varying vec3 vNormal;

void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vUv = gl_MultiTexCoord0.xy;
    vMix = gl_Color;
    vNormal = normalize(gl_Normal);
}
)GLSL";

    static constexpr const char* kTerrainFragmentShader = R"GLSL(
#version 120
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform vec3 lightDir;
uniform float ambientLight;
varying vec2 vUv;
varying vec4 vMix;
varying vec3 vNormal;

void main() {
    vec4 weights = clamp(vMix, 0.0, 1.0);
    vec4 blended =
        texture2D(tex0, vUv) * weights.r +
        texture2D(tex1, vUv) * weights.g +
        texture2D(tex2, vUv) * weights.b +
        texture2D(tex3, vUv) * weights.a;
    float lambert = max(dot(normalize(vNormal), normalize(lightDir)), 0.0);
    float lighting = clamp(ambientLight + lambert * (1.0 - ambientLight), 0.0, 1.0);
    gl_FragColor = vec4(clamp(blended.rgb * lighting, 0.0, 1.0), 1.0);
}
)GLSL";

    const GLuint vertexShader = CompileTerrainShader(GL_VERTEX_SHADER, kTerrainVertexShader, "vertex");
    if (vertexShader == 0) {
        return 0;
    }
    const GLuint fragmentShader = CompileTerrainShader(GL_FRAGMENT_SHADER, kTerrainFragmentShader, "fragment");
    if (fragmentShader == 0) {
        gGlDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = gGlCreateProgram();
    gGlAttachShader(program, vertexShader);
    gGlAttachShader(program, fragmentShader);
    gGlLinkProgram(program);

    GLint linked = 0;
    gGlGetProgramiv(program, GL_LINK_STATUS, &linked);
    gGlDeleteShader(vertexShader);
    gGlDeleteShader(fragmentShader);
    if (linked == 0) {
        LogDebug("terrain shader link failed: " + GetProgramInfoLog(program));
        gGlDeleteProgram(program);
        return 0;
    }

    gTerrainShaderProgram = program;
    gTerrainTextureUniforms[0] = gGlGetUniformLocation(program, "tex0");
    gTerrainTextureUniforms[1] = gGlGetUniformLocation(program, "tex1");
    gTerrainTextureUniforms[2] = gGlGetUniformLocation(program, "tex2");
    gTerrainTextureUniforms[3] = gGlGetUniformLocation(program, "tex3");
    gTerrainLightDirUniform = gGlGetUniformLocation(program, "lightDir");
    gTerrainAmbientUniform = gGlGetUniformLocation(program, "ambientLight");
    LogDebug("terrain shader compiled");
    return gTerrainShaderProgram;
}

std::vector<unsigned char> BuildChannelMaskedPixels(const TexturePreview& preview) {
    std::vector<unsigned char> pixels = preview.decoded.rgba;
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
        pixels[index + 0] = preview.red ? pixels[index + 0] : 0;
        pixels[index + 1] = preview.green ? pixels[index + 1] : 0;
        pixels[index + 2] = preview.blue ? pixels[index + 2] : 0;
        pixels[index + 3] = preview.alpha ? pixels[index + 3] : 255;
    }
    return pixels;
}

void UploadPreviewTexture(TexturePreview& preview) {
    if (preview.decoded.width <= 0 || preview.decoded.height <= 0 || preview.decoded.rgba.empty()) {
        return;
    }

    LogDebug(
        "upload preview texture " + preview.name + " " +
        std::to_string(preview.decoded.width) + "x" + std::to_string(preview.decoded.height));
    DestroyPreviewTexture(preview);

    const std::vector<unsigned char> pixels = BuildChannelMaskedPixels(preview);
    preview.textureId = CreateGlTextureRgba(
        preview.decoded.width,
        preview.decoded.height,
        pixels.data(),
        false,
        false);
}

void UploadFxPreviewTexture(FxPreview& preview) {
    if (preview.decoded.width <= 0 || preview.decoded.height <= 0 || preview.decoded.rgba.empty()) {
        return;
    }

    DestroyFxPreviewTexture(preview);
    preview.textureId = CreateGlTextureRgba(
        preview.decoded.width,
        preview.decoded.height,
        preview.decoded.rgba.data(),
        false,
        false);
}

void AudioDeviceCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
    auto* preview = static_cast<AudioPreview*>(device->pUserData);
    auto* out = static_cast<float*>(output);
    std::fill(out, out + static_cast<std::size_t>(frameCount) * device->playback.channels, 0.0f);
    if (preview == nullptr || !preview->playing.load(std::memory_order_relaxed)) {
        return;
    }

    const DecodedAudio& audio = preview->decoded;
    if (audio.channels <= 0 || audio.samples.empty() || audio.frameCount == 0) {
        preview->playing.store(false, std::memory_order_relaxed);
        return;
    }

    std::uint64_t cursor = preview->cursorFrame.load(std::memory_order_relaxed);
    const float volume = std::clamp(preview->volume.load(std::memory_order_relaxed), 0.0f, 1.0f);
    const bool loop = preview->loop.load(std::memory_order_relaxed);
    const ma_uint32 outputChannels = device->playback.channels;

    for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
        if (cursor >= audio.frameCount) {
            if (loop) {
                cursor = 0;
            } else {
                preview->playing.store(false, std::memory_order_relaxed);
                break;
            }
        }

        for (ma_uint32 channel = 0; channel < outputChannels; ++channel) {
            const int sourceChannel = static_cast<int>(channel % static_cast<ma_uint32>(audio.channels));
            const std::size_t sampleIndex =
                static_cast<std::size_t>(cursor) * audio.channels + static_cast<std::size_t>(sourceChannel);
            out[static_cast<std::size_t>(frame) * outputChannels + channel] = audio.samples[sampleIndex] * volume;
        }
        ++cursor;
    }

    preview->cursorFrame.store(cursor, std::memory_order_relaxed);
}

void StopAudioPlayback(AudioPreview& preview) {
    preview.playing.store(false, std::memory_order_relaxed);
    if (preview.deviceInitialized) {
        ma_device_stop(&preview.device);
        ma_device_uninit(&preview.device);
        preview.device = {};
        preview.deviceInitialized = false;
    }
}

void StopAudioPreview(AudioPreview& preview) {
    StopAudioPlayback(preview);
    preview.open = false;
    preview.name.clear();
    preview.path.clear();
    preview.status.clear();
    preview.decoded = {};
    preview.cursorFrame.store(0, std::memory_order_relaxed);
    preview.loop.store(false, std::memory_order_relaxed);
    preview.volume.store(preview.volumeUi, std::memory_order_relaxed);
}

void StartAudioPlayback(AudioPreview& preview) {
    StopAudioPlayback(preview);
    if (preview.decoded.samples.empty() ||
        preview.decoded.channels <= 0 ||
        preview.decoded.sampleRate <= 0 ||
        preview.decoded.frameCount == 0) {
        throw std::runtime_error("Audio preview has no decoded samples.");
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = static_cast<ma_uint32>(preview.decoded.channels);
    config.sampleRate = static_cast<ma_uint32>(preview.decoded.sampleRate);
    config.dataCallback = AudioDeviceCallback;
    config.pUserData = &preview;

    const ma_result initResult = ma_device_init(nullptr, &config, &preview.device);
    if (initResult != MA_SUCCESS) {
        preview.device = {};
        throw std::runtime_error("Could not initialize the audio device.");
    }

    preview.deviceInitialized = true;
    const ma_result startResult = ma_device_start(&preview.device);
    if (startResult != MA_SUCCESS) {
        StopAudioPlayback(preview);
        throw std::runtime_error("Could not start the audio device.");
    }

    preview.playing.store(true, std::memory_order_relaxed);
}

bool IsFxImageFrameNode(const ArchiveNode& node) {
    if (node.directory || node.entryIndex == static_cast<std::size_t>(-1)) {
        return false;
    }

    const std::string extension = NodeExtensionLower(node);
    return extension == ".mip" || extension == ".tga";
}

void SetFxPreviewFrame(FxPreview& preview, int frameIndex) {
    if (preview.frames.empty()) {
        return;
    }

    preview.frameIndex = std::clamp(frameIndex, 0, static_cast<int>(preview.frames.size()) - 1);
    preview.decoded = preview.frames[preview.frameIndex];
    UploadFxPreviewTexture(preview);
}

void ClosePreviewsForFx(AppState& state) {
    StopAudioPreview(state.audioPreview);
    StopLevelPreview(state.levelPreview);
    state.textPreview = {};
    DestroyPreviewTexture(state.texturePreview);
    state.texturePreview.open = false;
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    StopFxPreview(state.fxPreview);
}

bool OpenFxPreviewForIfl(AppState& state, const ArchiveNode& node) {
    if (!IsTextureNode(node) || NodeExtensionLower(node) != ".ifl") {
        return false;
    }

    LogDebug("open FX IFL preview: " + node.path);
    ClosePreviewsForFx(state);

    FxPreview preview;
    preview.open = true;
    preview.playing = true;
    preview.loop = true;
    preview.name = node.name;
    preview.path = node.path;

    try {
        const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
        LoadedIflFrames loadedFrames = LoadIflFrames(state, node, bytes);
        preview.frames = std::move(loadedFrames.frames);
        preview.frameNames = std::move(loadedFrames.frameNames);
        preview.frameTicks = std::move(loadedFrames.frameTicks);
        preview.lastFrameTime = ImGui::GetTime();
        preview.status =
            std::to_string(preview.frames.size()) + " IFL frames";
        if (loadedFrames.missingFrames > 0) {
            preview.status += "  missing " + std::to_string(loadedFrames.missingFrames);
        }

        state.fxPreview = std::move(preview);
        SetFxPreviewFrame(state.fxPreview, 0);
        state.status = "Previewing FX " + node.path;
        LogDebug("open FX IFL preview complete: " + node.path);
        return true;
    } catch (const std::exception& error) {
        preview.status = std::string("FX preview failed: ") + error.what();
        state.fxPreview = std::move(preview);
        state.status = state.fxPreview.status;
        LogDebug(state.fxPreview.status);
        return true;
    }
}

bool OpenFxPreviewForFolder(AppState& state, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(state.browser.nodes.size())) {
        return false;
    }

    const ArchiveNode& folder = state.browser.nodes[nodeIndex];
    if (!folder.directory || !IsFxNode(folder)) {
        return false;
    }

    int iflNodeIndex = -1;
    std::vector<int> frameNodeIndices;
    for (const int fileNodeIndex : folder.files) {
        const ArchiveNode& fileNode = state.browser.nodes[fileNodeIndex];
        const std::string extension = NodeExtensionLower(fileNode);
        if (extension == ".ifl" && iflNodeIndex < 0) {
            iflNodeIndex = fileNodeIndex;
        } else if (IsFxImageFrameNode(fileNode)) {
            frameNodeIndices.push_back(fileNodeIndex);
        }
    }

    if (iflNodeIndex >= 0) {
        return OpenFxPreviewForIfl(state, state.browser.nodes[iflNodeIndex]);
    }

    if (frameNodeIndices.empty()) {
        return false;
    }

    LogDebug("open FX folder preview: " + folder.path);
    ClosePreviewsForFx(state);

    FxPreview preview;
    preview.open = true;
    preview.playing = frameNodeIndices.size() > 1;
    preview.loop = true;
    preview.name = folder.name;
    preview.path = folder.path;

    int failedFrames = 0;
    for (const int frameNodeIndex : frameNodeIndices) {
        const ArchiveNode& frameNode = state.browser.nodes[frameNodeIndex];
        try {
            const std::vector<char> frameBytes = ReadEntryBytesForPreview(state, frameNode.entryIndex);
            preview.frames.push_back(DecodeTextureBytesByName(frameBytes, frameNode.path));
            preview.frameNames.push_back(frameNode.path);
            preview.frameTicks.push_back(1);
        } catch (const std::exception& error) {
            LogDebug(std::string("FX frame load failed: ") + frameNode.path + " - " + error.what());
            ++failedFrames;
        }
    }

    if (preview.frames.empty()) {
        preview.status = "FX preview failed: no supported frames could be decoded.";
        state.fxPreview = std::move(preview);
        state.status = state.fxPreview.status;
        return true;
    }

    preview.lastFrameTime = ImGui::GetTime();
    preview.status = std::to_string(preview.frames.size()) + " frames";
    if (failedFrames > 0) {
        preview.status += "  failed " + std::to_string(failedFrames);
    }

    state.fxPreview = std::move(preview);
    SetFxPreviewFrame(state.fxPreview, 0);
    state.status = "Previewing FX " + folder.path;
    LogDebug(
        "open FX folder preview complete: " + folder.path +
        " frames=" + std::to_string(state.fxPreview.frames.size()));
    return true;
}

void ShowUnsupportedPreview(AppState& state, const ArchiveNode& node) {
    TexturePreview& preview = state.texturePreview;
    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    state.textPreview = {};
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    DestroyPreviewTexture(preview);
    preview.open = true;
    preview.animated = false;
    preview.frameIndex = 0;
    preview.name = node.name;
    preview.path = node.path;
    preview.decoded = {};
    preview.frames.clear();
    preview.frameTicks.clear();
    preview.status = "Preview is not supported for this file type yet.";
    state.status = "Preview is not supported for " + node.name;
}

std::string DecodeTextPreviewBytes(const std::vector<char>& bytes) {
    std::string text(bytes.begin(), bytes.end());
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    for (char& character : text) {
        if (character == '\0') {
            character = ' ';
        }
    }
    return text;
}

void OpenTextPreview(AppState& state, const ArchiveNode& node) {
    if (!IsTextNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

    LogDebug("open text preview: " + node.path);
    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    DestroyPreviewTexture(state.texturePreview);
    state.texturePreview.open = false;
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;

    TextPreview preview;
    preview.open = true;
    preview.name = node.name;
    preview.path = node.path;
    try {
        const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
        preview.content = DecodeTextPreviewBytes(bytes);
        preview.status =
            gtc::FormatByteSize(bytes.size()) + "  " +
            std::to_string(std::count(preview.content.begin(), preview.content.end(), '\n') + 1) + " lines";
        state.textPreview = std::move(preview);
        state.status = "Previewing " + node.path;
        LogDebug("open text preview complete: " + node.path + " bytes=" + std::to_string(bytes.size()));
    } catch (const std::exception& error) {
        preview.status = std::string("Text preview failed: ") + error.what();
        state.textPreview = std::move(preview);
        state.status = state.textPreview.status;
        LogDebug(state.textPreview.status);
    }
}

void SelectFolder(AppState& state, int nodeIndex);

void OpenTexturePreview(AppState& state, const ArchiveNode& node) {
    if (!IsTextureNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

    LogDebug("open texture preview: " + node.path);
    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    state.textPreview = {};
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    TexturePreview& preview = state.texturePreview;
    preview.status.clear();
    preview.name = node.name;
    preview.path = node.path;
    preview.red = true;
    preview.green = true;
    preview.blue = true;
    preview.alpha = true;
    preview.animated = false;
    preview.frameIndex = 0;
    preview.lastFrameTime = 0.0;
    preview.frames.clear();
    preview.frameTicks.clear();

    try {
        const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
        const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
        if (extension == ".ifl") {
            LoadIflPreview(state, preview, node, bytes);
        } else {
            preview.decoded = extension == ".tga" ? DecodeTgaTexture(bytes) : DecodeMipTexture(bytes);
        }
        preview.open = true;
        UploadPreviewTexture(preview);
        state.status = "Previewing " + node.path;
        LogDebug("open texture preview complete: " + node.path);
    } catch (const std::exception& error) {
        LogDebug(std::string("open texture preview failed: ") + error.what());
        preview.open = true;
        preview.animated = false;
        preview.frameIndex = 0;
        preview.decoded = {};
        preview.frames.clear();
        preview.frameTicks.clear();
        DestroyPreviewTexture(preview);
        preview.status = std::string("Preview failed: ") + error.what();
        state.status = preview.status;
    }
}

void OpenModelPreview(AppState& state, const ArchiveNode& node) {
    if (!IsModelNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

    LogDebug("open model preview: " + node.path);
    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    state.textPreview = {};
    DestroyPreviewTexture(state.texturePreview);
    state.texturePreview.open = false;

    ModelPreview preview;
    preview.open = true;
    preview.name = node.name;
    preview.path = node.path;

    try {
        const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
        ModelPreview decoded = DecodeMd2Model(bytes);
        decoded.open = true;
        decoded.name = node.name;
        decoded.path = node.path;
        decoded.status =
            std::to_string(decoded.vertices.size()) + " vertices, " +
            std::to_string(decoded.triangles.size()) + " triangles, " +
            std::to_string(decoded.sections.size()) + " sections";
        if (!decoded.skinRecords.empty()) {
            decoded.status += "  SKN0 " + std::to_string(decoded.skinRecords.size()) + " blend records";
        }
        LoadModelPreviewSkeleton(state, decoded, node);
        LoadModelPreviewTextures(state, decoded);
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = std::move(decoded);
        state.status = "Previewing " + node.path;
        LogDebug(
            "open model preview complete: " + node.path + " vertices=" +
            std::to_string(state.modelPreview.vertices.size()) + " triangles=" +
            std::to_string(state.modelPreview.triangles.size()) + " sections=" +
            std::to_string(state.modelPreview.sections.size()));
    } catch (const std::exception& error) {
        LogDebug(std::string("open model preview failed: ") + error.what());
        preview.status = std::string("Model preview failed: ") + error.what();
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = std::move(preview);
        state.status = state.modelPreview.status;
    }
}

void OpenAudioPreview(AppState& state, const ArchiveNode& node) {
    if (!IsAudioNode(node) ||
        (!node.externalFile && node.entryIndex >= state.archive.entries.size())) {
        ShowUnsupportedPreview(state, node);
        return;
    }

    LogDebug("open audio preview: " + node.path);
    DestroyPreviewTexture(state.texturePreview);
    state.texturePreview.open = false;
    state.textPreview = {};
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    StopAudioPreview(state.audioPreview);

    AudioPreview& preview = state.audioPreview;
    preview.open = true;
    preview.name = node.name;
    preview.path = node.path;
    preview.status.clear();
    preview.cursorFrame.store(0, std::memory_order_relaxed);
    preview.playing.store(false, std::memory_order_relaxed);
    preview.volume.store(preview.volumeUi, std::memory_order_relaxed);

    try {
        const std::vector<char> bytes =
            node.externalFile ? ReadBinaryPreviewFile(node.externalPath) : ReadEntryBytesForPreview(state, node.entryIndex);
        preview.decoded = DecodeAudioBytes(bytes);
        preview.status =
            std::to_string(preview.decoded.channels) + " ch  " +
            std::to_string(preview.decoded.sampleRate) + " Hz  " +
            preview.decoded.format;
        StartAudioPlayback(preview);
        state.status = "Playing " + node.path;
        LogDebug(
            "open audio preview complete: " + node.path +
            (node.externalFile ? " external=" + node.externalPath.string() : "") +
            " channels=" + std::to_string(preview.decoded.channels) +
            " sample_rate=" + std::to_string(preview.decoded.sampleRate) +
            " bits=" + std::to_string(preview.decoded.bitsPerSample) +
            " frames=" + std::to_string(preview.decoded.frameCount));
    } catch (const std::exception& error) {
        StopAudioPlayback(preview);
        preview.status = std::string("Audio preview failed: ") + error.what();
        state.status = preview.status;
        LogDebug(preview.status);
    }
}

std::string TerrainDataCandidateFromWorldPath(std::string terrainPath) {
    std::replace(terrainPath.begin(), terrainPath.end(), '/', '\\');
    terrainPath = Trim(std::move(terrainPath));
    while (!terrainPath.empty() && (terrainPath.back() == '\\' || terrainPath.back() == '/')) {
        terrainPath.pop_back();
    }

    if (ToLower(std::filesystem::path(terrainPath).extension().string()) == ".tdf") {
        return terrainPath;
    }
    return JoinArchivePathString(terrainPath, "TERRDATA.TDF");
}

void TryLoadWorldTerrain(AppState& state, LevelPreview& preview) {
    std::vector<WrlTerrainLink> terrainLinks = preview.terrainLinks;
    if (terrainLinks.empty() && !preview.terrainPath.empty()) {
        WrlTerrainLink fallback;
        fallback.path = preview.terrainPath;
        AddUniqueTerrainLink(terrainLinks, std::move(fallback));
    }

    if (terrainLinks.empty()) {
        preview.status += ", no terrain link";
        return;
    }

    int loadedSections = 0;
    int missingSections = 0;
    for (const WrlTerrainLink& link : terrainLinks) {
        const std::string terrainCandidate = TerrainDataCandidateFromWorldPath(link.path);
        LogDebug(
            "WRL terrain candidate: " + terrainCandidate +
            " layer=" + (link.hasLayer ? FormatHex32(link.layer) : std::string("none")) +
            " pos=" + std::to_string(link.position.x) + "," + std::to_string(link.position.y) + "," +
            std::to_string(link.position.z) +
            " scale=" + std::to_string(link.scale.x) + "," + std::to_string(link.scale.y) + "," +
            std::to_string(link.scale.z) +
            " tex_scale=" + std::to_string(link.textureScaleX) + "," + std::to_string(link.textureScaleY));
        const std::size_t terrainEntryIndex = FindArchiveEntryIndex(state, terrainCandidate);
        if (terrainEntryIndex == static_cast<std::size_t>(-1)) {
            ++missingSections;
            LogDebug("WRL terrain not found: " + terrainCandidate);
            continue;
        }

        try {
            const gtc::FileEntry& terrainEntry = state.archive.entries[terrainEntryIndex];
            const std::vector<char> terrainBytes = ReadEntryBytesForPreview(state, terrainEntryIndex);
            LevelPreview terrain = DecodeTdfTerrain(
                terrainBytes,
                std::filesystem::path(terrainEntry.path).filename().string(),
                terrainEntry.path);
            AddTerrainSectionToWorld(preview, std::move(terrain), link);
            LoadTerrainSectionTexture(state, preview.terrainSections.back());
            ++loadedSections;
            LogDebug("WRL terrain loaded: " + terrainEntry.path);
        } catch (const std::exception& error) {
            ++missingSections;
            LogDebug(std::string("WRL terrain load failed: ") + terrainCandidate + " - " + error.what());
        }
    }

    if (loadedSections > 0) {
        preview.status +=
            ", " + std::to_string(loadedSections) + " terrain section";
        if (loadedSections != 1) {
            preview.status += "s";
        }
        preview.status += ", " + std::to_string(preview.triangles.size()) + " terrain triangles";
    }
    if (missingSections > 0) {
        preview.status += ", " + std::to_string(missingSections) + " terrain missing";
    }
}

void TryLoadWorldModels(AppState& state, LevelPreview& preview) {
    std::unordered_map<std::string, int> assetLookup;
    int missingModels = 0;
    int failedModels = 0;

    auto loadModelAsset = [&](const std::string& modelPath) -> int {
        const std::string lookupKey = NormalizeArchivePath(modelPath);
        const auto found = assetLookup.find(lookupKey);
        if (found != assetLookup.end()) {
            return found->second;
        }

        const std::size_t entryIndex = FindArchiveEntryIndex(state, modelPath);
        if (entryIndex == static_cast<std::size_t>(-1)) {
            ++missingModels;
            assetLookup.emplace(lookupKey, -1);
            LogDebug("WRL model not found: " + modelPath);
            return -1;
        }

        LevelModelAsset asset;
        asset.path = state.archive.entries[entryIndex].path;
        try {
            const std::vector<char> modelBytes = ReadEntryBytesForPreview(state, entryIndex);
            asset.model = DecodeMd2Model(modelBytes);
            asset.model.open = false;
            asset.model.name = std::filesystem::path(asset.path).filename().string();
            asset.model.path = asset.path;
            LoadModelPreviewTextures(state, asset.model);
            asset.loaded = true;
            asset.status =
                std::to_string(asset.model.vertices.size()) + " verts, " +
                std::to_string(asset.model.triangles.size()) + " tris, " +
                std::to_string(asset.model.sections.size()) + " sections";
            const int assetIndex = static_cast<int>(preview.modelAssets.size());
            preview.modelAssets.push_back(std::move(asset));
            assetLookup.emplace(lookupKey, assetIndex);
            return assetIndex;
        } catch (const std::exception& error) {
            ++failedModels;
            assetLookup.emplace(lookupKey, -1);
            LogDebug(std::string("WRL model load failed: ") + modelPath + " - " + error.what());
            return -1;
        }
    };

    for (std::size_t objectIndex = 0; objectIndex < preview.objects.size(); ++objectIndex) {
        WorldObject& object = preview.objects[objectIndex];
        if (object.modelPath.empty()) {
            continue;
        }

        const int assetIndex = loadModelAsset(object.modelPath);
        if (assetIndex < 0) {
            continue;
        }

        if (ToLower(object.className) == "cskybox") {
            preview.skyboxAssetIndices.push_back(assetIndex);
            preview.skyboxPaths.push_back(object.modelPath);
            continue;
        }
        if (!object.hasPosition) {
            continue;
        }

        LevelModelInstance instance;
        instance.objectIndex = static_cast<int>(objectIndex);
        instance.assetIndex = assetIndex;
        instance.position = object.position;
        instance.rotation = object.hasRotation ? object.rotation : Quat{};
        instance.scale = object.scale;
        preview.modelInstances.push_back(instance);
    }

    if (!preview.modelInstances.empty()) {
        preview.status +=
            ", " + std::to_string(preview.modelInstances.size()) + " model instances, " +
            std::to_string(preview.modelAssets.size()) + " unique models";
    }
    if (!preview.skyboxAssetIndices.empty()) {
        preview.status += ", " + std::to_string(preview.skyboxAssetIndices.size()) + " skybox";
        if (preview.skyboxAssetIndices.size() != 1) {
            preview.status += "es";
        }
    }
    if (missingModels > 0) {
        preview.status += ", " + std::to_string(missingModels) + " models missing";
    }
    if (failedModels > 0) {
        preview.status += ", " + std::to_string(failedModels) + " models failed";
    }
    LogDebug(
        "WRL models loaded: " + preview.path +
        " instances=" + std::to_string(preview.modelInstances.size()) +
        " skyboxes=" + std::to_string(preview.skyboxAssetIndices.size()) +
        " unique=" + std::to_string(preview.modelAssets.size()) +
        " missing=" + std::to_string(missingModels) +
        " failed=" + std::to_string(failedModels));
}

void TryLoadWorldWater(AppState& state, LevelPreview& preview) {
    int loadedWater = 0;
    int missingWater = 0;
    int failedWater = 0;

    for (LevelWaterSheet& waterSheet : preview.waterSheets) {
        if (waterSheet.texturePath.empty()) {
            ++missingWater;
            continue;
        }

        const std::size_t textureEntryIndex = ResolveModelTextureEntryIndex(state, waterSheet.texturePath);
        if (textureEntryIndex == static_cast<std::size_t>(-1)) {
            ++missingWater;
            LogDebug("WRL water texture not found: " + waterSheet.texturePath);
            continue;
        }

        try {
            DecodedTexture texture = DecodeArchiveTextureEntryForWhirledRender(state, textureEntryIndex);
            DeleteGlTexture(waterSheet.textureId);
            waterSheet.textureId = CreateGlTextureRgba(
                texture.width,
                texture.height,
                texture.rgba.data(),
                true,
                true);
            if (waterSheet.textureId == 0) {
                throw std::runtime_error("OpenGL texture creation failed.");
            }
            ++loadedWater;
        } catch (const std::exception& error) {
            ++failedWater;
            LogDebug(std::string("WRL water texture load failed: ") + waterSheet.texturePath + " - " + error.what());
        }
    }

    if (loadedWater > 0) {
        preview.status += ", " + std::to_string(loadedWater) + " water loaded";
    }
    if (missingWater > 0) {
        preview.status += ", " + std::to_string(missingWater) + " water missing";
    }
    if (failedWater > 0) {
        preview.status += ", " + std::to_string(failedWater) + " water failed";
    }
    LogDebug(
        "WRL water loaded: " + preview.path +
        " sheets=" + std::to_string(preview.waterSheets.size()) +
        " loaded=" + std::to_string(loadedWater) +
        " missing=" + std::to_string(missingWater) +
        " failed=" + std::to_string(failedWater));
}

void OpenLevelPreview(AppState& state, const ArchiveNode& node) {
    if (!IsLevelNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

    LogDebug("open level preview: " + node.path);
    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    DestroyPreviewTexture(state.texturePreview);
    state.texturePreview.open = false;
    state.textPreview = {};
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    StopLevelPreview(state.levelPreview);

    LevelPreview preview;
    preview.open = true;
    preview.name = node.name;
    preview.path = node.path;

    try {
        const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
        LogDebug("level preview bytes read: " + node.path + " bytes=" + std::to_string(bytes.size()));
        const std::string extension = NodeExtensionLower(node);
        if (extension == ".tdf") {
            preview = DecodeTdfTerrain(bytes, node.name, node.path);
            LoadTerrainPreviewTexture(state, preview, node.path);
        } else if (extension == ".wrl") {
            preview = DecodeWrlWorld(bytes, node.name, node.path);
            TryLoadWorldTerrain(state, preview);
            TryLoadWorldModels(state, preview);
            TryLoadWorldWater(state, preview);
        } else {
            throw std::runtime_error("Unsupported level file extension.");
        }

        state.levelPreview = std::move(preview);
        state.status = "Previewing " + node.path;
        LogDebug(
            "open level preview complete: " + node.path +
            " vertices=" + std::to_string(state.levelPreview.vertices.size()) +
            " triangles=" + std::to_string(state.levelPreview.triangles.size()) +
            " objects=" + std::to_string(state.levelPreview.objects.size()) +
            " water_sheets=" + std::to_string(state.levelPreview.waterSheets.size()) +
            " skyboxes=" + std::to_string(state.levelPreview.skyboxAssetIndices.size()));
    } catch (const std::exception& error) {
        LogDebug(std::string("open level preview failed: ") + error.what());
        preview.status = std::string("Level preview failed: ") + error.what();
        state.levelPreview = std::move(preview);
        state.status = state.levelPreview.status;
    } catch (...) {
        LogDebug("open level preview failed: unknown exception");
        preview.status = "Level preview failed: unknown exception.";
        state.levelPreview = std::move(preview);
        state.status = state.levelPreview.status;
    }
}

void HandleNodeDoubleClick(AppState& state, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(state.browser.nodes.size())) {
        LogDebug("double click ignored: invalid node index");
        return;
    }

    const ArchiveNode& node = state.browser.nodes[nodeIndex];
    LogDebug("double click: " + node.path);
    if (node.directory) {
        SelectFolder(state, nodeIndex);
        OpenFxPreviewForFolder(state, nodeIndex);
        return;
    }

    if (IsFxNode(node) && NodeExtensionLower(node) == ".ifl" && OpenFxPreviewForIfl(state, node)) {
        return;
    }

    if (IsTextureNode(node)) {
        OpenTexturePreview(state, node);
        return;
    }
    if (IsModelNode(node)) {
        OpenModelPreview(state, node);
        return;
    }
    if (IsLevelNode(node)) {
        OpenLevelPreview(state, node);
        return;
    }
    if (IsAudioNode(node)) {
        OpenAudioPreview(state, node);
        return;
    }
    if (IsTextNode(node)) {
        OpenTextPreview(state, node);
        return;
    }

    ShowUnsupportedPreview(state, node);
}

std::string FileTypeForNode(const AppState& state, const ArchiveNode& node) {
    if (node.directory) {
        return IsFxNode(node) ? "FX Folder" : "File folder";
    }
    if (IsAudioNode(node)) {
        return node.externalFile ? "Music Track" : "AIFF Audio";
    }
    if (IsTextNode(node)) {
        return "Text File";
    }
    if (IsLevelNode(node)) {
        const std::string extension = NodeExtensionLower(node);
        return extension == ".wrl" ? "Saved World" : "Terrain";
    }

    const std::filesystem::path fileName(node.name);
    std::string extension = fileName.extension().string();
    if (extension.empty()) {
        return "File";
    }
    if (extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return extension + " File";
}

std::string FileSizeForNode(const AppState& state, const ArchiveNode& node) {
    if (node.directory) {
        return "";
    }
    if (node.externalFile) {
        try {
            return gtc::FormatByteSize(std::filesystem::file_size(node.externalPath));
        } catch (...) {
            return "";
        }
    }
    if (node.entryIndex == static_cast<std::size_t>(-1)) {
        return "";
    }
    return gtc::FormatByteSize(state.archive.entries[node.entryIndex].size);
}

int ChildFolderCount(const ArchiveBrowser& browser, int nodeIndex) {
    return static_cast<int>(browser.nodes[nodeIndex].folders.size());
}

int ChildFileCount(const ArchiveBrowser& browser, int nodeIndex) {
    return static_cast<int>(browser.nodes[nodeIndex].files.size());
}

bool IsAncestorOrSelf(const ArchiveBrowser& browser, int maybeAncestor, int nodeIndex) {
    int current = nodeIndex;
    while (current >= 0 && current < static_cast<int>(browser.nodes.size())) {
        if (current == maybeAncestor) {
            return true;
        }
        current = browser.nodes[current].parent;
    }
    return false;
}

void SelectFolder(AppState& state, int nodeIndex) {
    state.browser.selectedFolder = nodeIndex;
    state.browser.selectedItem = nodeIndex;
    state.assetFilter = AssetFilter::All;
}

void DrawFolderTreeNode(AppState& state, int nodeIndex) {
    ArchiveBrowser& browser = state.browser;
    ArchiveNode& node = browser.nodes[nodeIndex];

    if (IsAncestorOrSelf(browser, nodeIndex, browser.selectedFolder)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.folders.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (browser.selectedFolder == nodeIndex) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const char* icon = browser.selectedFolder == nodeIndex ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
    if (nodeIndex == 0) {
        icon = ICON_FA_BOX_ARCHIVE;
    }

    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<intptr_t>(nodeIndex)),
        flags,
        "%s  %s",
        icon,
        node.name.c_str());

    if (ImGui::IsItemClicked()) {
        SelectFolder(state, nodeIndex);
    }

    if (open && !node.folders.empty()) {
        for (const int folderIndex : node.folders) {
            DrawFolderTreeNode(state, folderIndex);
        }
        ImGui::TreePop();
    }
}

void DrawBreadcrumb(AppState& state) {
    std::vector<int> chain;
    int current = state.browser.selectedFolder;
    while (current >= 0) {
        chain.push_back(current);
        current = state.browser.nodes[current].parent;
    }
    std::reverse(chain.begin(), chain.end());

    bool drewCrumb = false;
    for (std::size_t index = 0; index < chain.size(); ++index) {
        const int nodeIndex = chain[index];
        if (nodeIndex == 0) {
            continue;
        }

        const ArchiveNode& node = state.browser.nodes[nodeIndex];

        if (drewCrumb) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ICON_FA_CHEVRON_RIGHT);
            ImGui::SameLine();
        }

        const std::string label = (nodeIndex == 0 ? std::string(ICON_FA_BOX_ARCHIVE) : std::string(ICON_FA_FOLDER)) +
                                  "  " + node.name + "##crumb" + std::to_string(nodeIndex);
        if (ImGui::SmallButton(label.c_str())) {
            SelectFolder(state, nodeIndex);
        }
        drewCrumb = true;
    }
}

void DrawRightPane(AppState& state) {
    if (!state.archiveLoaded || state.browser.nodes.empty()) {
        ImGui::TextDisabled("No archive loaded.");
        return;
    }

    const float searchWidth = std::min(300.0f, std::max(180.0f, ImGui::GetContentRegionAvail().x * 0.32f));
    ImGui::SetNextItemWidth(searchWidth);
    const std::string searchHint = std::string(ICON_FA_MAGNIFYING_GLASS) + "  Search...";
    ImGui::InputTextWithHint("##ContentSearch", searchHint.c_str(), state.searchText.data(), state.searchText.size());
    ImGui::SameLine();
    if (state.assetFilter == AssetFilter::All) {
        DrawBreadcrumb(state);
    } else {
        ImGui::TextDisabled("%s", AssetFilterLabel(state.assetFilter));
    }
    ImGui::Separator();

    const ArchiveNode& currentFolder = state.browser.nodes[state.browser.selectedFolder];
    const std::string filter = ToLower(state.searchText.data());
    const ImVec2 tileSize(110.0f, 106.0f);
    const float tileStride = tileSize.x + ImGui::GetStyle().ItemSpacing.x;

    auto nodeMatchesFilter = [&](const ArchiveNode& node) {
        if (!NodeMatchesAssetFilter(node, state.assetFilter)) {
            return false;
        }

        if (filter.empty()) {
            return true;
        }

        const std::string haystack =
            state.assetFilter == AssetFilter::All ? ToLower(node.name) : ToLower(node.path);
        return haystack.find(filter) != std::string::npos;
    };

    auto iconForNode = [&](const ArchiveNode& node) -> const char* {
        if (node.directory) {
            return ICON_FA_FOLDER;
        }

        if (IsAudioNode(node)) {
            return ICON_FA_MUSIC;
        }
        if (IsFxNode(node)) {
            return ICON_FA_BOLT;
        }
        const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
        if (extension == ".tdf" || extension == ".wrl") {
            return ICON_FA_FILE_LINES;
        }
        if (extension == ".txt" || extension == ".inf") {
            return ICON_FA_FILE_LINES;
        }
        return ICON_FA_FILE;
    };

    auto drawTile = [&](int nodeIndex) {
        const ArchiveNode& node = state.browser.nodes[nodeIndex];
        const bool selected = state.browser.selectedItem == nodeIndex;

        ImGui::PushID(nodeIndex);
        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const ImVec2 tileMax(tileMin.x + tileSize.x, tileMin.y + tileSize.y);
        ImGui::InvisibleButton("tile", tileSize);
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            state.browser.selectedItem = nodeIndex;
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            HandleNodeDoubleClick(state, nodeIndex);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 background = ImGui::GetColorU32(
            selected ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_ChildBg);
        drawList->AddRectFilled(tileMin, tileMax, background, 4.0f);
        if (hovered || selected) {
            drawList->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
        }

        ImFont* font = ImGui::GetFont();
        const char* icon = iconForNode(node);
        const float iconSize = 44.0f;
        const ImVec2 iconTextSize = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
        const ImVec2 iconPos(
            tileMin.x + (tileSize.x - iconTextSize.x) * 0.5f,
            tileMin.y + 13.0f);
        const ImU32 iconColor = node.directory
                                    ? IM_COL32(255, 204, 74, 255)
                                    : IM_COL32(185, 196, 204, 255);
        drawList->AddText(font, iconSize, iconPos, iconColor, icon);

        const std::string displayName = node.name.size() > 32 ? node.name.substr(0, 29) + "..." : node.name;
        const float labelWrapWidth = tileSize.x - 12.0f;
        const float labelWidth = std::min(
            labelWrapWidth,
            font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, displayName.c_str()).x);
        const ImVec2 labelPos(tileMin.x + (tileSize.x - labelWidth) * 0.5f, tileMin.y + 66.0f);
        const ImVec4 labelClip(tileMin.x + 4.0f, tileMin.y + 64.0f, tileMax.x - 4.0f, tileMax.y - 4.0f);
        drawList->AddText(
            font,
            ImGui::GetFontSize(),
            labelPos,
            ImGui::GetColorU32(ImGuiCol_Text),
            displayName.c_str(),
            nullptr,
            labelWrapWidth,
            &labelClip);

        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(node.name.c_str());
            ImGui::TextDisabled("%s", FileTypeForNode(state, node).c_str());
            const std::string size = FileSizeForNode(state, node);
            if (!size.empty()) {
                ImGui::TextDisabled("%s", size.c_str());
            }
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    };

    if (ImGui::BeginChild("ContentTiles", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tileStride));
        std::vector<int> visibleNodes;
        visibleNodes.reserve(state.assetFilter == AssetFilter::All
                                 ? currentFolder.folders.size() + currentFolder.files.size()
                                 : state.browser.nodes.size());

        auto addNodeIfVisible = [&](int nodeIndex) {
            const ArchiveNode& node = state.browser.nodes[nodeIndex];
            if (!nodeMatchesFilter(node)) {
                return;
            }
            visibleNodes.push_back(nodeIndex);
        };

        if (state.assetFilter == AssetFilter::All) {
            for (const int folderIndex : currentFolder.folders) {
                addNodeIfVisible(folderIndex);
            }
            for (const int fileIndex : currentFolder.files) {
                addNodeIfVisible(fileIndex);
            }
        } else {
            for (int nodeIndex = 1; nodeIndex < static_cast<int>(state.browser.nodes.size()); ++nodeIndex) {
                addNodeIfVisible(nodeIndex);
            }
        }

        if (visibleNodes.empty()) {
            ImGui::TextDisabled("No matching items.");
        } else {
            const int rowCount = static_cast<int>((visibleNodes.size() + columns - 1) / columns);
            const float rowHeight = tileSize.y + ImGui::GetStyle().ItemSpacing.y;
            ImGuiListClipper clipper;
            clipper.Begin(rowCount, rowHeight);
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const int firstIndex = row * columns;
                    const int lastIndex = std::min(firstIndex + columns, static_cast<int>(visibleNodes.size()));
                    for (int index = firstIndex; index < lastIndex; ++index) {
                        if (index > firstIndex) {
                            ImGui::SameLine();
                        }
                        drawTile(visibleNodes[index]);
                    }
                }
            }
        }
    }
    ImGui::EndChild();
}

void DrawStatusSummary(AppState& state) {
    const DumpSnapshot dump = GetDumpSnapshot(state);
    if (!dump.active) {
        return;
    }

    const float fraction = dump.totalFiles == 0
                               ? 0.0f
                               : static_cast<float>(dump.filesWritten) /
                                     static_cast<float>(dump.totalFiles);
    const std::string label =
        std::to_string(dump.filesWritten) + " / " + std::to_string(dump.totalFiles);
    ImGui::ProgressBar(fraction, ImVec2(280.0f, 0.0f), label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", dump.message.c_str());

    if (!dump.currentPath.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", dump.currentPath.c_str());
    }
}

void DrawAssetFilterButton(AppState& state, AssetFilter filter) {
    const bool selected = state.assetFilter == filter;
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
    }

    if (ImGui::Button(AssetFilterLabel(filter))) {
        state.assetFilter = filter;
    }

    if (selected) {
        ImGui::PopStyleColor(2);
    }
}

void DrawAssetFilterBar(AppState& state) {
    constexpr AssetFilter filters[] = {
        AssetFilter::All,
        AssetFilter::Textures,
        AssetFilter::Models,
        AssetFilter::Levels,
        AssetFilter::Fx,
        AssetFilter::Audio,
    };

    if (!state.archiveLoaded) {
        ImGui::BeginDisabled();
    }

    for (std::size_t index = 0; index < std::size(filters); ++index) {
        if (index > 0) {
            ImGui::SameLine();
        }
        DrawAssetFilterButton(state, filters[index]);
    }

    if (!state.archiveLoaded) {
        ImGui::EndDisabled();
    }
}

void DrawBottomPanel(AppState& state) {
    ImGui::BeginChild("ContentBrowserDock", ImVec2(0.0f, state.bottomPanelHeight), ImGuiChildFlags_Borders);

    const float fullWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(180.0f, fullWidth * 0.20f);
    leftWidth = std::min(leftWidth, std::max(120.0f, fullWidth - 320.0f));

    const bool dumpActive = IsDumpActive(state);
    const float actionButtonSize = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    DrawAssetFilterBar(state);
    ImGui::SameLine();

    const float buttonStart =
        ImGui::GetWindowContentRegionMax().x - (actionButtonSize * 2.0f) - spacing;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), buttonStart));

    if (dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(actionButtonSize, 0.0f))) {
        OpenGtcDialog();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Browse for GAMEDATA.GTC");
    }
    if (dumpActive) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!state.archiveLoaded || dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_FA_DOWNLOAD, ImVec2(actionButtonSize, 0.0f))) {
        OpenDumpDirectoryDialog(state);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Dump all files");
    }
    if (!state.archiveLoaded || dumpActive) {
        ImGui::EndDisabled();
    }

    const float statusHeight = dumpActive ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    const float bodyHeight = std::max(60.0f, ImGui::GetContentRegionAvail().y - statusHeight);
    if (ImGui::BeginChild("ContentBrowserBody", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None)) {
        ImGui::BeginChild("ArchiveTree", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders);
        if (state.archiveLoaded && !state.browser.nodes.empty()) {
            for (const int folderIndex : state.browser.nodes[0].folders) {
                DrawFolderTreeNode(state, folderIndex);
            }
        } else {
            ImGui::TextDisabled("No archive loaded.");
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("FolderView", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        DrawRightPane(state);
        ImGui::EndChild();
    }
    ImGui::EndChild();

    if (dumpActive) {
        DrawStatusSummary(state);
    }
    ImGui::EndChild();
}

void DrawHorizontalSplitter(AppState& state, float availableHeight) {
    constexpr float splitterHeight = 7.0f;
    const float minHeight = 150.0f;
    const float maxHeight = std::max(minHeight, availableHeight * 0.70f);
    state.bottomPanelHeight = std::clamp(state.bottomPanelHeight, minHeight, maxHeight);

    ImGui::InvisibleButton("##BottomSplitter", ImVec2(-FLT_MIN, splitterHeight));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (active) {
        state.bottomPanelHeight = std::clamp(
            state.bottomPanelHeight - ImGui::GetIO().MouseDelta.y,
            minHeight,
            maxHeight);
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 color = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(min, max, color, 1.0f);
}

void DrawEmptyState(AppState& state);

void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cellSize) {
    const ImU32 dark = IM_COL32(45, 48, 50, 255);
    const ImU32 light = IM_COL32(72, 76, 80, 255);
    for (float y = min.y; y < max.y; y += cellSize) {
        for (float x = min.x; x < max.x; x += cellSize) {
            const bool alternate =
                (static_cast<int>((x - min.x) / cellSize) + static_cast<int>((y - min.y) / cellSize)) % 2 == 0;
            drawList->AddRectFilled(
                ImVec2(x, y),
                ImVec2(std::min(x + cellSize, max.x), std::min(y + cellSize, max.y)),
                alternate ? dark : light);
        }
    }
}

void UpdateTexturePreviewAnimation(TexturePreview& preview) {
    if (!preview.animated || preview.frames.size() <= 1 || preview.frameTicks.empty()) {
        return;
    }

    const double now = ImGui::GetTime();
    if (preview.lastFrameTime <= 0.0) {
        preview.lastFrameTime = now;
        return;
    }

    const int ticks = preview.frameIndex < static_cast<int>(preview.frameTicks.size())
                          ? preview.frameTicks[preview.frameIndex]
                          : 1;
    const double frameSeconds = static_cast<double>(std::max(1, ticks)) * 0.08;
    if (now - preview.lastFrameTime < frameSeconds) {
        return;
    }

    preview.lastFrameTime = now;
    preview.frameIndex = (preview.frameIndex + 1) % static_cast<int>(preview.frames.size());
    preview.decoded = preview.frames[preview.frameIndex];
    UploadPreviewTexture(preview);
}

void DrawTextPreview(AppState& state) {
    TextPreview& preview = state.textPreview;
    if (!preview.open) {
        return;
    }

    if (ImGui::SmallButton("x##CloseTextPreview")) {
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_FILE_LINES, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", preview.status.c_str());
    }

    if (!preview.path.empty()) {
        ImGui::TextDisabled("%s", preview.path.c_str());
    }
    ImGui::Separator();

    ImGui::BeginChild(
        "TextPreviewContent",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);
    if (preview.content.empty()) {
        ImGui::TextDisabled("Empty text file.");
    } else {
        ImGui::TextUnformatted(preview.content.c_str(), preview.content.c_str() + preview.content.size());
    }
    ImGui::EndChild();
}

void DrawTexturePreview(AppState& state) {
    TexturePreview& preview = state.texturePreview;
    if (!preview.open) {
        return;
    }

    try {
        UpdateTexturePreviewAnimation(preview);
    } catch (const std::exception& error) {
        preview.status = std::string("Animation update failed: ") + error.what();
        LogDebug(preview.status);
    }

    if (ImGui::SmallButton("x##CloseTexturePreview")) {
        DestroyPreviewTexture(preview);
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(preview.name.c_str());

    if (preview.decoded.width > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%dx%d  %d-bit  %d mips",
            preview.decoded.width,
            preview.decoded.height,
            preview.decoded.bitsPerPixel,
            preview.decoded.mipLevels);
        if (preview.animated) {
            ImGui::SameLine();
            ImGui::TextDisabled(
                "frame %d/%d",
                preview.frameIndex + 1,
                static_cast<int>(preview.frames.size()));
        }
    }

    bool changed = false;
    ImGui::SameLine();
    changed |= ImGui::Checkbox("R##PreviewRed", &preview.red);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("G##PreviewGreen", &preview.green);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("B##PreviewBlue", &preview.blue);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("A##PreviewAlpha", &preview.alpha);

    if (changed && !preview.decoded.rgba.empty()) {
        try {
            UploadPreviewTexture(preview);
            preview.status.clear();
        } catch (const std::exception& error) {
            preview.status = std::string("Preview update failed: ") + error.what();
            LogDebug(preview.status);
        }
    }

    if (!preview.status.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
    }

    ImGui::Separator();
    if (preview.textureId == 0 || preview.decoded.width <= 0 || preview.decoded.height <= 0) {
        return;
    }

    ImGui::BeginChild(
        "TexturePreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    const float scale = std::clamp(
        std::min(
            canvasSize.x / static_cast<float>(preview.decoded.width),
            canvasSize.y / static_cast<float>(preview.decoded.height)),
        0.05f,
        16.0f);
    const ImVec2 imageSize(
        static_cast<float>(preview.decoded.width) * scale,
        static_cast<float>(preview.decoded.height) * scale);
    const ImVec2 imageMin(
        canvasMin.x + std::max(0.0f, (canvasSize.x - imageSize.x) * 0.5f),
        canvasMin.y + std::max(0.0f, (canvasSize.y - imageSize.y) * 0.5f));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawCheckerboard(drawList, imageMin, imageMax, 12.0f);
    ImGui::SetCursorScreenPos(imageMin);
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<std::uintptr_t>(preview.textureId));
    ImGui::Image(ImTextureRef(textureId), imageSize);

    ImGui::EndChild();
}

std::string FormatAudioTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const int totalSeconds = static_cast<int>(std::floor(seconds + 0.5));
    const int minutes = totalSeconds / 60;
    const int remainingSeconds = totalSeconds % 60;
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, remainingSeconds);
    return buffer;
}

bool AudioIconButton(const char* icon, const char* id, const char* tooltip, ImVec2 size) {
    const std::string label = std::string(icon) + "##" + id;
    const bool clicked = ImGui::Button(label.c_str(), size);
    if (ImGui::IsItemHovered() && tooltip != nullptr) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

void UpdateFxPreviewAnimation(FxPreview& preview) {
    if (!preview.open || !preview.playing || preview.frames.size() <= 1) {
        return;
    }

    const double now = ImGui::GetTime();
    if (preview.lastFrameTime <= 0.0) {
        preview.lastFrameTime = now;
        return;
    }

    const int ticks = preview.frameIndex < static_cast<int>(preview.frameTicks.size())
                          ? preview.frameTicks[preview.frameIndex]
                          : 1;
    const double frameSeconds =
        static_cast<double>(std::max(1, ticks)) / static_cast<double>(std::max(1.0f, preview.fps));
    if (now - preview.lastFrameTime < frameSeconds) {
        return;
    }

    int nextFrame = preview.frameIndex + 1;
    if (nextFrame >= static_cast<int>(preview.frames.size())) {
        if (!preview.loop) {
            preview.playing = false;
            preview.lastFrameTime = now;
            return;
        }
        nextFrame = 0;
    }

    preview.lastFrameTime = now;
    SetFxPreviewFrame(preview, nextFrame);
}

void DrawFxPreview(AppState& state) {
    FxPreview& preview = state.fxPreview;
    if (!preview.open) {
        return;
    }

    try {
        UpdateFxPreviewAnimation(preview);
    } catch (const std::exception& error) {
        preview.status = std::string("FX animation failed: ") + error.what();
        LogDebug(preview.status);
    }

    if (ImGui::SmallButton("x##CloseFxPreview")) {
        StopFxPreview(preview);
        state.status = "FX preview closed.";
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_BOLT, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.frames.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    if (!preview.frames.empty()) {
        if (preview.frames.size() > 1) {
            if (AudioIconButton(
                    preview.playing ? ICON_FA_PAUSE : ICON_FA_PLAY,
                    "FxPlayPause",
                    preview.playing ? "Pause" : "Play",
                    ImVec2(34.0f, 0.0f))) {
                preview.playing = !preview.playing;
                preview.lastFrameTime = ImGui::GetTime();
            }
            ImGui::SameLine();
            if (AudioIconButton(ICON_FA_STOP, "FxStop", "Stop", ImVec2(34.0f, 0.0f))) {
                preview.playing = false;
                SetFxPreviewFrame(preview, 0);
                preview.lastFrameTime = ImGui::GetTime();
            }
            ImGui::SameLine();
            const std::string loopLabel = std::string(ICON_FA_REPEAT) + "##FxLoop";
            ImGui::Checkbox(loopLabel.c_str(), &preview.loop);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Loop");
            }

            int frameNumber = preview.frameIndex + 1;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::SliderInt(
                    "##FxFrame",
                    &frameNumber,
                    1,
                    static_cast<int>(preview.frames.size()),
                    "frame %d")) {
                preview.playing = false;
                SetFxPreviewFrame(preview, frameNumber - 1);
                preview.lastFrameTime = ImGui::GetTime();
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderFloat("##FxFps", &preview.fps, 1.0f, 30.0f, "%.1f fps")) {
                preview.lastFrameTime = ImGui::GetTime();
            }
        }

        if (!preview.frameNames.empty() &&
            preview.frameIndex >= 0 &&
            preview.frameIndex < static_cast<int>(preview.frameNames.size())) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", std::filesystem::path(preview.frameNames[preview.frameIndex]).filename().string().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", preview.frameNames[preview.frameIndex].c_str());
            }
        }
    }

    ImGui::Separator();
    ImGui::BeginChild(
        "FxPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    if (preview.textureId == 0 || preview.decoded.width <= 0 || preview.decoded.height <= 0) {
        const char* message = preview.status.empty() ? "No FX frame loaded." : preview.status.c_str();
        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message);
        ImGui::EndChild();
        return;
    }

    const float scale = std::clamp(
        std::min(
            canvasSize.x / static_cast<float>(preview.decoded.width),
            canvasSize.y / static_cast<float>(preview.decoded.height)),
        0.05f,
        18.0f);
    const ImVec2 imageSize(
        static_cast<float>(preview.decoded.width) * scale,
        static_cast<float>(preview.decoded.height) * scale);
    const ImVec2 imageMin(
        canvasMin.x + std::max(0.0f, (canvasSize.x - imageSize.x) * 0.5f),
        canvasMin.y + std::max(0.0f, (canvasSize.y - imageSize.y) * 0.5f));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawCheckerboard(drawList, imageMin, imageMax, 12.0f);
    ImGui::SetCursorScreenPos(imageMin);
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<std::uintptr_t>(preview.textureId));
    ImGui::Image(ImTextureRef(textureId), imageSize);

    ImGui::EndChild();
}

void DrawAudioPreview(AppState& state) {
    AudioPreview& preview = state.audioPreview;
    if (!preview.open) {
        return;
    }

    if (ImGui::SmallButton("x##CloseAudioPreview")) {
        StopAudioPreview(preview);
        state.status = "Audio preview closed.";
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_MUSIC, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.decoded.samples.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    ImGui::Separator();
    ImGui::BeginChild(
        "AudioPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const float panelWidth = std::clamp(canvasSize.x - 48.0f, 300.0f, 720.0f);
    const float panelHeight = 164.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (canvasSize.y - panelHeight) * 0.5f));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (canvasSize.x - panelWidth) * 0.5f));

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + panelWidth);
    ImGui::TextDisabled("%s", preview.path.c_str());
    ImGui::PopTextWrapPos();

    const double duration =
        preview.decoded.sampleRate > 0
            ? static_cast<double>(preview.decoded.frameCount) / static_cast<double>(preview.decoded.sampleRate)
            : 0.0;
    std::uint64_t cursor = preview.cursorFrame.load(std::memory_order_relaxed);
    cursor = std::min(cursor, preview.decoded.frameCount);
    const double elapsed =
        preview.decoded.sampleRate > 0
            ? static_cast<double>(cursor) / static_cast<double>(preview.decoded.sampleRate)
            : 0.0;

    float progress =
        preview.decoded.frameCount > 0
            ? static_cast<float>(static_cast<double>(cursor) / static_cast<double>(preview.decoded.frameCount))
            : 0.0f;
    ImGui::SetNextItemWidth(panelWidth);
    if (ImGui::SliderFloat("##AudioSeek", &progress, 0.0f, 1.0f, "")) {
        const auto target =
            static_cast<std::uint64_t>(std::clamp(progress, 0.0f, 1.0f) * static_cast<float>(preview.decoded.frameCount));
        preview.cursorFrame.store(std::min(target, preview.decoded.frameCount), std::memory_order_relaxed);
    }

    ImGui::TextDisabled(
        "%s / %s  %d Hz",
        FormatAudioTime(elapsed).c_str(),
        FormatAudioTime(duration).c_str(),
        preview.decoded.sampleRate);

    const bool hasAudio = !preview.decoded.samples.empty();
    if (!hasAudio) {
        ImGui::BeginDisabled();
    }

    const bool playing = preview.playing.load(std::memory_order_relaxed);
    if (AudioIconButton(
            playing ? ICON_FA_PAUSE : ICON_FA_PLAY,
            "AudioPlayPause",
            playing ? "Pause" : "Play",
            ImVec2(46.0f, 38.0f))) {
        try {
            if (playing) {
                preview.playing.store(false, std::memory_order_relaxed);
            } else {
                if (preview.cursorFrame.load(std::memory_order_relaxed) >= preview.decoded.frameCount) {
                    preview.cursorFrame.store(0, std::memory_order_relaxed);
                }
                if (!preview.deviceInitialized) {
                    StartAudioPlayback(preview);
                } else {
                    preview.playing.store(true, std::memory_order_relaxed);
                }
                state.status = "Playing " + preview.path;
            }
        } catch (const std::exception& error) {
            preview.status = std::string("Audio playback failed: ") + error.what();
            state.status = preview.status;
            LogDebug(preview.status);
        }
    }
    ImGui::SameLine();
    if (AudioIconButton(ICON_FA_STOP, "AudioStop", "Stop", ImVec2(46.0f, 38.0f))) {
        preview.playing.store(false, std::memory_order_relaxed);
        preview.cursorFrame.store(0, std::memory_order_relaxed);
        state.status = "Stopped " + preview.path;
    }

    ImGui::SameLine();
    bool loop = preview.loop.load(std::memory_order_relaxed);
    const std::string loopLabel = std::string(ICON_FA_REPEAT) + "##AudioLoop";
    if (ImGui::Checkbox(loopLabel.c_str(), &loop)) {
        preview.loop.store(loop, std::memory_order_relaxed);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Loop");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(ICON_FA_VOLUME_HIGH);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(90.0f, panelWidth - 260.0f));
    float volumePercent = preview.volumeUi * 100.0f;
    if (ImGui::SliderFloat("##AudioVolume", &volumePercent, 0.0f, 100.0f, "%.0f%%")) {
        preview.volumeUi = std::clamp(volumePercent / 100.0f, 0.0f, 1.0f);
        preview.volume.store(std::clamp(preview.volumeUi, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    if (!hasAudio) {
        ImGui::EndDisabled();
    }

    ImGui::EndGroup();
    ImGui::EndChild();
}

std::array<float, 4> ModelMaterialColorFloats(std::uint32_t material) {
    static constexpr std::array<ImVec4, 12> kPalette = {
        ImVec4(0.95f, 0.68f, 0.18f, 1.0f),
        ImVec4(0.32f, 0.62f, 0.95f, 1.0f),
        ImVec4(0.84f, 0.28f, 0.24f, 1.0f),
        ImVec4(0.34f, 0.72f, 0.43f, 1.0f),
        ImVec4(0.78f, 0.55f, 0.95f, 1.0f),
        ImVec4(0.92f, 0.82f, 0.48f, 1.0f),
        ImVec4(0.48f, 0.72f, 0.78f, 1.0f),
        ImVec4(0.88f, 0.48f, 0.58f, 1.0f),
        ImVec4(0.74f, 0.74f, 0.78f, 1.0f),
        ImVec4(0.55f, 0.72f, 0.33f, 1.0f),
        ImVec4(0.93f, 0.57f, 0.32f, 1.0f),
        ImVec4(0.40f, 0.48f, 0.82f, 1.0f),
    };
    const ImVec4 base = kPalette[material % kPalette.size()];
    return {base.x, base.y, base.z, 1.0f};
}

Vec3 TransformModelPointToWorld(const LevelModelInstance& instance, Vec3 point) {
    point.x *= instance.scale.x;
    point.y *= instance.scale.y;
    point.z *= instance.scale.z;
    return Add(instance.position, Rotate(instance.rotation, point));
}

Vec3 LevelWorldToDisplay(Vec3 point) {
    return {-point.x, point.y, point.z};
}

struct PendingModelRender {
    ModelPreview* preview = nullptr;
    ImVec2 canvasMin;
    ImVec2 canvasMax;
};

PendingModelRender gPendingModelRender;

void EmitModelTexCoord(const ModelVertex& vertex, unsigned char coordinateIndex) {
    unsigned char uvSet = coordinateIndex;
    if (uvSet >= vertex.uvSetCount || uvSet >= vertex.uSets.size()) {
        uvSet = 0;
    }
    glTexCoord2f(vertex.uSets[uvSet], vertex.vSets[uvSet]);
}

void EmitModelVertex(const ModelVertex& vertex, unsigned char coordinateIndex = 0) {
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    EmitModelTexCoord(vertex, coordinateIndex);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitSkyboxVertex(const ModelVertex& vertex) {
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glTexCoord2f(vertex.u, vertex.v);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void ApplyModelTextureTiling(unsigned char) {
    // The MD2 blend tiling byte is not a direct OpenGL wrap mask. Whirled leaves
    // these textures repeatable; clamping smears edge texels across UVs outside 0..1.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

GLuint BuildModelDisplayList(const ModelPreview& preview, bool skyboxUv = false) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    glNewList(displayList, GL_COMPILE);
    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        GLuint textureId = 0;
        if (section.textureIndex < preview.materials.size()) {
            textureId = preview.materials[section.textureIndex].textureId;
        }
        if (textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureId);
            ApplyModelTextureTiling(section.textureTiling);
            glColor4f(1.0f, 1.0f, 1.0f, section.alpha);
            if (section.alphaType == 0) {
                glDisable(GL_BLEND);
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.5f);
            } else if (section.alphaType == 1) {
                glDisable(GL_ALPHA_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            } else if (section.alphaType == 4) {
                glDisable(GL_ALPHA_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDisable(GL_CULL_FACE);
            } else {
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_BLEND);
            }
        } else {
            const auto color = ModelMaterialColorFloats(section.textureIndex);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            glColor4f(color[0], color[1], color[2], color[3]);
        }

        glBegin(GL_TRIANGLES);
        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= preview.vertices.size() ||
                triangle.b >= preview.vertices.size() ||
                triangle.c >= preview.vertices.size()) {
                continue;
            }
            if (skyboxUv) {
                EmitSkyboxVertex(preview.vertices[triangle.a]);
                EmitSkyboxVertex(preview.vertices[triangle.b]);
                EmitSkyboxVertex(preview.vertices[triangle.c]);
            } else {
                EmitModelVertex(preview.vertices[triangle.a], section.textureCoordinateIndex);
                EmitModelVertex(preview.vertices[triangle.b], section.textureCoordinateIndex);
                EmitModelVertex(preview.vertices[triangle.c], section.textureCoordinateIndex);
            }
        }
        glEnd();
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();
    return displayList;
}

void RenderModelSelectionHighlight(const ModelPreview& preview) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.10f, 1.0f, 0.18f, 0.28f);

    glBegin(GL_TRIANGLES);
    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= preview.vertices.size() ||
                triangle.b >= preview.vertices.size() ||
                triangle.c >= preview.vertices.size()) {
                continue;
            }
            glVertex3f(
                preview.vertices[triangle.a].position.x,
                preview.vertices[triangle.a].position.y,
                preview.vertices[triangle.a].position.z);
            glVertex3f(
                preview.vertices[triangle.b].position.x,
                preview.vertices[triangle.b].position.y,
                preview.vertices[triangle.b].position.z);
            glVertex3f(
                preview.vertices[triangle.c].position.x,
                preview.vertices[triangle.c].position.y,
                preview.vertices[triangle.c].position.z);
        }
    }
    glEnd();

    const Vec3 min = preview.boundsMin;
    const Vec3 max = preview.boundsMax;
    const std::array<Vec3, 8> corners = {
        Vec3{min.x, min.y, min.z},
        Vec3{max.x, min.y, min.z},
        Vec3{max.x, max.y, min.z},
        Vec3{min.x, max.y, min.z},
        Vec3{min.x, min.y, max.z},
        Vec3{max.x, min.y, max.z},
        Vec3{max.x, max.y, max.z},
        Vec3{min.x, max.y, max.z},
    };
    static constexpr std::array<std::array<int, 2>, 12> kEdges = {{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glLineWidth(3.0f);
    glColor4f(0.0f, 1.0f, 0.12f, 1.0f);
    glBegin(GL_LINES);
    for (const auto& edge : kEdges) {
        const Vec3 a = corners[static_cast<std::size_t>(edge[0])];
        const Vec3 b = corners[static_cast<std::size_t>(edge[1])];
        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
    }
    glEnd();
}

void RenderModelSkeletonOpenGl(const ModelPreview& preview) {
    if (!preview.showSkeleton || preview.skeleton.empty()) {
        return;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
    glPointSize(5.0f);

    for (const SkeletonBone& bone : preview.skeleton) {
        if (bone.parent < 0 || bone.parent >= static_cast<int>(preview.skeleton.size())) {
            continue;
        }
        const bool selectedBranch =
            preview.selectedBone >= 0 &&
            IsBoneAffectedBySelection(preview, bone.id, preview.selectedBone);
        if (selectedBranch) {
            glColor4f(1.0f, 0.22f, 0.12f, 1.0f);
            glLineWidth(3.0f);
        } else {
            glColor4f(0.15f, 0.85f, 1.0f, 1.0f);
            glLineWidth(2.0f);
        }
        glBegin(GL_LINES);
        const Vec3 parent = preview.skeleton[bone.parent].worldPosition;
        const Vec3 child = bone.worldPosition;
        glVertex3f(parent.x, parent.y, parent.z);
        glVertex3f(child.x, child.y, child.z);
        glEnd();
    }

    glPointSize(5.0f);
    glColor4f(1.0f, 0.92f, 0.24f, 1.0f);
    glBegin(GL_POINTS);
    for (const SkeletonBone& bone : preview.skeleton) {
        if (bone.id != preview.selectedBone) {
            glVertex3f(bone.worldPosition.x, bone.worldPosition.y, bone.worldPosition.z);
        }
    }
    glEnd();

    if (preview.selectedBone >= 0 && preview.selectedBone < static_cast<int>(preview.skeleton.size())) {
        glPointSize(8.0f);
        glColor4f(1.0f, 0.08f, 0.04f, 1.0f);
        glBegin(GL_POINTS);
        const Vec3 selected = preview.skeleton[preview.selectedBone].worldPosition;
        glVertex3f(selected.x, selected.y, selected.z);
        glEnd();
    }
}

void RenderModelOpenGl(ModelPreview& preview, ImVec2 canvasMin, ImVec2 canvasMax) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return;
    }

    UpdateModelSkinning(preview);
    const std::vector<ModelVertex>& renderVertices =
        preview.skinnedVertices.size() == preview.vertices.size() ? preview.skinnedVertices : preview.vertices;

    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scaleX = io.DisplayFramebufferScale.x;
    const float scaleY = io.DisplayFramebufferScale.y;

    int x = static_cast<int>((canvasMin.x - viewport->Pos.x) * scaleX);
    int y = static_cast<int>(static_cast<float>(gClientHeight) - (canvasMax.y - viewport->Pos.y) * scaleY);
    int width = static_cast<int>((canvasMax.x - canvasMin.x) * scaleX);
    int height = static_cast<int>((canvasMax.y - canvasMin.y) * scaleY);

    x = std::clamp(x, 0, gClientWidth);
    y = std::clamp(y, 0, gClientHeight);
    width = std::clamp(width, 0, gClientWidth - x);
    height = std::clamp(height, 0, gClientHeight - y);
    if (width <= 1 || height <= 1) {
        return;
    }

    GLint previousProgram = 0;
    if (gGlUseProgram != nullptr) {
        glGetIntegerv(kGlCurrentProgram, &previousProgram);
        gGlUseProgram(0);
    }

    if (gLastLoggedModelRenderPath != preview.path) {
        const auto loadedTextures = std::count_if(
            preview.materials.begin(),
            preview.materials.end(),
            [](const ModelMaterial& material) {
                return material.textureId != 0;
            });
        const auto visibleSections = std::count_if(
            preview.sections.begin(),
            preview.sections.end(),
            [](const ModelSection& section) {
                return section.visible && section.triangleCount > 0;
            });
        LogDebug(
            "render model opengl: " + preview.path +
            " viewport=" + std::to_string(width) + "x" + std::to_string(height) +
            " radius=" + std::to_string(preview.radius) +
            " textures=" + std::to_string(loadedTextures) + "/" + std::to_string(preview.materials.size()) +
            " visible_sections=" + std::to_string(visibleSections) + "/" + std::to_string(preview.sections.size()) +
            " uv_as_authored=1 render_texture_flip_y=1" +
            " previous_program=" + std::to_string(previousProgram));
        gLastLoggedModelRenderPath = preview.path;
    }

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_TEXTURE_BIT |
        GL_LIGHTING_BIT |
        GL_CURRENT_BIT |
        GL_VIEWPORT_BIT |
        GL_SCISSOR_BIT |
        GL_POLYGON_BIT |
        GL_LINE_BIT |
        GL_POINT_BIT);

    glViewport(x, y, width, height);
    glScissor(x, y, width, height);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.070f, 0.074f, 0.078f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float halfHeight = std::max(0.001f, preview.radius * 1.18f / std::max(preview.zoom, 0.001f));
    const float halfWidth = halfHeight * aspect;
    const float depth = std::max(1.0f, preview.radius * 4.0f);
    glOrtho(-halfWidth, halfWidth, -halfHeight, halfHeight, -depth, depth);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(preview.pitch * 57.2957795f, 1.0f, 0.0f, 0.0f);
    glRotatef(preview.yaw * 57.2957795f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-preview.center.x, -preview.center.y, -preview.center.z);

    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        GLuint textureId = 0;
        if (section.textureIndex < preview.materials.size()) {
            textureId = preview.materials[section.textureIndex].textureId;
        }

        if (textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureId);
            ApplyModelTextureTiling(section.textureTiling);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            const auto color = ModelMaterialColorFloats(section.textureIndex);
            glDisable(GL_TEXTURE_2D);
            glColor4f(color[0], color[1], color[2], color[3]);
        }

        glBegin(GL_TRIANGLES);
        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= renderVertices.size() ||
                triangle.b >= renderVertices.size() ||
                triangle.c >= renderVertices.size()) {
                continue;
            }
            EmitModelVertex(renderVertices[triangle.a], section.textureCoordinateIndex);
            EmitModelVertex(renderVertices[triangle.b], section.textureCoordinateIndex);
            EmitModelVertex(renderVertices[triangle.c], section.textureCoordinateIndex);
        }
        glEnd();
    }

    RenderModelSkeletonOpenGl(preview);

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    if (gGlUseProgram != nullptr) {
        gGlUseProgram(static_cast<GLuint>(previousProgram));
    }
}

void RenderModelDrawCallback(const ImDrawList*, const ImDrawCmd* command) {
    auto* request = static_cast<PendingModelRender*>(command->UserCallbackData);
    if (request == nullptr || request->preview == nullptr) {
        return;
    }
    RenderModelOpenGl(*request->preview, request->canvasMin, request->canvasMax);
}

struct PendingLevelRender {
    LevelPreview* preview = nullptr;
    ImVec2 canvasMin;
    ImVec2 canvasMax;
};

PendingLevelRender gPendingLevelRender;

std::array<float, 3> LevelTerrainColor(const LevelPreview& preview, float height) {
    const float span = std::max(0.001f, preview.boundsMax.y - preview.boundsMin.y);
    const float t = std::clamp((height - preview.boundsMin.y) / span, 0.0f, 1.0f);
    return {
        0.12f + 0.38f * t,
        0.30f + 0.42f * t,
        0.16f + 0.20f * t,
    };
}

bool IsCheckpointObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    return className == "ccheckpoint" || className.find("checkpoint") != std::string::npos;
}

std::string WorldObjectSearchText(const WorldObject& object) {
    std::string text = object.className + " " + object.name + " " + object.binding + " " +
                       object.assetPath + " " + object.modelPath;
    return ToLower(std::move(text));
}

bool IsStartPositionObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    const std::string path = ToLower(object.modelPath.empty() ? object.assetPath : object.modelPath);
    const std::string objectText = WorldObjectSearchText(object);
    return className == "cracestartpos" ||
           className == "cfoyerstartpos" ||
           className.find("startpos") != std::string::npos ||
           className.find("racestart") != std::string::npos ||
           path.find("cpfloater.md2") != std::string::npos ||
           objectText.find("cpfloater") != std::string::npos;
}

bool IsWeaponPickupObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    const std::string path = ToLower(object.modelPath.empty() ? object.assetPath : object.modelPath);
    return className == "cweaponpickup" ||
           className.find("weapon") != std::string::npos ||
           path.find("\\weapons\\") != std::string::npos ||
           path.find("weapon") != std::string::npos;
}

bool IsAiObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    return className == "caisection" ||
           className == "caiquad" ||
           className == "cspline" ||
           className.rfind("cai", 0) == 0 ||
           className.find("ai") != std::string::npos;
}

bool HasActiveTerrainSection(const LevelPreview& preview) {
    return preview.selectedTerrainSection >= 0 &&
           preview.selectedTerrainSection < static_cast<int>(preview.terrainSections.size());
}

const LevelTerrainSection* ActiveTerrainSection(const LevelPreview& preview) {
    if (!HasActiveTerrainSection(preview)) {
        return nullptr;
    }
    return &preview.terrainSections[static_cast<std::size_t>(preview.selectedTerrainSection)];
}

bool IsLayerVisibleForActiveSublevel(const LevelPreview& preview, bool hasLayer, std::uint32_t layer) {
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section == nullptr || !section->hasLayer) {
        return true;
    }
    return hasLayer && layer == section->layer;
}

bool IsObjectInActiveSublevel(const LevelPreview& preview, const WorldObject& object) {
    return IsLayerVisibleForActiveSublevel(preview, object.hasLayer, object.layer);
}

bool IsTerrainSectionVisibleForActiveSublevel(const LevelPreview& preview, std::size_t sectionIndex) {
    if (!HasActiveTerrainSection(preview)) {
        return true;
    }
    return sectionIndex == static_cast<std::size_t>(preview.selectedTerrainSection);
}

std::array<float, 4> LevelObjectColor(const WorldObject& object, bool selected) {
    if (selected) {
        return {1.0f, 0.90f, 0.14f, 1.0f};
    }

    const std::string className = ToLower(object.className);
    if (IsStartPositionObject(object)) {
        return {0.35f, 1.0f, 0.38f, 1.0f};
    }
    if (className.find("checkpoint") != std::string::npos) {
        return {1.0f, 0.32f, 0.20f, 1.0f};
    }
    if (className.find("pickup") != std::string::npos ||
        className.find("bonus") != std::string::npos ||
        className.find("weapon") != std::string::npos) {
        return {0.92f, 0.42f, 1.0f, 1.0f};
    }
    if (IsAiObject(object)) {
        return {0.42f, 0.78f, 1.0f, 1.0f};
    }
    if (className.find("water") != std::string::npos) {
        return {0.22f, 0.80f, 1.0f, 1.0f};
    }
    if (className.find("camera") != std::string::npos) {
        return {0.96f, 0.82f, 0.30f, 1.0f};
    }
    if (className.find("static") != std::string::npos ||
        className.find("mobile") != std::string::npos) {
        return {0.54f, 0.82f, 1.0f, 1.0f};
    }
    return {0.78f, 0.82f, 0.86f, 1.0f};
}

void EmitLevelVertex(const LevelPreview& preview,
                     const LevelVertex& vertex,
                     bool textured,
                     float textureScaleX = 1.0f,
                     float textureScaleY = 1.0f) {
    if (textured) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    } else {
        const auto color = LevelTerrainColor(preview, vertex.position.y);
        glColor4f(color[0], color[1], color[2], 1.0f);
    }
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitLevelVertexLayer(const LevelVertex& vertex,
                          int layer,
                          float textureScaleX,
                          float textureScaleY) {
    glColor4ub(255, 255, 255, vertex.mix[static_cast<std::size_t>(layer)]);
    glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitLevelVertexTerrainShader(const LevelVertex& vertex,
                                  float textureScaleX,
                                  float textureScaleY) {
    glColor4ub(vertex.mix[0], vertex.mix[1], vertex.mix[2], vertex.mix[3]);
    glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

bool HasWhirledTerrainTextures(const LevelTerrainSection& section) {
    return std::any_of(section.layerTextureIds.begin(), section.layerTextureIds.end(), [](GLuint textureId) {
        return textureId != 0;
    });
}

std::vector<std::uint32_t> CollectTerrainMaterialKeys(const std::vector<LevelTriangle>& triangles) {
    std::vector<std::uint32_t> keys;
    for (const LevelTriangle& triangle : triangles) {
        if (std::find(keys.begin(), keys.end(), triangle.materialKey) == keys.end()) {
            keys.push_back(triangle.materialKey);
        }
    }
    return keys;
}

GLuint BuildLevelTerrainDisplayList(const LevelPreview& preview,
                                    const std::vector<LevelVertex>& vertices,
                                    const std::vector<LevelTriangle>& triangles,
                                    bool textured,
                                    float textureScaleX,
                                    float textureScaleY) {
    if (vertices.empty() || triangles.empty()) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    glNewList(displayList, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : triangles) {
        if (triangle.a >= vertices.size() ||
            triangle.b >= vertices.size() ||
            triangle.c >= vertices.size()) {
            continue;
        }
        EmitLevelVertex(preview, vertices[triangle.a], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.b], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.c], textured, textureScaleX, textureScaleY);
    }
    glEnd();
    glEndList();
    return displayList;
}

GLuint BuildWhirledTerrainFixedDisplayList(const LevelTerrainSection& section) {
    if (section.vertices.empty() || section.triangles.empty() || !HasWhirledTerrainTextures(section)) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    (void)BlackTextureId();
    const std::vector<std::uint32_t> materialKeys = CollectTerrainMaterialKeys(section.triangles);
    glNewList(displayList, GL_COMPILE);
    glDisable(GL_CULL_FACE);

    // Match Whirled's shader blend in fixed-function OpenGL: first write depth, then add each texture layer by mix.
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : section.triangles) {
        if (triangle.a >= section.vertices.size() ||
            triangle.b >= section.vertices.size() ||
            triangle.c >= section.vertices.size()) {
            continue;
        }
        const LevelVertex& a = section.vertices[triangle.a];
        const LevelVertex& b = section.vertices[triangle.b];
        const LevelVertex& c = section.vertices[triangle.c];
        glNormal3f(a.normal.x, a.normal.y, a.normal.z);
        glVertex3f(a.position.x, a.position.y, a.position.z);
        glNormal3f(b.normal.x, b.normal.y, b.normal.z);
        glVertex3f(b.position.x, b.position.y, b.position.z);
        glNormal3f(c.normal.x, c.normal.y, c.normal.z);
        glVertex3f(c.position.x, c.position.y, c.position.z);
    }
    glEnd();

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    for (std::uint32_t materialKey : materialKeys) {
        for (int layer = 0; layer < 4; ++layer) {
            const auto textureIndex = static_cast<unsigned char>((materialKey >> (layer * 8)) & 0xffU);
            if (textureIndex == 0xffU || section.layerTextureIds[textureIndex] == 0) {
                continue;
            }

            glBindTexture(GL_TEXTURE_2D, section.layerTextureIds[textureIndex]);
            glBegin(GL_TRIANGLES);
            for (const LevelTriangle& triangle : section.triangles) {
                if (triangle.materialKey != materialKey ||
                    triangle.a >= section.vertices.size() ||
                    triangle.b >= section.vertices.size() ||
                    triangle.c >= section.vertices.size()) {
                    continue;
                }
                EmitLevelVertexLayer(section.vertices[triangle.a], layer, section.textureScaleX, section.textureScaleY);
                EmitLevelVertexLayer(section.vertices[triangle.b], layer, section.textureScaleX, section.textureScaleY);
                EmitLevelVertexLayer(section.vertices[triangle.c], layer, section.textureScaleX, section.textureScaleY);
            }
            glEnd();
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEndList();
    return displayList;
}

void BindTerrainMaterialTextures(const LevelTerrainSection& section, std::uint32_t materialKey) {
    const GLuint blackTexture = BlackTextureId();
    for (int layer = 0; layer < 4; ++layer) {
        GLuint textureId = blackTexture;
        const auto textureIndex = static_cast<unsigned char>((materialKey >> (layer * 8)) & 0xffU);
        if (textureIndex != 0xffU && section.layerTextureIds[textureIndex] != 0) {
            textureId = section.layerTextureIds[textureIndex];
        }
        gGlActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(layer));
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
    gGlActiveTexture(GL_TEXTURE0);
}

void ResetTerrainMaterialTextures() {
    if (gGlActiveTexture == nullptr) {
        return;
    }
    for (int layer = 3; layer >= 1; --layer) {
        gGlActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(layer));
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
    gGlActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BuildWhirledTerrainDisplayList(const LevelTerrainSection& section) {
    if (section.vertices.empty() || section.triangles.empty() || !HasWhirledTerrainTextures(section)) {
        return 0;
    }
    if (EnsureTerrainShaderProgram() == 0 || gGlActiveTexture == nullptr) {
        return BuildWhirledTerrainFixedDisplayList(section);
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    const std::vector<std::uint32_t> materialKeys = CollectTerrainMaterialKeys(section.triangles);
    glNewList(displayList, GL_COMPILE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    for (std::uint32_t materialKey : materialKeys) {
        BindTerrainMaterialTextures(section, materialKey);
        glBegin(GL_TRIANGLES);
        for (const LevelTriangle& triangle : section.triangles) {
            if (triangle.materialKey != materialKey ||
                triangle.a >= section.vertices.size() ||
                triangle.b >= section.vertices.size() ||
                triangle.c >= section.vertices.size()) {
                continue;
            }
            EmitLevelVertexTerrainShader(section.vertices[triangle.a], section.textureScaleX, section.textureScaleY);
            EmitLevelVertexTerrainShader(section.vertices[triangle.b], section.textureScaleX, section.textureScaleY);
            EmitLevelVertexTerrainShader(section.vertices[triangle.c], section.textureScaleX, section.textureScaleY);
        }
        glEnd();
    }
    ResetTerrainMaterialTextures();
    glEndList();
    return displayList;
}

void RenderLevelTriangleList(const LevelPreview& preview,
                             const std::vector<LevelVertex>& vertices,
                             const std::vector<LevelTriangle>& triangles,
                             GLuint textureId,
                             const DecodedTexture& texture,
                             float textureScaleX,
                             float textureScaleY) {
    const bool textured =
        preview.showTerrainTexture &&
        textureId != 0 &&
        texture.width > 0 &&
        texture.height > 0;
    if (textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
    } else {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : triangles) {
        if (triangle.a >= vertices.size() ||
            triangle.b >= vertices.size() ||
            triangle.c >= vertices.size()) {
            continue;
        }
        EmitLevelVertex(preview, vertices[triangle.a], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.b], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.c], textured, textureScaleX, textureScaleY);
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderLevelTerrainSection(LevelPreview& preview, LevelTerrainSection& section) {
    const bool whirledTextured = preview.showTerrainTexture && HasWhirledTerrainTextures(section);
    if (whirledTextured) {
        const GLuint terrainShader = EnsureTerrainShaderProgram();
        const bool useTerrainShader = terrainShader != 0 && gGlActiveTexture != nullptr;
        if (section.displayListTextured == 0) {
            section.displayListTextured = BuildWhirledTerrainDisplayList(section);
        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if (useTerrainShader) {
            gGlUseProgram(terrainShader);
            for (int layer = 0; layer < 4; ++layer) {
                if (gTerrainTextureUniforms[layer] >= 0) {
                    gGlUniform1i(gTerrainTextureUniforms[layer], layer);
                }
            }
            if (gTerrainLightDirUniform >= 0) {
                gGlUniform3f(gTerrainLightDirUniform, 0.0f, 0.70710677f, 0.70710677f);
            }
            if (gTerrainAmbientUniform >= 0) {
                gGlUniform1f(gTerrainAmbientUniform, 0.48f);
            }
        }
        if (section.displayListTextured != 0) {
            glCallList(section.displayListTextured);
        }
        if (useTerrainShader) {
            ResetTerrainMaterialTextures();
            gGlUseProgram(0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    const bool textured =
        preview.showTerrainTexture &&
        section.textureId != 0 &&
        section.texture.width > 0 &&
        section.texture.height > 0;
    if (textured) {
        if (section.displayListTextured == 0) {
            section.displayListTextured = BuildLevelTerrainDisplayList(
                preview,
                section.vertices,
                section.triangles,
                true,
                section.textureScaleX,
                section.textureScaleY);
        }
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, section.textureId);
        if (section.displayListTextured != 0) {
            glCallList(section.displayListTextured);
        }
    } else {
        if (section.displayListColored == 0) {
            section.displayListColored = BuildLevelTerrainDisplayList(
                preview,
                section.vertices,
                section.triangles,
                false,
                1.0f,
                1.0f);
        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (section.displayListColored != 0) {
            glCallList(section.displayListColored);
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MultLevelTransform(Vec3 position, Quat rotation, Vec3 scale) {
    rotation = Normalize(rotation);
    if (Length(Vec3{rotation.x, rotation.y, rotation.z}) <= 0.00001f && std::abs(rotation.w) <= 0.00001f) {
        rotation = {};
    }

    const float x = rotation.x;
    const float y = rotation.y;
    const float z = rotation.z;
    const float w = rotation.w;
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    const float r00 = 1.0f - 2.0f * (yy + zz);
    const float r01 = 2.0f * (xy - wz);
    const float r02 = 2.0f * (xz + wy);
    const float r10 = 2.0f * (xy + wz);
    const float r11 = 1.0f - 2.0f * (xx + zz);
    const float r12 = 2.0f * (yz - wx);
    const float r20 = 2.0f * (xz - wy);
    const float r21 = 2.0f * (yz + wx);
    const float r22 = 1.0f - 2.0f * (xx + yy);

    const GLfloat matrix[16] = {
        r00 * scale.x, r10 * scale.x, r20 * scale.x, 0.0f,
        r01 * scale.y, r11 * scale.y, r21 * scale.y, 0.0f,
        r02 * scale.z, r12 * scale.z, r22 * scale.z, 0.0f,
        position.x, position.y, position.z, 1.0f,
    };
    glMultMatrixf(matrix);
}

bool IsLevelModelInstanceHidden(const LevelPreview& preview, const LevelModelInstance& instance) {
    if (instance.objectIndex < 0 || instance.objectIndex >= static_cast<int>(preview.objects.size())) {
        return false;
    }
    const WorldObject& object = preview.objects[instance.objectIndex];
    if (!IsObjectInActiveSublevel(preview, object)) {
        return true;
    }
    return (!preview.showCheckpoints && IsCheckpointObject(object)) ||
           (!preview.showStartPositions && IsStartPositionObject(object)) ||
           (!preview.showWeaponPickups && IsWeaponPickupObject(object)) ||
           (!preview.showAiObjects && IsAiObject(object));
}

void RenderLevelModelsOpenGl(LevelPreview& preview) {
    if (!preview.showObjects || preview.modelInstances.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    LevelModelInstance* selectedInstance = nullptr;
    LevelModelAsset* selectedAsset = nullptr;

    for (LevelModelInstance& instance : preview.modelInstances) {
        if (instance.assetIndex < 0 || instance.assetIndex >= static_cast<int>(preview.modelAssets.size())) {
            continue;
        }
        if (IsLevelModelInstanceHidden(preview, instance)) {
            continue;
        }

        LevelModelAsset& asset = preview.modelAssets[instance.assetIndex];
        if (!asset.loaded || asset.model.vertices.empty() || asset.model.triangles.empty()) {
            continue;
        }
        if (asset.displayList == 0) {
            asset.displayList = BuildModelDisplayList(asset.model, false);
        }
        if (asset.displayList == 0) {
            continue;
        }

        glPushMatrix();
        MultLevelTransform(instance.position, instance.rotation, instance.scale);
        glCallList(asset.displayList);
        glPopMatrix();

        if (instance.objectIndex == preview.selectedObject) {
            selectedInstance = &instance;
            selectedAsset = &asset;
        }
    }

    if (selectedInstance != nullptr && selectedAsset != nullptr) {
        glPushMatrix();
        MultLevelTransform(selectedInstance->position, selectedInstance->rotation, selectedInstance->scale);
        RenderModelSelectionHighlight(selectedAsset->model);
        glPopMatrix();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BuildWaterSheetDisplayList(const LevelWaterSheet& waterSheet) {
    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    const float halfWidth = waterSheet.width * 0.5f;
    const float halfDepth = waterSheet.depth * 0.5f;
    glNewList(displayList, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, 0.0f);
    glVertex3f(halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, waterSheet.vScale);
    glVertex3f(halfWidth, 0.0f, halfDepth);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, waterSheet.vScale);
    glVertex3f(halfWidth, 0.0f, halfDepth);
    glTexCoord2f(0.0f, waterSheet.vScale);
    glVertex3f(-halfWidth, 0.0f, halfDepth);
    glEnd();
    glEndList();
    return displayList;
}

void RenderLevelWaterOpenGl(LevelPreview& preview) {
    if (!preview.showWater || preview.waterSheets.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (LevelWaterSheet& waterSheet : preview.waterSheets) {
        if (waterSheet.objectIndex >= 0 && waterSheet.objectIndex < static_cast<int>(preview.objects.size()) &&
            !IsObjectInActiveSublevel(preview, preview.objects[waterSheet.objectIndex])) {
            continue;
        }
        if (waterSheet.displayList == 0) {
            waterSheet.displayList = BuildWaterSheetDisplayList(waterSheet);
        }
        if (waterSheet.displayList == 0) {
            continue;
        }

        if (waterSheet.textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, waterSheet.textureId);
            glColor4f(1.0f, 1.0f, 1.0f, waterSheet.alpha);
        } else {
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            glColor4f(0.26f, 0.58f, 0.82f, 0.58f);
        }

        glPushMatrix();
        MultLevelTransform(waterSheet.position, waterSheet.rotation, {1.0f, 1.0f, 1.0f});
        glCallList(waterSheet.displayList);
        glPopMatrix();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void RenderLevelSkyboxesOpenGl(LevelPreview& preview) {
    if (!preview.showSkybox || preview.skyboxAssetIndices.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);

    for (int assetIndex : preview.skyboxAssetIndices) {
        if (assetIndex < 0 || assetIndex >= static_cast<int>(preview.modelAssets.size())) {
            continue;
        }

        LevelModelAsset& asset = preview.modelAssets[assetIndex];
        if (!asset.loaded || asset.model.vertices.empty() || asset.model.triangles.empty()) {
            continue;
        }
        if (asset.skyboxDisplayList == 0) {
            asset.skyboxDisplayList = BuildModelDisplayList(asset.model, true);
        }
        if (asset.skyboxDisplayList == 0) {
            continue;
        }

        const float modelRadius = std::max(1.0f, asset.model.radius);
        const float targetRadius = std::max(512.0f, preview.radius * 2.75f);
        const float scale = targetRadius / modelRadius;
        glPushMatrix();
        glScalef(scale, scale, scale);
        glCallList(asset.skyboxDisplayList);
        glPopMatrix();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void RenderLevelObjectsOpenGl(const LevelPreview& preview) {
    if (!preview.showObjects ||
        preview.objects.empty() ||
        (!preview.showObjectMarkers && preview.selectedObject < 0)) {
        return;
    }

    const float markerHeight = std::clamp(preview.radius * 0.025f, 4.0f, 36.0f);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glPointSize(5.0f);

    glBegin(GL_LINES);
    for (std::size_t index = 0; index < preview.objects.size(); ++index) {
        const WorldObject& object = preview.objects[index];
        if (!IsObjectInActiveSublevel(preview, object)) {
            continue;
        }
        if (!object.hasPosition) {
            continue;
        }
        if (!preview.showCheckpoints && IsCheckpointObject(object)) {
            continue;
        }
        const bool startPosition = IsStartPositionObject(object);
        const bool weaponPickup = IsWeaponPickupObject(object);
        const bool aiObject = IsAiObject(object);
        const bool selected = static_cast<int>(index) == preview.selectedObject;
        if (!selected && startPosition && !preview.showStartPositions) {
            continue;
        }
        if (!selected && weaponPickup && !preview.showWeaponPickups) {
            continue;
        }
        if (!selected && aiObject && !preview.showAiObjects) {
            continue;
        }
        if (!selected && !preview.showObjectMarkers) {
            continue;
        }
        if (!selected && !object.modelPath.empty()) {
            continue;
        }
        const auto color = LevelObjectColor(object, selected);
        glColor4f(color[0], color[1], color[2], selected ? 1.0f : 0.70f);
        glVertex3f(object.position.x, object.position.y, object.position.z);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
    }
    glEnd();

    glBegin(GL_POINTS);
    for (std::size_t index = 0; index < preview.objects.size(); ++index) {
        const WorldObject& object = preview.objects[index];
        if (!IsObjectInActiveSublevel(preview, object)) {
            continue;
        }
        if (!object.hasPosition) {
            continue;
        }
        if (!preview.showCheckpoints && IsCheckpointObject(object)) {
            continue;
        }
        const bool startPosition = IsStartPositionObject(object);
        const bool weaponPickup = IsWeaponPickupObject(object);
        const bool aiObject = IsAiObject(object);
        const bool selected = static_cast<int>(index) == preview.selectedObject;
        if (!selected && startPosition && !preview.showStartPositions) {
            continue;
        }
        if (!selected && weaponPickup && !preview.showWeaponPickups) {
            continue;
        }
        if (!selected && aiObject && !preview.showAiObjects) {
            continue;
        }
        if (!selected && !preview.showObjectMarkers) {
            continue;
        }
        if (!selected && !object.modelPath.empty()) {
            continue;
        }
        const auto color = LevelObjectColor(object, selected);
        glColor4f(color[0], color[1], color[2], 1.0f);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
    }
    glEnd();

    if (preview.selectedObject >= 0 &&
        preview.selectedObject < static_cast<int>(preview.objects.size()) &&
        preview.objects[preview.selectedObject].hasPosition &&
        IsObjectInActiveSublevel(preview, preview.objects[preview.selectedObject])) {
        const WorldObject& object = preview.objects[preview.selectedObject];
        glPointSize(10.0f);
        glColor4f(1.0f, 0.96f, 0.12f, 1.0f);
        glBegin(GL_POINTS);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
        glEnd();
    }
}

bool ComputeActiveSublevelBounds(const LevelPreview& preview, Vec3& boundsMin, Vec3& boundsMax) {
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section == nullptr) {
        return false;
    }

    bool initialized = false;
    auto include = [&](Vec3 point) {
        if (!initialized) {
            boundsMin = point;
            boundsMax = point;
            initialized = true;
            return;
        }
        boundsMin.x = std::min(boundsMin.x, point.x);
        boundsMin.y = std::min(boundsMin.y, point.y);
        boundsMin.z = std::min(boundsMin.z, point.z);
        boundsMax.x = std::max(boundsMax.x, point.x);
        boundsMax.y = std::max(boundsMax.y, point.y);
        boundsMax.z = std::max(boundsMax.z, point.z);
    };

    for (const LevelVertex& vertex : section->vertices) {
        include(vertex.position);
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition && IsObjectInActiveSublevel(preview, object)) {
            include(object.position);
        }
    }
    return initialized;
}

Vec3 ActiveLevelCenter(const LevelPreview& preview) {
    Vec3 boundsMin{};
    Vec3 boundsMax{};
    if (ComputeActiveSublevelBounds(preview, boundsMin, boundsMax)) {
        return {
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f,
        };
    }
    return preview.center;
}

float ActiveLevelRadius(const LevelPreview& preview) {
    Vec3 boundsMin{};
    Vec3 boundsMax{};
    if (!ComputeActiveSublevelBounds(preview, boundsMin, boundsMax)) {
        return preview.radius;
    }

    const Vec3 center{
        (boundsMin.x + boundsMax.x) * 0.5f,
        (boundsMin.y + boundsMax.y) * 0.5f,
        (boundsMin.z + boundsMax.z) * 0.5f,
    };
    float radius = 0.0f;
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section != nullptr) {
        for (const LevelVertex& vertex : section->vertices) {
            radius = std::max(radius, Length(Subtract(vertex.position, center)));
        }
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition && IsObjectInActiveSublevel(preview, object)) {
            radius = std::max(radius, Length(Subtract(object.position, center)));
        }
    }
    return radius > 0.0001f && std::isfinite(radius) ? radius : preview.radius;
}

Vec3 LevelDisplayCenter(const LevelPreview& preview) {
    const Vec3 center = ActiveLevelCenter(preview);
    return {-center.x, center.y, center.z};
}

Vec3 LevelCameraForward(const LevelPreview& preview) {
    const float cp = std::cos(preview.pitch);
    return Normalize(Vec3{
        -std::sin(preview.yaw) * cp,
        std::sin(preview.pitch),
        -std::cos(preview.yaw) * cp,
    });
}

Vec3 LevelCameraRight(const LevelPreview& preview) {
    return Normalize(Vec3{std::cos(preview.yaw), 0.0f, -std::sin(preview.yaw)});
}

void EnsureLevelFlyCamera(LevelPreview& preview);

struct LevelPickRay {
    Vec3 origin;
    Vec3 direction;
};

LevelPickRay BuildLevelPickRay(const LevelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    const float normalizedX =
        ((mouse.x - canvasMin.x) / std::max(1.0f, canvasSize.x)) * 2.0f - 1.0f;
    const float normalizedY =
        1.0f - ((mouse.y - canvasMin.y) / std::max(1.0f, canvasSize.y)) * 2.0f;
    const float aspect = std::max(0.001f, canvasSize.x / std::max(1.0f, canvasSize.y));
    const float tanHalfFov = std::tan(60.0f * 0.01745329252f * 0.5f);
    const Vec3 forward = LevelCameraForward(preview);
    const Vec3 right = LevelCameraRight(preview);
    Vec3 up = Normalize(Cross(right, forward));
    if (Length(up) <= 0.0001f) {
        up = {0.0f, 1.0f, 0.0f};
    }

    return {
        preview.flyCameraPosition,
        Normalize(Add(
            Add(forward, Multiply(right, normalizedX * tanHalfFov * aspect)),
            Multiply(up, normalizedY * tanHalfFov))),
    };
}

bool RayIntersectsAabb(Vec3 origin, Vec3 direction, Vec3 boundsMin, Vec3 boundsMax, float& hitDistance) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float originAxis = GetAxis(origin, axis);
        const float directionAxis = GetAxis(direction, axis);
        const float minAxis = GetAxis(boundsMin, axis);
        const float maxAxis = GetAxis(boundsMax, axis);
        if (std::abs(directionAxis) <= 0.000001f) {
            if (originAxis < minAxis || originAxis > maxAxis) {
                return false;
            }
            continue;
        }

        float t1 = (minAxis - originAxis) / directionAxis;
        float t2 = (maxAxis - originAxis) / directionAxis;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return false;
        }
    }

    hitDistance = tMin >= 0.0f ? tMin : tMax;
    return hitDistance >= 0.0f && std::isfinite(hitDistance);
}

bool ComputeLevelModelDisplayBounds(const LevelModelInstance& instance,
                                    const ModelPreview& model,
                                    Vec3& boundsMin,
                                    Vec3& boundsMax) {
    if (model.vertices.empty() || model.triangles.empty()) {
        return false;
    }

    const Vec3 min = model.boundsMin;
    const Vec3 max = model.boundsMax;
    const std::array<Vec3, 8> corners = {
        Vec3{min.x, min.y, min.z},
        Vec3{max.x, min.y, min.z},
        Vec3{max.x, max.y, min.z},
        Vec3{min.x, max.y, min.z},
        Vec3{min.x, min.y, max.z},
        Vec3{max.x, min.y, max.z},
        Vec3{max.x, max.y, max.z},
        Vec3{min.x, max.y, max.z},
    };

    bool initialized = false;
    for (Vec3 corner : corners) {
        const Vec3 displayPoint = LevelWorldToDisplay(TransformModelPointToWorld(instance, corner));
        if (!initialized) {
            boundsMin = displayPoint;
            boundsMax = displayPoint;
            initialized = true;
            continue;
        }
        boundsMin.x = std::min(boundsMin.x, displayPoint.x);
        boundsMin.y = std::min(boundsMin.y, displayPoint.y);
        boundsMin.z = std::min(boundsMin.z, displayPoint.z);
        boundsMax.x = std::max(boundsMax.x, displayPoint.x);
        boundsMax.y = std::max(boundsMax.y, displayPoint.y);
        boundsMax.z = std::max(boundsMax.z, displayPoint.z);
    }
    return initialized;
}

int PickLevelModelObject(LevelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    if (!preview.showObjects || preview.modelInstances.empty()) {
        return -1;
    }

    EnsureLevelFlyCamera(preview);
    const LevelPickRay ray = BuildLevelPickRay(preview, mouse, canvasMin, canvasSize);
    if (Length(ray.direction) <= 0.0001f) {
        return -1;
    }

    float bestDistance = std::numeric_limits<float>::max();
    int bestObject = -1;
    for (const LevelModelInstance& instance : preview.modelInstances) {
        if (instance.objectIndex < 0 ||
            instance.objectIndex >= static_cast<int>(preview.objects.size()) ||
            instance.assetIndex < 0 ||
            instance.assetIndex >= static_cast<int>(preview.modelAssets.size()) ||
            IsLevelModelInstanceHidden(preview, instance)) {
            continue;
        }

        const LevelModelAsset& asset = preview.modelAssets[instance.assetIndex];
        if (!asset.loaded) {
            continue;
        }

        Vec3 boundsMin{};
        Vec3 boundsMax{};
        if (!ComputeLevelModelDisplayBounds(instance, asset.model, boundsMin, boundsMax)) {
            continue;
        }

        float hitDistance = 0.0f;
        if (RayIntersectsAabb(ray.origin, ray.direction, boundsMin, boundsMax, hitDistance) &&
            hitDistance < bestDistance) {
            bestDistance = hitDistance;
            bestObject = instance.objectIndex;
        }
    }

    return bestObject;
}

void EnsureLevelFlyCamera(LevelPreview& preview) {
    if (preview.flyCameraInitialized) {
        return;
    }

    preview.yaw = 0.0f;
    preview.pitch = -0.22f;
    const Vec3 center = LevelDisplayCenter(preview);
    const Vec3 forward = LevelCameraForward(preview);
    const float activeRadius = ActiveLevelRadius(preview);
    const float distance = std::max(48.0f, activeRadius * 1.45f);
    preview.flyCameraPosition = Subtract(center, Multiply(forward, distance));
    preview.flySpeed = std::max(12.0f, activeRadius * 0.42f);
    preview.flyCameraInitialized = true;
}

void LoadPerspectiveProjection(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float top = std::tan(fovYRadians * 0.5f) * nearPlane;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void RenderLevelOpenGl(LevelPreview& preview, ImVec2 canvasMin, ImVec2 canvasMax) {
    if (preview.vertices.empty() && preview.objects.empty()) {
        return;
    }
    EnsureLevelFlyCamera(preview);

    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scaleX = io.DisplayFramebufferScale.x;
    const float scaleY = io.DisplayFramebufferScale.y;

    int x = static_cast<int>((canvasMin.x - viewport->Pos.x) * scaleX);
    int y = static_cast<int>(static_cast<float>(gClientHeight) - (canvasMax.y - viewport->Pos.y) * scaleY);
    int width = static_cast<int>((canvasMax.x - canvasMin.x) * scaleX);
    int height = static_cast<int>((canvasMax.y - canvasMin.y) * scaleY);

    x = std::clamp(x, 0, gClientWidth);
    y = std::clamp(y, 0, gClientHeight);
    width = std::clamp(width, 0, gClientWidth - x);
    height = std::clamp(height, 0, gClientHeight - y);
    if (width <= 1 || height <= 1) {
        return;
    }

    GLint previousProgram = 0;
    if (gGlUseProgram != nullptr) {
        glGetIntegerv(kGlCurrentProgram, &previousProgram);
        gGlUseProgram(0);
    }

    if (gLastLoggedLevelRenderPath != preview.path) {
        LogDebug(
            "render level opengl: " + preview.path +
            " viewport=" + std::to_string(width) + "x" + std::to_string(height) +
            " radius=" + std::to_string(preview.radius) +
            " vertices=" + std::to_string(preview.vertices.size()) +
            " triangles=" + std::to_string(preview.triangles.size()) +
            " terrain_sections=" + std::to_string(preview.terrainSections.size()) +
            " objects=" + std::to_string(preview.objects.size()) +
            " model_instances=" + std::to_string(preview.modelInstances.size()) +
            " water_sheets=" + std::to_string(preview.waterSheets.size()) +
            " skyboxes=" + std::to_string(preview.skyboxAssetIndices.size()) +
            " model_assets=" + std::to_string(preview.modelAssets.size()) +
            " texture=" + LevelTerrainTextureSummary(preview) +
            " previous_program=" + std::to_string(previousProgram));
        gLastLoggedLevelRenderPath = preview.path;
    }

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_TEXTURE_BIT |
        GL_LIGHTING_BIT |
        GL_CURRENT_BIT |
        GL_VIEWPORT_BIT |
        GL_SCISSOR_BIT |
        GL_POLYGON_BIT |
        GL_LINE_BIT |
        GL_POINT_BIT);

    glViewport(x, y, width, height);
    glScissor(x, y, width, height);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.070f, 0.074f, 0.078f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float nearPlane = std::max(0.08f, preview.radius * 0.0005f);
    const float farPlane = std::max(4096.0f, preview.radius * 40.0f);
    LoadPerspectiveProjection(60.0f * 0.01745329252f, aspect, nearPlane, farPlane);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(-preview.pitch * 57.2957795f, 1.0f, 0.0f, 0.0f);
    glRotatef(-preview.yaw * 57.2957795f, 0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glScalef(-1.0f, 1.0f, 1.0f);
    RenderLevelSkyboxesOpenGl(preview);
    glPopMatrix();
    glTranslatef(-preview.flyCameraPosition.x, -preview.flyCameraPosition.y, -preview.flyCameraPosition.z);
    glScalef(-1.0f, 1.0f, 1.0f);

    if (preview.showTerrain) {
        if (!preview.terrainSections.empty()) {
            for (std::size_t sectionIndex = 0; sectionIndex < preview.terrainSections.size(); ++sectionIndex) {
                LevelTerrainSection& section = preview.terrainSections[sectionIndex];
                if (!IsTerrainSectionVisibleForActiveSublevel(preview, sectionIndex)) {
                    continue;
                }
                if (!section.show || section.vertices.empty() || section.triangles.empty()) {
                    continue;
                }
                RenderLevelTerrainSection(preview, section);
            }
        } else if (!preview.vertices.empty() && !preview.triangles.empty()) {
            RenderLevelTriangleList(
                preview,
                preview.vertices,
                preview.triangles,
                preview.terrainTextureId,
                preview.terrainTexture,
                1.0f,
                1.0f);
        }
    }

    RenderLevelModelsOpenGl(preview);
    RenderLevelWaterOpenGl(preview);
    RenderLevelObjectsOpenGl(preview);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    if (gGlUseProgram != nullptr) {
        gGlUseProgram(static_cast<GLuint>(previousProgram));
    }
}

void RenderLevelDrawCallback(const ImDrawList*, const ImDrawCmd* command) {
    auto* request = static_cast<PendingLevelRender*>(command->UserCallbackData);
    if (request == nullptr || request->preview == nullptr) {
        return;
    }
    RenderLevelOpenGl(*request->preview, request->canvasMin, request->canvasMax);
}

bool HasLevelTerrainTexture(const LevelPreview& preview) {
    if (preview.terrainTextureId != 0) {
        return true;
    }
    return std::any_of(preview.terrainSections.begin(), preview.terrainSections.end(), [](const LevelTerrainSection& section) {
        if (section.textureId != 0) {
            return true;
        }
        return std::any_of(section.layerTextureIds.begin(), section.layerTextureIds.end(), [](GLuint textureId) {
            return textureId != 0;
        });
    });
}

std::string LevelTerrainTextureSummary(const LevelPreview& preview) {
    if (!preview.terrainTexturePath.empty()) {
        return preview.terrainTexturePath;
    }

    int textureCount = 0;
    std::string firstTexture;
    int layerTextureCount = 0;
    std::string firstLayerTexture;
    for (const LevelTerrainSection& section : preview.terrainSections) {
        if (section.textureId != 0 && !section.texturePath.empty()) {
            ++textureCount;
            if (firstTexture.empty()) {
                firstTexture = section.texturePath;
            }
        }
        for (std::size_t index = 0; index < section.layerTextureIds.size(); ++index) {
            if (section.layerTextureIds[index] == 0 || section.layerTexturePaths[index].empty()) {
                continue;
            }
            ++layerTextureCount;
            if (firstLayerTexture.empty()) {
                firstLayerTexture = section.layerTexturePaths[index];
            }
        }
    }
    if (layerTextureCount > 0) {
        return std::to_string(layerTextureCount) + " Whirled terrain layer textures loaded. First: " + firstLayerTexture;
    }
    if (textureCount == 0) {
        return "No baked terrain texture loaded.";
    }
    if (textureCount == 1) {
        return firstTexture;
    }
    return std::to_string(textureCount) + " terrain textures loaded. First: " + firstTexture;
}

std::string JoinNames(const std::vector<std::string>& names, std::size_t maxNames) {
    std::string text;
    const std::size_t count = std::min(names.size(), maxNames);
    for (std::size_t index = 0; index < count; ++index) {
        if (!text.empty()) {
            text += ", ";
        }
        text += names[index];
    }
    if (names.size() > count) {
        text += ", ...";
    }
    return text;
}

Vec3 RotateX(Vec3 value, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {
        value.x,
        value.y * cosine - value.z * sine,
        value.y * sine + value.z * cosine,
    };
}

Vec3 RotateY(Vec3 value, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {
        value.x * cosine + value.z * sine,
        value.y,
        -value.x * sine + value.z * cosine,
    };
}

Vec3 ModelPointToView(const ModelPreview& preview, Vec3 point) {
    Vec3 view = Subtract(point, preview.center);
    view = RotateY(view, preview.yaw);
    view = RotateX(view, preview.pitch);
    return view;
}

ImVec2 ProjectModelPoint(
    const ModelPreview& preview,
    Vec3 point,
    ImVec2 canvasMin,
    ImVec2 canvasSize) {
    const float aspect = std::max(0.001f, canvasSize.x / std::max(1.0f, canvasSize.y));
    const float halfHeight = std::max(0.001f, preview.radius * 1.18f / std::max(preview.zoom, 0.001f));
    const float halfWidth = halfHeight * aspect;
    const Vec3 view = ModelPointToView(preview, point);
    const float normalizedX = std::clamp(view.x / halfWidth, -4.0f, 4.0f);
    const float normalizedY = std::clamp(view.y / halfHeight, -4.0f, 4.0f);
    return {
        canvasMin.x + (normalizedX * 0.5f + 0.5f) * canvasSize.x,
        canvasMin.y + (0.5f - normalizedY * 0.5f) * canvasSize.y,
    };
}

float DistancePointSegmentSquared(ImVec2 point, ImVec2 start, ImVec2 end) {
    const ImVec2 segment(end.x - start.x, end.y - start.y);
    const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
    if (lengthSquared <= 0.0001f) {
        const float dx = point.x - start.x;
        const float dy = point.y - start.y;
        return dx * dx + dy * dy;
    }

    const float t = std::clamp(
        ((point.x - start.x) * segment.x + (point.y - start.y) * segment.y) / lengthSquared,
        0.0f,
        1.0f);
    const ImVec2 closest(start.x + segment.x * t, start.y + segment.y * t);
    const float dx = point.x - closest.x;
    const float dy = point.y - closest.y;
    return dx * dx + dy * dy;
}

int PickModelBone(ModelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    if (!preview.showSkeleton || preview.skeleton.empty()) {
        return -1;
    }

    UpdateModelSkinning(preview);
    float bestDistanceSquared = 12.0f * 12.0f;
    int bestBone = -1;
    for (const SkeletonBone& bone : preview.skeleton) {
        const ImVec2 joint = ProjectModelPoint(preview, bone.worldPosition, canvasMin, canvasSize);
        const float jointDx = mouse.x - joint.x;
        const float jointDy = mouse.y - joint.y;
        const float jointDistanceSquared = jointDx * jointDx + jointDy * jointDy;
        if (jointDistanceSquared < bestDistanceSquared) {
            bestDistanceSquared = jointDistanceSquared;
            bestBone = bone.id;
        }

        if (bone.parent >= 0 && bone.parent < static_cast<int>(preview.skeleton.size())) {
            const ImVec2 parent = ProjectModelPoint(
                preview,
                preview.skeleton[bone.parent].worldPosition,
                canvasMin,
                canvasSize);
            const float segmentDistanceSquared = DistancePointSegmentSquared(mouse, parent, joint);
            if (segmentDistanceSquared < bestDistanceSquared) {
                bestDistanceSquared = segmentDistanceSquared;
                bestBone = bone.id;
            }
        }
    }

    return bestBone;
}

Vec3 CameraViewAxisModelSpace(const ModelPreview& preview) {
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    axis = RotateX(axis, -preview.pitch);
    axis = RotateY(axis, -preview.yaw);
    return Normalize(axis);
}

void ResetSelectedBonePose(ModelPreview& preview) {
    preview.rotatingBone = false;
    preview.boneEditAngle = 0.0f;
    UpdateModelSkinning(preview);
}

void DeselectModelBone(ModelPreview& preview) {
    preview.selectedBone = -1;
    ResetSelectedBonePose(preview);
}

void DrawModelRigControls(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        ImGui::BeginDisabled();
        bool showSkeleton = false;
        ImGui::Checkbox("Skeleton", &showSkeleton);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("No matching BSB skeleton was found for this model.");
        }
        return;
    }

    ImGui::Checkbox("Skeleton", &preview.showSkeleton);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%d bones", static_cast<int>(preview.skeleton.size()));
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%d bones", static_cast<int>(preview.skeleton.size()));
    if (preview.selectedBone >= 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("bone %d%s", preview.selectedBone, preview.rotatingBone ? " rotating" : "");
    }
    if (!preview.skeletonClips.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%d clips", static_cast<int>(preview.skeletonClips.size()));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", JoinNames(preview.skeletonClips, 12).c_str());
        }
    }
}

void DrawModelSectionControls(ModelPreview& preview) {
    if (preview.sections.size() <= 1) {
        return;
    }

    if (ImGui::SmallButton("All##ModelSections")) {
        for (ModelSection& section : preview.sections) {
            section.visible = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("None##ModelSections")) {
        for (ModelSection& section : preview.sections) {
            section.visible = false;
        }
    }
    ImGui::SameLine();

    ImGui::BeginChild(
        "ModelSectionControls",
        ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + 4.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    for (std::size_t index = 0; index < preview.sections.size(); ++index) {
        ModelSection& section = preview.sections[index];
        bool visible = section.visible;
        const std::string label =
            "M" + std::to_string(index + 1) + "##ModelSection" + std::to_string(index);
        if (ImGui::Checkbox(label.c_str(), &visible)) {
            section.visible = visible;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", section.name.c_str());
        }
        ImGui::SameLine();
    }
    ImGui::EndChild();
}

void DrawModelAnimationPanel(ModelPreview& preview, ImVec2 size) {
    ImGui::BeginChild(
        "ModelAnimationPanel",
        size,
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Animations");
    if (HasActiveModelAnimation(preview)) {
        const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
        const double duration = clip.fps > 0.0f
                                    ? static_cast<double>(clip.frameCount) / static_cast<double>(clip.fps)
                                    : 0.0;
        float progress = duration > 0.0 ? static_cast<float>(preview.animationTime / duration) : 0.0f;
        progress = std::clamp(progress, 0.0f, 1.0f);
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

        const char* playPauseIcon = preview.animationPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
        if (AudioIconButton(playPauseIcon, "ModelAnimationPlayPause", "Play / pause", ImVec2(36.0f, 0.0f))) {
            preview.animationPlaying = !preview.animationPlaying;
            preview.animationLastUpdateTime = ImGui::GetTime();
        }
        ImGui::SameLine();
        if (AudioIconButton(ICON_FA_STOP, "ModelAnimationStop", "Stop", ImVec2(36.0f, 0.0f))) {
            preview.animationPlaying = false;
            preview.animationTime = 0.0;
            preview.animationLastUpdateTime = ImGui::GetTime();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%d frames  %.0f fps",
            clip.frameCount,
            static_cast<double>(clip.fps));
    } else if (!preview.animations.empty()) {
        ImGui::TextDisabled("Select a clip");
    } else {
        ImGui::TextDisabled("No clips");
    }

    ImGui::Separator();
    ImGui::BeginChild("ModelAnimationList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(preview.animations.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            ModelAnimationClip& clip = preview.animations[index];
            const bool selected = index == preview.selectedAnimation;
            const std::string label =
                std::string(clip.loaded ? ICON_FA_PLAY : ICON_FA_FILE) + "  " +
                clip.name + "##ModelAnimation" + std::to_string(index);
            if (!clip.loaded) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable(label.c_str(), selected)) {
                PlayModelAnimation(preview, index);
            }
            if (!clip.loaded) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "%s%s%s",
                    clip.status.c_str(),
                    clip.path.empty() ? "" : "\n",
                    clip.path.c_str());
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawModelPreviewCanvas(AppState& state, ModelPreview& preview) {
    if (!preview.open) {
        return;
    }

    ImGui::BeginChild(
        "ModelPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    ImGui::InvisibleButton("##ModelPreviewInput", canvasSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();

    if (preview.selectedBone >= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        DeselectModelBone(preview);
        state.status = "Bone deselected.";
    }

    if (preview.selectedBone >= 0 && ImGui::IsKeyPressed(ImGuiKey_R)) {
        preview.rotatingBone = true;
        preview.boneRotationAxis = CameraViewAxisModelSpace(preview);
        preview.boneEditAngle = 0.0f;
        state.status = "Rotating bone " + std::to_string(preview.selectedBone);
    }

    if (preview.rotatingBone) {
        if (hovered && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            preview.boneEditAngle += (io.MouseDelta.x + io.MouseDelta.y * 0.35f) * 0.0125f;
            UpdateModelSkinning(preview);
        }
        if (hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
            ResetSelectedBonePose(preview);
            state.status = "Bone rotation reset.";
        }
    } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int pickedBone = PickModelBone(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedBone >= 0) {
            preview.selectedBone = pickedBone;
            preview.showSkeleton = true;
            preview.boneEditAngle = 0.0f;
            preview.rotatingBone = false;
            state.status = "Selected bone " + std::to_string(preview.selectedBone);
        } else if (preview.selectedBone >= 0) {
            DeselectModelBone(preview);
            state.status = "Bone deselected.";
        }
    } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && preview.selectedBone >= 0) {
        const int pickedBone = PickModelBone(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedBone < 0) {
            DeselectModelBone(preview);
            state.status = "Bone deselected.";
        }
    }

    if (!preview.rotatingBone && active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
            preview.yaw += io.MouseDelta.x * 0.010f;
            preview.pitch += io.MouseDelta.y * 0.010f;
            preview.pitch = std::clamp(preview.pitch, -1.45f, 1.45f);
        }
    }
    if (hovered && io.MouseWheel != 0.0f) {
        preview.zoom *= std::pow(1.12f, io.MouseWheel);
        preview.zoom = std::clamp(preview.zoom, 0.05f, 80.0f);
    }

    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 19, 20, 255));

    if (preview.vertices.empty() || preview.triangles.empty()) {
        const std::string message = preview.status.empty() ? "No drawable mesh." : preview.status;
        const ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
        drawList->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message.c_str());
        ImGui::EndChild();
        return;
    }

    gPendingModelRender.preview = &preview;
    gPendingModelRender.canvasMin = canvasMin;
    gPendingModelRender.canvasMax = canvasMax;
    drawList->AddCallback(RenderModelDrawCallback, &gPendingModelRender);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::EndChild();
}

void DrawModelPreview(AppState& state) {
    ModelPreview& preview = state.modelPreview;
    if (!preview.open) {
        return;
    }

    AdvanceModelAnimation(preview);

    if (ImGui::SmallButton("x##CloseModelPreview")) {
        DestroyModelRenderTexture(preview);
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.vertices.empty() || preview.triangles.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    DrawModelRigControls(preview);
    DrawModelSectionControls(preview);
    ImGui::Separator();

    const bool showAnimationPanel = !preview.skeleton.empty();
    if (!showAnimationPanel) {
        DrawModelPreviewCanvas(state, preview);
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = std::clamp(available.x * 0.22f, 220.0f, 320.0f);
    const float canvasWidth = std::max(120.0f, available.x - panelWidth - spacing);

    ImGui::BeginChild("ModelPreviewCanvasDock", ImVec2(canvasWidth, 0.0f), ImGuiChildFlags_None);
    DrawModelPreviewCanvas(state, preview);
    ImGui::EndChild();
    ImGui::SameLine();
    DrawModelAnimationPanel(preview, ImVec2(panelWidth, 0.0f));
}

void DrawLevelPreviewCanvas(AppState& state, LevelPreview& preview) {
    if (!preview.open) {
        return;
    }

    ImGui::BeginChild(
        "LevelPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    ImGui::InvisibleButton("##LevelPreviewInput", canvasSize);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();

    EnsureLevelFlyCamera(preview);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        POINT cursor = {};
        if (GetCursorPos(&cursor)) {
            preview.mouseLookLocked = true;
            preview.mouseLookLockX = cursor.x;
            preview.mouseLookLockY = cursor.y;
        }
    }
    if (preview.mouseLookLocked && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        preview.mouseLookLocked = false;
    }

    const bool rightMouseLook = preview.mouseLookLocked && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (rightMouseLook) {
        POINT cursor = {};
        if (GetCursorPos(&cursor)) {
            const float deltaX = static_cast<float>(cursor.x - preview.mouseLookLockX);
            const float deltaY = static_cast<float>(cursor.y - preview.mouseLookLockY);
            preview.yaw -= deltaX * 0.0045f;
            preview.pitch -= deltaY * 0.0045f;
            SetCursorPos(preview.mouseLookLockX, preview.mouseLookLockY);
        }
        preview.pitch = std::clamp(preview.pitch, -1.50f, 1.50f);
    }

    if (hovered && !rightMouseLook && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int pickedObject = PickLevelModelObject(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedObject >= 0) {
            preview.selectedObject = pickedObject;
            preview.scrollSelectedObjectIntoView = true;
            const WorldObject& object = preview.objects[static_cast<std::size_t>(pickedObject)];
            state.status = "Selected " + (object.name.empty() ? object.className : object.name);
        } else if (preview.selectedObject >= 0) {
            preview.selectedObject = -1;
            preview.scrollSelectedObjectIntoView = false;
            state.status = "Model selection cleared.";
        }
    }

    const bool flyActive = hovered || rightMouseLook;
    if (flyActive) {
        Vec3 move{};
        const Vec3 forward = LevelCameraForward(preview);
        const Vec3 right = LevelCameraRight(preview);
        const Vec3 up{0.0f, 1.0f, 0.0f};
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            move = Add(move, forward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            move = Subtract(move, forward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            move = Add(move, right);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            move = Subtract(move, right);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) {
            move = Add(move, up);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
            move = Subtract(move, up);
        }

        float speed = preview.flySpeed;
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
            speed *= 3.5f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt)) {
            speed *= 0.25f;
        }

        if (Length(move) > 0.0001f) {
            preview.flyCameraPosition = Add(
                preview.flyCameraPosition,
                Multiply(Normalize(move), speed * std::max(0.001f, io.DeltaTime)));
        }
    }

    if (rightMouseLook) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 19, 20, 255));

    if (preview.vertices.empty() && preview.objects.empty()) {
        const std::string message = preview.status.empty() ? "No level data loaded." : preview.status;
        const ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
        drawList->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message.c_str());
        ImGui::EndChild();
        return;
    }

    gPendingLevelRender.preview = &preview;
    gPendingLevelRender.canvasMin = canvasMin;
    gPendingLevelRender.canvasMax = canvasMax;
    drawList->AddCallback(RenderLevelDrawCallback, &gPendingLevelRender);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::EndChild();
}

std::string VisibleImGuiLabel(const std::string& label) {
    const std::size_t idSeparator = label.find("##");
    return idSeparator == std::string::npos ? label : label.substr(0, idSeparator);
}

void DrawWrappedCheckbox(const std::string& label,
                         bool* value,
                         bool enabled,
                         const std::string& tooltip,
                         bool& firstOnLine) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string visibleLabel = VisibleImGuiLabel(label);
    const float checkboxWidth =
        ImGui::GetFrameHeight() +
        style.ItemInnerSpacing.x +
        ImGui::CalcTextSize(visibleLabel.c_str()).x;

    if (!firstOnLine) {
        const float nextRight =
            ImGui::GetItemRectMax().x +
            style.ItemSpacing.x +
            checkboxWidth;
        const float contentRight = ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x;
        if (nextRight <= contentRight) {
            ImGui::SameLine();
        }
    }

    if (!enabled) {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox(label.c_str(), value);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", tooltip.c_str());
    }
    firstOnLine = false;
}

std::string LevelControlLabel(const char* visibleLabel, const char* suffix) {
    return std::string(visibleLabel) + "##Level" + suffix + visibleLabel;
}

void DrawLevelVisibilityControls(LevelPreview& preview, const char* suffix) {
    bool firstOnLine = true;
    DrawWrappedCheckbox(LevelControlLabel("Terrain", suffix), &preview.showTerrain, true, {}, firstOnLine);

    const bool hasTerrainTexture = HasLevelTerrainTexture(preview);
    DrawWrappedCheckbox(
        LevelControlLabel("Texture", suffix),
        &preview.showTerrainTexture,
        hasTerrainTexture,
        hasTerrainTexture ? LevelTerrainTextureSummary(preview) : "No terrain texture loaded.",
        firstOnLine);

    DrawWrappedCheckbox(LevelControlLabel("Objects", suffix), &preview.showObjects, true, {}, firstOnLine);
    if (preview.showObjects) {
        DrawWrappedCheckbox(LevelControlLabel("Checkpoints", suffix), &preview.showCheckpoints, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Start Position", suffix), &preview.showStartPositions, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Weapon Pickups", suffix), &preview.showWeaponPickups, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("AI Stuff", suffix), &preview.showAiObjects, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Markers", suffix), &preview.showObjectMarkers, true, {}, firstOnLine);
    }
    if (!preview.skyboxAssetIndices.empty()) {
        DrawWrappedCheckbox(LevelControlLabel("Skybox", suffix), &preview.showSkybox, true, {}, firstOnLine);
    }
    if (!preview.waterSheets.empty()) {
        DrawWrappedCheckbox(LevelControlLabel("Water", suffix), &preview.showWater, true, {}, firstOnLine);
    }
}

std::string LevelObjectVisibleLabel(const WorldObject& object) {
    std::string visibleLabel = object.className + "  " + object.name;
    if (!object.modelPath.empty()) {
        visibleLabel += "  ";
        visibleLabel += std::filesystem::path(object.modelPath).filename().string();
    }
    if (object.hasLayer) {
        visibleLabel += "  L " + FormatHex32(object.layer);
    }
    return visibleLabel;
}

std::string LevelTerrainSectionLabel(const LevelTerrainSection& section, int index) {
    std::filesystem::path terrainPath(section.path);
    std::string label = terrainPath.parent_path().filename().string();
    if (label.empty()) {
        label = terrainPath.stem().string();
    }
    if (label.empty()) {
        label = "Level " + std::to_string(index + 1);
    }
    if (section.hasLayer) {
        label += "  " + FormatHex32(section.layer);
    }
    return label;
}

void DrawLevelSublevelSelector(LevelPreview& preview) {
    if (preview.terrainSections.size() <= 1) {
        preview.selectedTerrainSection = -1;
        return;
    }

    if (!HasActiveTerrainSection(preview)) {
        preview.selectedTerrainSection = -1;
    }

    const std::string current =
        HasActiveTerrainSection(preview)
            ? LevelTerrainSectionLabel(
                  preview.terrainSections[static_cast<std::size_t>(preview.selectedTerrainSection)],
                  preview.selectedTerrainSection)
            : std::string("All levels");

    if (ImGui::BeginCombo("Level", current.c_str())) {
        const bool allSelected = preview.selectedTerrainSection < 0;
        if (ImGui::Selectable("All levels", allSelected)) {
            preview.selectedTerrainSection = -1;
            preview.selectedObject = -1;
            preview.scrollSelectedObjectIntoView = false;
            preview.flyCameraInitialized = false;
        }
        if (allSelected) {
            ImGui::SetItemDefaultFocus();
        }

        for (std::size_t index = 0; index < preview.terrainSections.size(); ++index) {
            const bool selected = preview.selectedTerrainSection == static_cast<int>(index);
            const std::string label = LevelTerrainSectionLabel(preview.terrainSections[index], static_cast<int>(index));
            if (ImGui::Selectable(label.c_str(), selected)) {
                preview.selectedTerrainSection = static_cast<int>(index);
                preview.selectedObject = -1;
                preview.scrollSelectedObjectIntoView = false;
                preview.flyCameraInitialized = false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void DrawLevelObjectPanel(LevelPreview& preview, ImVec2 size) {
    ImGui::BeginChild(
        "LevelObjectPanel",
        size,
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("World");
    DrawLevelSublevelSelector(preview);

    ImGui::Separator();
    ImGui::BeginChild("LevelObjectList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    std::vector<int> visibleObjects;
    visibleObjects.reserve(preview.objects.size());
    for (int index = 0; index < static_cast<int>(preview.objects.size()); ++index) {
        if (IsObjectInActiveSublevel(preview, preview.objects[static_cast<std::size_t>(index)])) {
            visibleObjects.push_back(index);
        }
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visibleObjects.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const int index = visibleObjects[static_cast<std::size_t>(row)];
            const WorldObject& object = preview.objects[index];
            const bool selected = index == preview.selectedObject;
            const std::string visibleLabel = LevelObjectVisibleLabel(object);
            const std::string label = visibleLabel + "##LevelObject" + std::to_string(index);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.03f, 0.46f, 0.13f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.04f, 0.58f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.03f, 0.68f, 0.20f, 1.0f));
            }
            if (ImGui::Selectable(label.c_str(), selected)) {
                preview.selectedObject = index;
                preview.scrollSelectedObjectIntoView = false;
            }
            if (selected) {
                ImGui::PopStyleColor(3);
                if (preview.scrollSelectedObjectIntoView) {
                    ImGui::SetScrollHereY(0.5f);
                    preview.scrollSelectedObjectIntoView = false;
                }
            }
            if (ImGui::IsItemHovered()) {
                std::string tooltip = object.name + "\n" + object.className;
                if (object.hasLayer) {
                    tooltip += "\nlayer " + FormatHex32(object.layer);
                }
                if (!object.binding.empty()) {
                    tooltip += "\nbinding " + object.binding;
                }
                if (object.recordSize > 0) {
                    tooltip += "\nrecord " + std::to_string(object.recordSize) + " bytes";
                }
                if (object.hasPosition) {
                    tooltip +=
                        "\nposition " +
                        std::to_string(object.position.x) + ", " +
                        std::to_string(object.position.y) + ", " +
                        std::to_string(object.position.z);
                }
                for (const std::string& assetPath : object.assetPaths) {
                    tooltip += "\n" + assetPath;
                }
                if (object.assetPaths.empty() && !object.assetPath.empty()) {
                    tooltip += "\n" + object.assetPath;
                }
                ImGui::SetTooltip("%s", tooltip.c_str());
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawLevelPreview(AppState& state) {
    LevelPreview& preview = state.levelPreview;
    if (!preview.open) {
        return;
    }

    DrawLevelVisibilityControls(preview, "Top");
    ImGui::Separator();
    if (preview.objects.empty()) {
        DrawLevelPreviewCanvas(state, preview);
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float maxPanelWidth = std::max(0.0f, available.x - 180.0f - spacing);
    if (maxPanelWidth < 220.0f) {
        DrawLevelPreviewCanvas(state, preview);
        return;
    }
    const float panelWidth = std::min(std::clamp(available.x * 0.24f, 240.0f, 360.0f), maxPanelWidth);
    const float canvasWidth = std::max(1.0f, available.x - panelWidth - spacing);

    ImGui::BeginChild("LevelPreviewCanvasDock", ImVec2(canvasWidth, 0.0f), ImGuiChildFlags_None);
    DrawLevelPreviewCanvas(state, preview);
    ImGui::EndChild();
    ImGui::SameLine();
    DrawLevelObjectPanel(preview, ImVec2(panelWidth, 0.0f));
}

void DrawRenderArea(AppState& state, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.058f, 0.060f, 1.0f));
    ImGui::BeginChild(
        "RenderArea",
        ImVec2(0.0f, height),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!state.archiveLoaded) {
        DrawEmptyState(state);
    } else if (state.audioPreview.open) {
        DrawAudioPreview(state);
    } else if (state.fxPreview.open) {
        DrawFxPreview(state);
    } else if (state.modelPreview.open) {
        DrawModelPreview(state);
    } else if (state.levelPreview.open) {
        DrawLevelPreview(state);
    } else if (state.textPreview.open) {
        DrawTextPreview(state);
    } else {
        DrawTexturePreview(state);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DrawExplorer(AppState& state) {
    ImVec2 available = ImGui::GetContentRegionAvail();
    constexpr float splitterHeight = 7.0f;
    state.bottomPanelHeight = std::clamp(state.bottomPanelHeight, 150.0f, std::max(150.0f, available.y * 0.70f));
    const float topHeight = std::max(80.0f, available.y - state.bottomPanelHeight - splitterHeight);

    DrawRenderArea(state, topHeight);
    DrawHorizontalSplitter(state, available.y);
    DrawBottomPanel(state);
}

void DrawFileDialogs(AppState& state) {
    if (ImGuiFileDialog::Instance()->Display(
            kChooseGtcDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            LoadArchive(state, ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile));
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display(
            kChooseDumpDirectoryDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outputFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            if (outputFolder.empty()) {
                outputFolder = ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile);
            }
            StartDump(state, outputFolder);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void DrawEmptyState(AppState& state) {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    constexpr ImVec2 buttonSize(240.0f, 46.0f);
    const float yOffset = std::max(0.0f, (available.y - buttonSize.y) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
    ImGui::SetCursorPosX((available.x - buttonSize.x) * 0.5f);

    if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + "  Load GAMEDATA.GTC").c_str(), buttonSize)) {
        OpenGtcDialog();
    }

    if (state.status.starts_with("Load failed:") || state.status.starts_with("Last GTC")) {
        const ImVec2 textSize = ImGui::CalcTextSize(state.status.c_str());
        ImGui::SetCursorPosX(std::max(0.0f, (available.x - textSize.x) * 0.5f));
        ImGui::TextDisabled("%s", state.status.c_str());
    }
}

void DrawToolbar(AppState& state) {
    ImGui::Text("%s  LEGO Racers 2 GTC Browser", ICON_FA_BOX_ARCHIVE);
    const bool dumpActive = IsDumpActive(state);
    const float browseWidth = 118.0f;
    const float dumpWidth = 122.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - browseWidth - dumpWidth - spacing);

    if (dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + "  Browse").c_str(), ImVec2(browseWidth, 0.0f))) {
        OpenGtcDialog();
    }
    if (dumpActive) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!state.archiveLoaded || dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button((std::string(ICON_FA_DOWNLOAD) + "  Dump All").c_str(), ImVec2(dumpWidth, 0.0f))) {
        OpenDumpDirectoryDialog(state);
    }
    if (!state.archiveLoaded || dumpActive) {
        ImGui::EndDisabled();
    }
    ImGui::Separator();
}

void DrawMainUi(AppState& state) {
    PollDumpTask(state);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::Begin("GTC Browser", nullptr, windowFlags);

    DrawExplorer(state);
    DrawFileDialogs(state);

    ImGui::End();
}

void ApplyAppStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.085f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.11f, 0.118f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.145f, 0.155f, 0.165f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.19f, 0.32f, 0.41f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.41f, 0.54f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.19f, 0.32f, 0.41f, 0.76f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.41f, 0.54f, 0.86f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.035f);
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFont* baseFont = nullptr;
    const std::filesystem::path segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (std::filesystem::exists(segoe)) {
        baseFont = io.Fonts->AddFontFromFileTTF(segoe.string().c_str(), 16.0f);
    }
    if (baseFont == nullptr) {
        baseFont = io.Fonts->AddFontDefault();
    }

    static constexpr ImWchar iconRanges[] = {0xf000, 0xf8ff, 0};
    if (std::filesystem::exists(FONT_AWESOME_SOLID_TTF)) {
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = 15.0f;
        io.Fonts->AddFontFromFileTTF(FONT_AWESOME_SOLID_TTF, 14.0f, &config, iconRanges);
    }
}

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

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int commandShow) {
    InitializeDebugLog();
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
            LogDebug(std::string("retired texture cleanup failed: ") + error.what());
        }
    }

    StopAudioPreview(state.audioPreview);
    DestroyModelRenderTextureImmediate(state.modelPreview);
    StopLevelPreview(state.levelPreview);
    DestroyFxPreviewTextureImmediate(state.fxPreview);
    DestroyAllPreviewTextures(state.texturePreview);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupOpenGl(hwnd);
    DestroyWindow(hwnd);
    UnregisterClassW(kWindowClassName, instance);

    return 0;
}
