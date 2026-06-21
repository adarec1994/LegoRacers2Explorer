std::string TerrainTargetLabel(const std::string& path, bool hasLayer, std::uint32_t layer, int index) {
    std::filesystem::path terrainPath(path);
    std::string label = terrainPath.parent_path().filename().string();
    if (label.empty()) {
        label = terrainPath.stem().string();
    }
    if (label.empty()) {
        label = "Level " + std::to_string(index + 1);
    }
    if (hasLayer) {
        label += "  " + FormatHex32(layer);
    }
    return label;
}

std::string TerrainSectionTargetLabel(const LevelTerrainSection& section, int index) {
    return TerrainTargetLabel(section.path, section.hasLayer, section.layer, index);
}

std::string TerrainLinkTargetLabel(const WrlTerrainLink& link, int index) {
    return TerrainTargetLabel(link.path, link.hasLayer, link.layer, index);
}

DecodedTexture BuildHeightmapTexture(const LevelPreview& preview, int terrainSectionIndex = -1) {
    std::vector<const LevelVertex*> vertices;
    if (terrainSectionIndex >= 0) {
        if (terrainSectionIndex >= static_cast<int>(preview.terrainSections.size())) {
            throw std::runtime_error("Requested WRL terrain section was not loaded.");
        }
        const LevelTerrainSection& section = preview.terrainSections[static_cast<std::size_t>(terrainSectionIndex)];
        for (const LevelVertex& vertex : section.vertices) {
            vertices.push_back(&vertex);
        }
    } else if (!preview.terrainSections.empty()) {
        for (const LevelTerrainSection& section : preview.terrainSections) {
            for (const LevelVertex& vertex : section.vertices) {
                vertices.push_back(&vertex);
            }
        }
    } else {
        for (const LevelVertex& vertex : preview.vertices) {
            vertices.push_back(&vertex);
        }
    }
    if (vertices.empty()) {
        throw std::runtime_error("WRL has no loaded terrain height data.");
    }

    Vec3 minValue{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 maxValue{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    for (const LevelVertex* vertex : vertices) {
        minValue.x = std::min(minValue.x, vertex->position.x);
        minValue.y = std::min(minValue.y, vertex->position.y);
        minValue.z = std::min(minValue.z, vertex->position.z);
        maxValue.x = std::max(maxValue.x, vertex->position.x);
        maxValue.y = std::max(maxValue.y, vertex->position.y);
        maxValue.z = std::max(maxValue.z, vertex->position.z);
    }

    constexpr int size = 513;
    DecodedTexture texture;
    texture.width = size;
    texture.height = size;
    texture.bitsPerPixel = 32;
    texture.mipLevels = 1;
    texture.rgba.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4U, 0);

    const float width = std::max(0.0001f, maxValue.x - minValue.x);
    const float depth = std::max(0.0001f, maxValue.z - minValue.z);
    const float height = std::max(0.0001f, maxValue.y - minValue.y);
    for (const LevelVertex* vertex : vertices) {
        const int x = std::clamp(static_cast<int>(((vertex->position.x - minValue.x) / width) * (size - 1)), 0, size - 1);
        const int y = std::clamp(static_cast<int>(((vertex->position.z - minValue.z) / depth) * (size - 1)), 0, size - 1);
        const auto shade = static_cast<unsigned char>(
            std::clamp(((vertex->position.y - minValue.y) / height) * 255.0f, 0.0f, 255.0f));
        const std::size_t pixel = (static_cast<std::size_t>(size - 1 - y) * size + static_cast<std::size_t>(x)) * 4U;
        texture.rgba[pixel + 0] = shade;
        texture.rgba[pixel + 1] = shade;
        texture.rgba[pixel + 2] = shade;
        texture.rgba[pixel + 3] = 255;
    }
    return texture;
}

std::vector<std::string> LoadWrlHeightmapTargetLabels(AppState& state, const ArchiveNode& node) {
    if (!IsLevelNode(node) || NodeExtensionLower(node) != ".wrl") {
        return {};
    }

    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    LevelPreview level = DecodeWrlWorld(bytes, node.name, node.path);
    std::vector<std::string> labels;
    labels.reserve(level.terrainLinks.size());
    for (std::size_t index = 0; index < level.terrainLinks.size(); ++index) {
        labels.push_back(TerrainLinkTargetLabel(level.terrainLinks[index], static_cast<int>(index)));
    }
    return labels;
}

void OpenWrlHeightmapPreview(AppState& state, const ArchiveNode& node, int terrainSectionIndex = -1) {
    if (!IsLevelNode(node) || NodeExtensionLower(node) != ".wrl") {
        throw std::runtime_error("Selected file is not a WRL saved world.");
    }

    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    LevelPreview level = DecodeWrlWorld(bytes, node.name, node.path);
    TryLoadWorldTerrainForExport(state, level);
    DecodedTexture heightmap = BuildHeightmapTexture(level, terrainSectionIndex);
    std::string targetName;
    if (terrainSectionIndex >= 0 &&
        terrainSectionIndex < static_cast<int>(level.terrainSections.size())) {
        targetName = TerrainSectionTargetLabel(
            level.terrainSections[static_cast<std::size_t>(terrainSectionIndex)],
            terrainSectionIndex);
    }

    StopAudioPreview(state.audioPreview);
    StopFxPreview(state.fxPreview);
    StopLevelPreview(state.levelPreview);
    DestroyModelRenderTexture(state.modelPreview);
    state.modelPreview.open = false;
    state.textPreview = {};
    DestroyPreviewTexture(state.texturePreview);

    TexturePreview preview;
    preview.open = true;
    preview.generatedHeightmap = true;
    preview.name = std::filesystem::path(node.name).stem().string();
    if (!targetName.empty()) {
        preview.name += " " + targetName;
    }
    preview.name += " heightmap";
    preview.path = node.path;
    preview.status = "Generated WRL heightmap from terrain vertices.";
    preview.decoded = std::move(heightmap);
    state.texturePreview = std::move(preview);
    UploadPreviewTexture(state.texturePreview);
    state.status = targetName.empty()
                       ? "Viewing heightmap for " + node.path
                       : "Viewing heightmap for " + node.path + " / " + targetName;
    StopLevelPreview(level);
}

void ExportHeightmapNode(AppState& state,
                         const ArchiveNode& node,
                         const std::filesystem::path& requestedPath,
                         ExportKind kind,
                         int terrainSectionIndex = -1) {
    if (!IsLevelNode(node) || NodeExtensionLower(node) != ".wrl") {
        throw std::runtime_error("Selected file is not a WRL saved world.");
    }

    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    LevelPreview level = DecodeWrlWorld(bytes, node.name, node.path);
    TryLoadWorldTerrainForExport(state, level);
    DecodedTexture heightmap = BuildHeightmapTexture(level, terrainSectionIndex);
    ExportDecodedTextureFile(heightmap, requestedPath, kind);
    StopLevelPreview(level);
}
