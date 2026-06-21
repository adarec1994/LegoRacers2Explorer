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
        return true;
    } catch (const std::exception& error) {
        preview.status = std::string("FX preview failed: ") + error.what();
        state.fxPreview = std::move(preview);
        state.status = state.fxPreview.status;
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
    } catch (const std::exception& error) {
        preview.status = std::string("Text preview failed: ") + error.what();
        state.textPreview = std::move(preview);
        state.status = state.textPreview.status;
    }
}

void SelectFolder(AppState& state, int nodeIndex);

void OpenTexturePreview(AppState& state, const ArchiveNode& node) {
    if (!IsTextureNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

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
    preview.generatedHeightmap = false;
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
    } catch (const std::exception& error) {
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
    } catch (const std::exception& error) {
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
    } catch (const std::exception& error) {
        StopAudioPlayback(preview);
        preview.status = std::string("Audio preview failed: ") + error.what();
        state.status = preview.status;
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
        const std::size_t terrainEntryIndex = FindArchiveEntryIndex(state, terrainCandidate);
        if (terrainEntryIndex == static_cast<std::size_t>(-1)) {
            ++missingSections;
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
        } catch (const std::exception& error) {
            ++missingSections;
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
}

void OpenLevelPreview(AppState& state, const ArchiveNode& node) {
    if (!IsLevelNode(node) || node.entryIndex >= state.archive.entries.size()) {
        ShowUnsupportedPreview(state, node);
        return;
    }

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
    } catch (const std::exception& error) {
        preview.status = std::string("Level preview failed: ") + error.what();
        state.levelPreview = std::move(preview);
        state.status = state.levelPreview.status;
    } catch (...) {
        preview.status = "Level preview failed: unknown exception.";
        state.levelPreview = std::move(preview);
        state.status = state.levelPreview.status;
    }
}
