std::string Lr2SafeStem(std::string value) {
    value = Trim(std::move(value));
    if (value.empty()) {
        value = "asset";
    }
    for (char& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 32 ||
            character == '<' ||
            character == '>' ||
            character == ':' ||
            character == '"' ||
            character == '/' ||
            character == '\\' ||
            character == '|' ||
            character == '?' ||
            character == '*' ||
            character == ';' ||
            character == ',') {
            character = '_';
        }
    }
    while (!value.empty() && (value.back() == '.' || value.back() == ' ')) {
        value.pop_back();
    }
    return value.empty() ? std::string("asset") : value;
}

struct LevelPreviewGuard {
    LevelPreview* level = nullptr;

    ~LevelPreviewGuard() {
        if (level != nullptr) {
            StopLevelPreview(*level);
        }
    }
};

std::string Lr2RelativePath(const std::filesystem::path& baseDirectory, const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path relative = std::filesystem::relative(path, baseDirectory, error);
    if (error || relative.empty()) {
        relative = path.filename();
    }
    std::string text = relative.generic_string();
    return text;
}

void WriteJsonVec3(std::ostream& out, Vec3 value) {
    out << "[" << value.x << "," << value.y << "," << value.z << "]";
}

void WriteJsonQuat(std::ostream& out, Quat value) {
    out << "[" << value.x << "," << value.y << "," << value.z << "," << value.w << "]";
}

void WriteJsonString(std::ostream& out, const std::string& value) {
    out << "\"" << JsonEscape(value) << "\"";
}

std::size_t ResolveLevelTextureEntryIndex(AppState& state, const std::string& texturePath) {
    std::size_t entryIndex = FindArchiveEntryIndex(state, texturePath);
    if (entryIndex != static_cast<std::size_t>(-1)) {
        return entryIndex;
    }
    return ResolveModelTextureEntryIndex(state, texturePath);
}

std::string ExportLr2TextureReference(AppState& state,
                                      const std::string& archivePath,
                                      const std::filesystem::path& lr2Directory,
                                      const std::filesystem::path& textureDirectory,
                                      std::unordered_map<std::string, std::string>& exportedTextures,
                                      int& textureCounter) {
    if (archivePath.empty()) {
        return {};
    }

    const std::string key = NormalizeArchivePath(archivePath);
    const auto found = exportedTextures.find(key);
    if (found != exportedTextures.end()) {
        return found->second;
    }

    const std::size_t entryIndex = ResolveLevelTextureEntryIndex(state, archivePath);
    if (entryIndex == static_cast<std::size_t>(-1)) {
        exportedTextures.emplace(key, std::string{});
        return {};
    }

    const gtc::FileEntry& entry = state.archive.entries[entryIndex];
    DecodedTexture texture = DecodeArchiveTextureEntryForWhirledRender(state, entryIndex);
    std::filesystem::create_directories(textureDirectory);
    const std::string stem = Lr2SafeStem(std::filesystem::path(entry.path).stem().string());
    const std::filesystem::path outputPath =
        textureDirectory / (stem + "_" + std::to_string(textureCounter++) + ".png");
    WriteWicImageFile(texture, outputPath, GUID_ContainerFormatPng);
    const std::string relative = Lr2RelativePath(lr2Directory, outputPath);
    exportedTextures.emplace(key, relative);
    return relative;
}

