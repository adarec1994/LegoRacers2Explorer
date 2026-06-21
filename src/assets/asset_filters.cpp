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
