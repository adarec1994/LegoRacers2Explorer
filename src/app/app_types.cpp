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

struct ExportProgress {
    std::size_t stepsDone = 0;
    std::size_t totalSteps = 0;
    std::string currentPath;
    std::string message;
};

using ExportProgressCallback = std::function<void(const ExportProgress&)>;

struct ExportSnapshot {
    bool active = false;
    bool finished = false;
    bool succeeded = false;
    std::size_t stepsDone = 0;
    std::size_t totalSteps = 0;
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
    bool generatedHeightmap = false;
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
    SkinnedModels,
    Levels,
    Fx,
    Audio,
};

enum class ExportKind {
    None,
    TexturePng,
    TextureTiff,
    TextureDds,
    ModelGlb,
    ModelFbx,
    LevelLr2,
    HeightmapPng,
    HeightmapTiff,
    HeightmapDds,
    AudioWav,
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
    std::mutex exportMutex;
    bool exportActive = false;
    bool exportFinished = false;
    bool exportSucceeded = false;
    std::size_t exportStepsDone = 0;
    std::size_t exportTotalSteps = 0;
    std::string exportCurrentPath;
    std::string exportMessage;
    std::future<void> exportFuture;
    std::array<char, 256> searchText = {};
    AssetFilter assetFilter = AssetFilter::All;
    ExportKind pendingExportKind = ExportKind::None;
    int pendingExportNode = -1;
    int pendingExportTerrainSection = -1;
    bool pendingExportPreviewTexture = false;
    float bottomPanelHeight = 260.0f;
    float fontSize = 16.0f;
    bool aboutOpen = false;
    bool aboutImageLoadAttempted = false;
    GLuint aboutTextureId = 0;
    int aboutImageWidth = 0;
    int aboutImageHeight = 0;
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