void WriteLr2Object(std::ostream& out, const WorldObject& object, int index) {
    out << "{";
    out << "\"index\":" << index << ",";
    out << "\"class\":";
    WriteJsonString(out, object.className);
    out << ",\"name\":";
    WriteJsonString(out, object.name);
    out << ",\"binding\":";
    WriteJsonString(out, object.binding);
    out << ",\"asset\":";
    WriteJsonString(out, object.assetPath);
    out << ",\"model\":";
    WriteJsonString(out, object.modelPath);
    out << ",\"layer\":" << object.layer << ",";
    out << "\"hasLayer\":" << (object.hasLayer ? "true" : "false") << ",";
    out << "\"hasPosition\":" << (object.hasPosition ? "true" : "false") << ",";
    out << "\"position\":";
    WriteJsonVec3(out, object.position);
    out << ",\"hasRotation\":" << (object.hasRotation ? "true" : "false") << ",";
    out << "\"rotation\":";
    WriteJsonQuat(out, object.rotation);
    out << ",\"hasScale\":" << (object.hasScale ? "true" : "false") << ",";
    out << "\"scale\":";
    WriteJsonVec3(out, object.scale);
    out << ",\"paths\":[";
    for (std::size_t pathIndex = 0; pathIndex < object.assetPaths.size(); ++pathIndex) {
        if (pathIndex != 0) {
            out << ",";
        }
        WriteJsonString(out, object.assetPaths[pathIndex]);
    }
    out << "]}";
}

void WriteLr2TerrainSection(std::ostream& out,
                            AppState& state,
                            const LevelTerrainSection& section,
                            int index,
                            const std::filesystem::path& lr2Directory,
                            const std::filesystem::path& textureDirectory,
                            std::unordered_map<std::string, std::string>& exportedTextures,
                            int& textureCounter) {
    out << "{";
    out << "\"index\":" << index << ",";
    out << "\"path\":";
    WriteJsonString(out, section.path);
    out << ",\"layer\":" << section.layer << ",";
    out << "\"hasLayer\":" << (section.hasLayer ? "true" : "false") << ",";
    out << "\"position\":";
    WriteJsonVec3(out, section.position);
    out << ",\"scale\":";
    WriteJsonVec3(out, section.scale);
    out << ",\"textureScale\":[" << section.textureScaleX << "," << section.textureScaleY << "],";
    out << "\"texture\":{";
    out << "\"archivePath\":";
    WriteJsonString(out, section.texturePath);
    out << ",\"path\":";
    WriteJsonString(out, ExportLr2TextureReference(
        state,
        section.texturePath,
        lr2Directory,
        textureDirectory,
        exportedTextures,
        textureCounter));
    out << "},\"layerTextures\":[";
    bool wroteTexture = false;
    for (std::size_t textureIndex = 0; textureIndex < section.layerTexturePaths.size(); ++textureIndex) {
        const std::string& archivePath = section.layerTexturePaths[textureIndex];
        if (archivePath.empty()) {
            continue;
        }
        if (wroteTexture) {
            out << ",";
        }
        wroteTexture = true;
        out << "{\"index\":" << textureIndex << ",\"archivePath\":";
        WriteJsonString(out, archivePath);
        out << ",\"path\":";
        WriteJsonString(out, ExportLr2TextureReference(
            state,
            archivePath,
            lr2Directory,
            textureDirectory,
            exportedTextures,
            textureCounter));
        out << "}";
    }
    out << "],\"vertices\":[";
    for (std::size_t vertexIndex = 0; vertexIndex < section.vertices.size(); ++vertexIndex) {
        if (vertexIndex != 0) {
            out << ",";
        }
        const LevelVertex& vertex = section.vertices[vertexIndex];
        out << "{\"position\":";
        WriteJsonVec3(out, vertex.position);
        out << ",\"normal\":";
        WriteJsonVec3(out, vertex.normal);
        out << ",\"uv\":[" << vertex.u << "," << vertex.v << "]";
        out << ",\"mix\":[" <<
            static_cast<int>(vertex.mix[0]) << "," <<
            static_cast<int>(vertex.mix[1]) << "," <<
            static_cast<int>(vertex.mix[2]) << "," <<
            static_cast<int>(vertex.mix[3]) << "]}";
    }
    out << "],\"triangles\":[";
    for (std::size_t triangleIndex = 0; triangleIndex < section.triangles.size(); ++triangleIndex) {
        if (triangleIndex != 0) {
            out << ",";
        }
        const LevelTriangle& triangle = section.triangles[triangleIndex];
        out << "{\"indices\":[" << triangle.a << "," << triangle.b << "," << triangle.c << "]";
        out << ",\"materialKey\":" << triangle.materialKey << "}";
    }
    out << "]}";
}

