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
    case ExportKind::HeightmapPng:
        return "Export Heightmap PNG";
    case ExportKind::HeightmapTiff:
        return "Export Heightmap TIFF";
    case ExportKind::HeightmapDds:
        return "Export Heightmap DDS";
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

void ExecuteExportDialogResult(AppState& state, const std::filesystem::path& exportPath) {
    if (state.pendingExportPreviewTexture) {
        if (!state.texturePreview.open || !state.texturePreview.generatedHeightmap) {
            throw std::runtime_error("Heightmap preview is no longer open.");
        }
        ExportDecodedTextureFile(state.texturePreview.decoded, exportPath, state.pendingExportKind);
        state.status = "Exported heightmap preview to " + exportPath.string();
        return;
    }

    if (state.pendingExportNode < 0 || state.pendingExportNode >= static_cast<int>(state.browser.nodes.size())) {
        throw std::runtime_error("Export selection is no longer valid.");
    }

    const ArchiveNode& node = state.browser.nodes[state.pendingExportNode];
    switch (state.pendingExportKind) {
    case ExportKind::TexturePng:
    case ExportKind::TextureTiff:
    case ExportKind::TextureDds:
        ExportTextureNode(state, node, exportPath, state.pendingExportKind);
        break;
    case ExportKind::ModelGlb:
    case ExportKind::ModelFbx:
        ExportModelNode(state, node, exportPath, state.pendingExportKind);
        break;
    case ExportKind::HeightmapPng:
    case ExportKind::HeightmapTiff:
    case ExportKind::HeightmapDds:
        ExportHeightmapNode(state, node, exportPath, state.pendingExportKind, state.pendingExportTerrainSection);
        break;
    default:
        throw std::runtime_error("No export is pending.");
    }

    state.status = "Exported " + node.path + " to " + exportPath.string();
}
