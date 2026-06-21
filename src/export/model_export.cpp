void LoadModelTexturesForExport(AppState& state, ModelPreview& preview) {
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

ModelPreview LoadModelForExport(AppState& state, const ArchiveNode& node) {
    if (!IsModelNode(node)) {
        throw std::runtime_error("Selected file is not a model.");
    }
    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    ModelPreview model = DecodeMd2Model(bytes);
    model.name = node.name;
    model.path = node.path;
    LoadModelPreviewSkeleton(state, model, node);
    LoadModelTexturesForExport(state, model);
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