void WriteLr2WaterSheet(std::ostream& out,
                        AppState& state,
                        const LevelWaterSheet& waterSheet,
                        int index,
                        const std::filesystem::path& lr2Directory,
                        const std::filesystem::path& textureDirectory,
                        std::unordered_map<std::string, std::string>& exportedTextures,
                        int& textureCounter) {
    out << "{";
    out << "\"index\":" << index << ",";
    out << "\"object\":" << waterSheet.objectIndex << ",";
    out << "\"position\":";
    WriteJsonVec3(out, waterSheet.position);
    out << ",\"rotation\":";
    WriteJsonQuat(out, waterSheet.rotation);
    out << ",\"width\":" << waterSheet.width << ",";
    out << "\"depth\":" << waterSheet.depth << ",";
    out << "\"uScale\":" << waterSheet.uScale << ",";
    out << "\"vScale\":" << waterSheet.vScale << ",";
    out << "\"alpha\":" << waterSheet.alpha << ",";
    out << "\"texture\":{";
    out << "\"archivePath\":";
    WriteJsonString(out, waterSheet.texturePath);
    out << ",\"path\":";
    WriteJsonString(out, ExportLr2TextureReference(
        state,
        waterSheet.texturePath,
        lr2Directory,
        textureDirectory,
        exportedTextures,
        textureCounter));
    out << "},\"reflectionTexture\":{";
    out << "\"archivePath\":";
    WriteJsonString(out, waterSheet.reflectionTexturePath);
    out << ",\"path\":";
    WriteJsonString(out, ExportLr2TextureReference(
        state,
        waterSheet.reflectionTexturePath,
        lr2Directory,
        textureDirectory,
        exportedTextures,
        textureCounter));
    out << "}}";
}

