const char* ExportDialogTitle(ExportKind kind) {
    switch (kind) {
    case ExportKind::TexturePng:
        return "Export PNG";
    case ExportKind::TextureTiff:
        return "Export TIFF";
    case ExportKind::TextureDds:
        return "Export DDS";
    case ExportKind::ModelGlb:
        return "Export GLB";
    case ExportKind::ModelFbx:
        return "Export FBX";
    case ExportKind::LevelLr2:
        return "Export Level";
    case ExportKind::HeightmapPng:
        return "Export Heightmap PNG";
    case ExportKind::HeightmapTiff:
        return "Export Heightmap TIFF";
    case ExportKind::HeightmapDds:
        return "Export Heightmap DDS";
    case ExportKind::AudioWav:
        return "Export WAV";
    default:
        return "Export";
    }
}

const char* ExportDialogFilter(ExportKind kind) {
    switch (kind) {
    case ExportKind::TexturePng:
    case ExportKind::HeightmapPng:
        return ".png";
    case ExportKind::TextureTiff:
    case ExportKind::HeightmapTiff:
        return ".tiff";
    case ExportKind::TextureDds:
    case ExportKind::HeightmapDds:
        return ".dds";
    case ExportKind::ModelGlb:
        return ".glb";
    case ExportKind::ModelFbx:
        return ".fbx";
    case ExportKind::LevelLr2:
        return ".lr2";
    case ExportKind::AudioWav:
        return ".wav";
    default:
        return ".*";
    }
}

std::string ExportDefaultFileName(const ArchiveNode& node, ExportKind kind) {
    std::string extension = ExportDialogFilter(kind);
    if (extension.empty() || extension == ".*") {
        extension = ".dat";
    }
    return ExportBaseName(node) + extension;
}

std::string HeightmapExportDefaultFileName(const std::string& baseName, ExportKind kind) {
    std::string extension = ExportDialogFilter(kind);
    if (extension.empty() || extension == ".*") {
        extension = ".png";
    }
    std::string stem = baseName;
    if (stem.empty()) {
        stem = "heightmap";
    }
    if (ToLower(stem).find("heightmap") == std::string::npos) {
        stem += "_heightmap";
    }
    return stem + extension;
}

std::string SanitizeFileNameStem(std::string stem) {
    for (char& character : stem) {
        const auto value = static_cast<unsigned char>(character);
        if (value < 32 ||
            character == '<' ||
            character == '>' ||
            character == ':' ||
            character == '"' ||
            character == '/' ||
            character == '\\' ||
            character == '|' ||
            character == '?' ||
            character == '*') {
            character = '_';
        }
    }
    stem = Trim(std::move(stem));
    while (!stem.empty() && (stem.back() == '.' || stem.back() == ' ')) {
        stem.pop_back();
    }
    return stem.empty() ? std::string("export") : stem;
}

void OpenExportDialog(AppState& state, int nodeIndex, ExportKind kind) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(state.browser.nodes.size())) {
        return;
    }

    const ArchiveNode& node = state.browser.nodes[nodeIndex];
    state.pendingExportKind = kind;
    state.pendingExportNode = nodeIndex;
    state.pendingExportTerrainSection = -1;
    state.pendingExportPreviewTexture = false;

    IGFD::FileDialogConfig config;
    config.path = state.archiveLoaded && state.archive.gtcPath.has_parent_path()
                      ? state.archive.gtcPath.parent_path().string()
                      : PreferredInitialDirectory();
    config.fileName = IsImageExportKind(kind) && (kind == ExportKind::HeightmapPng ||
                                                  kind == ExportKind::HeightmapTiff ||
                                                  kind == ExportKind::HeightmapDds)
                          ? HeightmapExportDefaultFileName(ExportBaseName(node), kind)
                          : ExportDefaultFileName(node, kind);
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ConfirmOverwrite |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kExportFileDialogKey,
        ExportDialogTitle(kind),
        ExportDialogFilter(kind),
        config);
}

