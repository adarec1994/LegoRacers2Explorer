void LoadTerrainSectionTextureReferencesForExport(AppState& state, LevelTerrainSection& section) {
    int whirledTextureCount = 0;
    for (const unsigned char textureIndex : CollectTerrainTextureIndices(section)) {
        const std::size_t layerEntryIndex = ResolveTerrainLayerTextureEntryIndex(state, section.path, textureIndex);
        if (layerEntryIndex == static_cast<std::size_t>(-1)) {
            continue;
        }
        section.layerTexturePaths[textureIndex] = state.archive.entries[layerEntryIndex].path;
        ++whirledTextureCount;
    }

    if (whirledTextureCount > 0) {
        return;
    }

    const std::size_t textureEntryIndex = ResolveTerrainTextureEntryIndex(state, section.path);
    if (textureEntryIndex != static_cast<std::size_t>(-1)) {
        section.texturePath = state.archive.entries[textureEntryIndex].path;
    }
}

void TryLoadWorldTerrainForExport(AppState& state, LevelPreview& preview, const ExportProgressCallback& progress = {}) {
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
    for (std::size_t linkIndex = 0; linkIndex < terrainLinks.size(); ++linkIndex) {
        const WrlTerrainLink& link = terrainLinks[linkIndex];
        if (progress) {
            progress({
                linkIndex,
                terrainLinks.size(),
                link.path,
                "Loading terrain",
            });
        }

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
            LoadTerrainSectionTextureReferencesForExport(state, preview.terrainSections.back());
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

void TryLoadWorldModelsForExport(AppState& state, LevelPreview& preview, const ExportProgressCallback& progress = {}) {
    std::unordered_map<std::string, int> assetLookup;
    int missingModels = 0;
    int failedModels = 0;
    std::size_t modelLoadIndex = 0;

    auto loadModelAsset = [&](const std::string& modelPath) -> int {
        const std::string lookupKey = NormalizeArchivePath(modelPath);
        const auto found = assetLookup.find(lookupKey);
        if (found != assetLookup.end()) {
            return found->second;
        }

        if (progress) {
            progress({
                modelLoadIndex++,
                std::max<std::size_t>(1, preview.objects.size()),
                modelPath,
                "Loading models",
            });
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
            ArchiveNode modelNode;
            modelNode.name = asset.model.name;
            modelNode.path = asset.path;
            modelNode.directory = false;
            modelNode.entryIndex = entryIndex;
            LoadModelPreviewSkeleton(state, asset.model, modelNode);
            LoadModelTexturesForExport(state, asset.model);
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