void ExportLevelNode(AppState& state, const ArchiveNode& node, const std::filesystem::path& requestedPath) {
    if (!IsLevelNode(node) || NodeExtensionLower(node) != ".wrl") {
        throw std::runtime_error("Selected file is not a WRL saved world.");
    }

    const std::filesystem::path lr2Path = EnsureExtension(requestedPath, ".lr2");
    const std::filesystem::path lr2Directory =
        lr2Path.has_parent_path() ? lr2Path.parent_path() : std::filesystem::current_path();
    const std::filesystem::path assetDirectory = lr2Directory / (lr2Path.stem().string() + "_assets");
    const std::filesystem::path modelDirectory = assetDirectory / "models";
    const std::filesystem::path textureDirectory = assetDirectory / "textures";

    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    LevelPreview level = DecodeWrlWorld(bytes, node.name, node.path);
    LevelPreviewGuard levelGuard{&level};
    TryLoadWorldTerrain(state, level);
    TryLoadWorldModels(state, level);
    TryLoadWorldWater(state, level);

    std::filesystem::create_directories(modelDirectory);
    std::vector<std::string> modelPaths(level.modelAssets.size());
    for (std::size_t assetIndex = 0; assetIndex < level.modelAssets.size(); ++assetIndex) {
        const LevelModelAsset& asset = level.modelAssets[assetIndex];
        if (!asset.loaded || asset.model.vertices.empty()) {
            continue;
        }
        const std::string stem = Lr2SafeStem(std::filesystem::path(asset.path).stem().string());
        const std::filesystem::path modelPath =
            modelDirectory / (stem + "_" + std::to_string(assetIndex) + ".glb");
        ExportModelGlb(asset.model, modelPath);
        modelPaths[assetIndex] = Lr2RelativePath(lr2Directory, modelPath);
    }

    std::unordered_map<std::string, std::string> exportedTextures;
    int textureCounter = 0;

    std::ofstream out(lr2Path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Could not open LR2 export file: " + lr2Path.string());
    }
    out << std::setprecision(9);
    out << "{";
    out << "\"format\":\"lr2-level\",";
    out << "\"version\":1,";
    out << "\"source\":{";
    out << "\"name\":";
    WriteJsonString(out, node.name);
    out << ",\"path\":";
    WriteJsonString(out, node.path);
    out << "},";
    out << "\"bounds\":{\"min\":";
    WriteJsonVec3(out, level.boundsMin);
    out << ",\"max\":";
    WriteJsonVec3(out, level.boundsMax);
    out << ",\"center\":";
    WriteJsonVec3(out, level.center);
    out << ",\"radius\":" << level.radius << "},";
    out << "\"models\":[";
    for (std::size_t assetIndex = 0; assetIndex < level.modelAssets.size(); ++assetIndex) {
        if (assetIndex != 0) {
            out << ",";
        }
        const LevelModelAsset& asset = level.modelAssets[assetIndex];
        out << "{\"id\":" << assetIndex << ",";
        out << "\"name\":";
        WriteJsonString(out, std::filesystem::path(asset.path).filename().string());
        out << ",\"archivePath\":";
        WriteJsonString(out, asset.path);
        out << ",\"path\":";
        WriteJsonString(out, modelPaths[assetIndex]);
        out << ",\"loaded\":" << (asset.loaded ? "true" : "false") << "}";
    }
    out << "],\"instances\":[";
    for (std::size_t instanceIndex = 0; instanceIndex < level.modelInstances.size(); ++instanceIndex) {
        if (instanceIndex != 0) {
            out << ",";
        }
        const LevelModelInstance& instance = level.modelInstances[instanceIndex];
        const WorldObject* object =
            instance.objectIndex >= 0 && instance.objectIndex < static_cast<int>(level.objects.size())
                ? &level.objects[static_cast<std::size_t>(instance.objectIndex)]
                : nullptr;
        out << "{\"id\":" << instanceIndex << ",";
        out << "\"object\":" << instance.objectIndex << ",";
        out << "\"model\":" << instance.assetIndex << ",";
        out << "\"name\":";
        WriteJsonString(out, object != nullptr ? object->name : std::string{});
        out << ",\"class\":";
        WriteJsonString(out, object != nullptr ? object->className : std::string{});
        out << ",\"position\":";
        WriteJsonVec3(out, instance.position);
        out << ",\"rotation\":";
        WriteJsonQuat(out, instance.rotation);
        out << ",\"scale\":";
        WriteJsonVec3(out, instance.scale);
        out << "}";
    }
    out << "],\"terrain\":[";
    for (std::size_t sectionIndex = 0; sectionIndex < level.terrainSections.size(); ++sectionIndex) {
        if (sectionIndex != 0) {
            out << ",";
        }
        WriteLr2TerrainSection(
            out,
            state,
            level.terrainSections[sectionIndex],
            static_cast<int>(sectionIndex),
            lr2Directory,
            textureDirectory,
            exportedTextures,
            textureCounter);
    }
    out << "],\"water\":[";
    for (std::size_t waterIndex = 0; waterIndex < level.waterSheets.size(); ++waterIndex) {
        if (waterIndex != 0) {
            out << ",";
        }
        WriteLr2WaterSheet(
            out,
            state,
            level.waterSheets[waterIndex],
            static_cast<int>(waterIndex),
            lr2Directory,
            textureDirectory,
            exportedTextures,
            textureCounter);
    }
    out << "],\"objects\":[";
    for (std::size_t objectIndex = 0; objectIndex < level.objects.size(); ++objectIndex) {
        if (objectIndex != 0) {
            out << ",";
        }
        WriteLr2Object(out, level.objects[objectIndex], static_cast<int>(objectIndex));
    }
    out << "]}";
    if (!out) {
        throw std::runtime_error("Could not write LR2 export file: " + lr2Path.string());
    }
}
