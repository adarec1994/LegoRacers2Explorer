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
            preview.animations.push_back(std::move(clip));
        }
    }
}

void LoadModelPreviewSkeleton(AppState& state, ModelPreview& preview, const ArchiveNode& modelNode) {
    const std::size_t skeletonEntryIndex = ResolveModelSkeletonEntryIndex(state, modelNode);
    if (skeletonEntryIndex == static_cast<std::size_t>(-1)) {
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
        }
    } catch (const std::exception& error) {
        preview.status += "  skeleton failed";
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
            ++missing;
        }
    }

    if (!preview.materials.empty()) {
        preview.status += "  textures " + std::to_string(loaded) + "/" +
                          std::to_string(preview.materials.size()) + " loaded";
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
