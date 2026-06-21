std::filesystem::path BlenderAddonOutputPath(std::filesystem::path path) {
    if (ToLower(path.extension().string()) != ".zip") {
        path.replace_extension(".zip");
    }
    return path;
}

void ExportBlenderAddon(AppState& state, const std::filesystem::path& selectedPath) {
    try {
        std::filesystem::path outputPath = BlenderAddonOutputPath(selectedPath);
        if (outputPath.empty() || outputPath.filename().empty()) {
            throw std::runtime_error("Choose an output ZIP path.");
        }
        if (kBlenderAddonZipSize == 0) {
            throw std::runtime_error("Embedded Blender addon is empty.");
        }
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Could not create " + outputPath.string());
        }
        file.write(
            reinterpret_cast<const char*>(kBlenderAddonZipData),
            static_cast<std::streamsize>(kBlenderAddonZipSize));
        if (!file) {
            throw std::runtime_error("Could not write " + outputPath.string());
        }

        state.status = "Exported Blender addon to " + outputPath.string();
    } catch (const std::exception& error) {
        state.status = std::string("Blender addon export failed: ") + error.what();
    }
}

void OpenBlenderAddonDialog() {
    IGFD::FileDialogConfig config;
    config.path = PreferredInitialDirectory();
    config.fileName = "import_lr2.zip";
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ConfirmOverwrite |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kBlenderAddonDialogKey,
        "Export Blender Addon",
        ".zip",
        config);
}
