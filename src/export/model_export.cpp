ModelPreview LoadModelForExport(AppState& state, const ArchiveNode& node) {
    if (!IsModelNode(node)) {
        throw std::runtime_error("Selected file is not a model.");
    }
    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    ModelPreview model = DecodeMd2Model(bytes);
    model.name = node.name;
    model.path = node.path;
    LoadModelPreviewSkeleton(state, model, node);
    LoadModelPreviewTextures(state, model);
    return model;
}

void ExportModelNode(AppState& state, const ArchiveNode& node, const std::filesystem::path& requestedPath, ExportKind kind) {
    ModelPreview model = LoadModelForExport(state, node);
    switch (kind) {
    case ExportKind::ModelGlb:
        ExportModelGlb(model, requestedPath);
        break;
    case ExportKind::ModelFbx:
        ExportModelFbx(model, requestedPath);
        break;
    default:
        throw std::runtime_error("Unsupported model export format.");
    }
    DestroyModelTextures(model);
    DestroyModelRenderTextureImmediate(model);
}