void OpenHeightmapExportDialog(AppState& state,
                               int nodeIndex,
                               ExportKind kind,
                               int terrainSectionIndex,
                               const std::string& targetLabel) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(state.browser.nodes.size())) {
        return;
    }

    const ArchiveNode& node = state.browser.nodes[nodeIndex];
    std::string baseName = ExportBaseName(node);
    if (!targetLabel.empty()) {
        baseName += "_" + SanitizeFileNameStem(targetLabel);
    }

    state.pendingExportKind = kind;
    state.pendingExportNode = nodeIndex;
    state.pendingExportTerrainSection = terrainSectionIndex;
    state.pendingExportPreviewTexture = false;

    IGFD::FileDialogConfig config;
    config.path = state.archiveLoaded && state.archive.gtcPath.has_parent_path()
                      ? state.archive.gtcPath.parent_path().string()
                      : PreferredInitialDirectory();
    config.fileName = HeightmapExportDefaultFileName(baseName, kind);
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ConfirmOverwrite |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kExportFileDialogKey,
        ExportDialogTitle(kind),
        ExportDialogFilter(kind),
        config);
}

void OpenPreviewTextureExportDialog(AppState& state, ExportKind kind) {
    if (!state.texturePreview.open ||
        !state.texturePreview.generatedHeightmap ||
        state.texturePreview.decoded.rgba.empty()) {
        return;
    }

    state.pendingExportKind = kind;
    state.pendingExportNode = -1;
    state.pendingExportTerrainSection = -1;
    state.pendingExportPreviewTexture = true;

    IGFD::FileDialogConfig config;
    config.path = state.archiveLoaded && state.archive.gtcPath.has_parent_path()
                      ? state.archive.gtcPath.parent_path().string()
                      : PreferredInitialDirectory();
    config.fileName = HeightmapExportDefaultFileName(state.texturePreview.name, kind);
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ConfirmOverwrite |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kExportFileDialogKey,
        ExportDialogTitle(kind),
        ExportDialogFilter(kind),
        config);
}

struct ExportTaskRequest {
    gtc::ArchiveInfo archive;
    ArchiveBrowser browser;
    ArchiveNode node;
    ExportKind kind = ExportKind::None;
    int terrainSection = -1;
    bool previewTexture = false;
    DecodedTexture previewTextureData;
    std::string previewTextureName;
    std::filesystem::path exportPath;
};

void SetExportProgress(AppState& state, const ExportProgress& progress) {
    std::lock_guard lock(state.exportMutex);
    state.exportStepsDone = progress.stepsDone;
    state.exportTotalSteps = progress.totalSteps;
    state.exportCurrentPath = progress.currentPath;
    state.exportMessage = progress.message.empty() ? "Exporting" : progress.message;
}

ExportSnapshot GetExportSnapshot(AppState& state) {
    std::lock_guard lock(state.exportMutex);
    return {
        state.exportActive,
        state.exportFinished,
        state.exportSucceeded,
        state.exportStepsDone,
        state.exportTotalSteps,
        state.exportCurrentPath,
        state.exportMessage,
    };
}

bool IsExportActive(AppState& state) {
    std::lock_guard lock(state.exportMutex);
    return state.exportActive;
}

void PollExportTask(AppState& state) {
    if (state.exportFuture.valid() &&
        state.exportFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        state.exportFuture.get();

        std::lock_guard lock(state.exportMutex);
        state.exportActive = false;
        state.status = state.exportMessage;
    }
}

ExportTaskRequest BuildExportTaskRequest(AppState& state, const std::filesystem::path& exportPath) {
    ExportTaskRequest request;
    request.archive = state.archive;
    request.browser = state.browser;
    request.kind = state.pendingExportKind;
    request.terrainSection = state.pendingExportTerrainSection;
    request.previewTexture = state.pendingExportPreviewTexture;
    request.exportPath = exportPath;

    if (request.previewTexture) {
        if (!state.texturePreview.open || !state.texturePreview.generatedHeightmap) {
            throw std::runtime_error("Heightmap preview is no longer open.");
        }
        request.previewTextureData = state.texturePreview.decoded;
        request.previewTextureName = state.texturePreview.name;
        return request;
    }

    if (state.pendingExportNode < 0 || state.pendingExportNode >= static_cast<int>(state.browser.nodes.size())) {
        throw std::runtime_error("Export selection is no longer valid.");
    }
    request.node = state.browser.nodes[state.pendingExportNode];
    return request;
}

