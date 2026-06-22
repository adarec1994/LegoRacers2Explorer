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

bool IsSkinnedModelNode(const ArchiveNode& node) {
    if (!IsModelNode(node)) {
        return false;
    }

    static constexpr std::array<const char*, 21> paths = {{
        "game data\\aliens\\alien bodies\\models\\alien body.md2",
        "game data\\animation\\albatros\\albatros.md2",
        "game data\\animation\\beige_beast\\beige.md2",
        "game data\\animation\\bluemech\\bluemech.md2",
        "game data\\animation\\frog_walker\\frog_walker.md2",
        "game data\\animation\\hopper\\hopper.md2",
        "game data\\animation\\polar_bear\\polar_bear.md2",
        "game data\\animation\\ptera\\ptera.md2",
        "game data\\animation\\red_mech\\redmech.md2",
        "game data\\animation\\shieldgenerator\\shieldgenerator.md2",
        "game data\\animation\\stegosaurus\\steg1.md2",
        "game data\\animation\\tentacle\\tentacle.md2",
        "game data\\animation\\theberg\\anm\\theberg.md2",
        "game data\\animation\\theberg\\theberg.md2",
        "game data\\animation\\t-rex\\t-rex.md2",
        "game data\\animation\\triceratops\\triceratops.md2",
        "game data\\animation\\trophy\\trophy.md2",
        "game data\\animation\\tyreman\\tyreman.md2",
        "game data\\animation\\working_crane\\working_crane.md2",
        "game data\\characters\\bodies\\models\\minifig body.md2",
        "game data\\ramas\\rama bodies\\models\\rama body.md2",
    }};
    const std::string normalized = NormalizeArchivePath(node.path);
    return std::find(paths.begin(), paths.end(), normalized) != paths.end();
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
    case AssetFilter::SkinnedModels:
        return "Skinned Models";
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

bool NodeMatchesAssetFilter(AppState& state, const ArchiveNode& node, AssetFilter filter) {
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
    case AssetFilter::SkinnedModels:
        return IsSkinnedModelNode(node);
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