void ExecuteExportTaskRequest(AppState& exportState,
                              const ExportTaskRequest& request,
                              const ExportProgressCallback& progress) {
    if (request.previewTexture) {
        if (progress) {
            progress({0, 0, request.previewTextureName, "Exporting image"});
        }
        ExportDecodedTextureFile(request.previewTextureData, request.exportPath, request.kind);
        if (progress) {
            progress({1, 1, request.exportPath.string(), "Export complete"});
        }
        return;
    }

    const ArchiveNode& node = request.node;
    switch (request.kind) {
    case ExportKind::TexturePng:
    case ExportKind::TextureTiff:
    case ExportKind::TextureDds:
        if (progress) {
            progress({0, 0, node.path, "Exporting texture"});
        }
        ExportTextureNode(exportState, node, request.exportPath, request.kind);
        break;
    case ExportKind::ModelGlb:
    case ExportKind::ModelFbx:
        if (progress) {
            progress({0, 0, node.path, "Exporting model"});
        }
        ExportModelNode(exportState, node, request.exportPath, request.kind);
        break;
    case ExportKind::LevelLr2:
        ExportLevelNode(exportState, node, request.exportPath, progress);
        break;
    case ExportKind::HeightmapPng:
    case ExportKind::HeightmapTiff:
    case ExportKind::HeightmapDds:
        if (progress) {
            progress({0, 0, node.path, "Exporting heightmap"});
        }
        ExportHeightmapNode(exportState, node, request.exportPath, request.kind, request.terrainSection);
        break;
    case ExportKind::AudioWav:
        if (progress) {
            progress({0, 0, node.path, "Exporting audio"});
        }
        ExportAudioNode(exportState, node, request.exportPath);
        break;
    default:
        throw std::runtime_error("No export is pending.");
    }

    if (progress) {
        progress({1, 1, request.exportPath.string(), "Export complete"});
    }
}

std::string ExportSuccessMessage(const ExportTaskRequest& request) {
    if (request.previewTexture) {
        return "Exported heightmap preview to " + request.exportPath.string();
    }
    return "Exported " + request.node.path + " to " + request.exportPath.string();
}

void StartExportTask(AppState& state, const std::filesystem::path& exportPath) {
    if (IsDumpActive(state)) {
        state.status = "Wait for dump to finish before exporting.";
        return;
    }

    ExportTaskRequest request;
    try {
        request = BuildExportTaskRequest(state, exportPath);
    } catch (const std::exception& error) {
        state.status = std::string("Export failed: ") + error.what();
        return;
    }

    {
        std::lock_guard lock(state.exportMutex);
        if (state.exportActive) {
            state.status = "Wait for the current export to finish.";
            return;
        }

        state.exportActive = true;
        state.exportFinished = false;
        state.exportSucceeded = false;
        state.exportStepsDone = 0;
        state.exportTotalSteps = 0;
        state.exportCurrentPath = request.previewTexture ? request.previewTextureName : request.node.path;
        state.exportMessage = "Starting export";
    }

    state.exportFuture = std::async(std::launch::async, [&state, request = std::move(request)]() {
        try {
            AppState exportState;
            exportState.archive = request.archive;
            exportState.browser = request.browser;
            exportState.archiveLoaded = true;
            exportState.archiveDataLoaded = false;

            const ExportProgressCallback progress = [&state](const ExportProgress& value) {
                SetExportProgress(state, value);
            };
            progress({0, 0, request.previewTexture ? request.previewTextureName : request.node.path, "Preparing export"});
            ExecuteExportTaskRequest(exportState, request, progress);

            std::lock_guard lock(state.exportMutex);
            state.exportFinished = true;
            state.exportSucceeded = true;
            state.exportStepsDone = state.exportTotalSteps == 0 ? 1 : state.exportTotalSteps;
            state.exportTotalSteps = state.exportTotalSteps == 0 ? 1 : state.exportTotalSteps;
            state.exportCurrentPath = request.exportPath.string();
            state.exportMessage = ExportSuccessMessage(request);
        } catch (const std::exception& error) {
            std::lock_guard lock(state.exportMutex);
            state.exportFinished = true;
            state.exportSucceeded = false;
            state.exportMessage = std::string("Export failed: ") + error.what();
        } catch (...) {
            std::lock_guard lock(state.exportMutex);
            state.exportFinished = true;
            state.exportSucceeded = false;
            state.exportMessage = "Export failed: unknown error.";
        }
    });
}
