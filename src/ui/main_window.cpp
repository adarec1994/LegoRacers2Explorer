void DrawRenderArea(AppState& state, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.058f, 0.060f, 1.0f));
    ImGui::BeginChild(
        "RenderArea",
        ImVec2(0.0f, height),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!state.archiveLoaded) {
        DrawEmptyState(state);
    } else if (state.audioPreview.open) {
        DrawAudioPreview(state);
    } else if (state.fxPreview.open) {
        DrawFxPreview(state);
    } else if (state.modelPreview.open) {
        DrawModelPreview(state);
    } else if (state.levelPreview.open) {
        DrawLevelPreview(state);
    } else if (state.textPreview.open) {
        DrawTextPreview(state);
    } else {
        DrawTexturePreview(state);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DrawExplorer(AppState& state) {
    ImVec2 available = ImGui::GetContentRegionAvail();
    constexpr float splitterHeight = 7.0f;
    state.bottomPanelHeight = std::clamp(state.bottomPanelHeight, 150.0f, std::max(150.0f, available.y * 0.70f));
    const float topHeight = std::max(80.0f, available.y - state.bottomPanelHeight - splitterHeight);

    DrawRenderArea(state, topHeight);
    DrawHorizontalSplitter(state, available.y);
    DrawBottomPanel(state);
}

void DrawFileDialogs(AppState& state) {
    if (ImGuiFileDialog::Instance()->Display(
            kChooseGtcDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            LoadArchive(state, ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile));
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display(
            kChooseDumpDirectoryDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outputFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            if (outputFolder.empty()) {
                outputFolder = ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile);
            }
            StartDump(state, outputFolder);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display(
            kExportFileDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            StartExportTask(
                state,
                ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile));
        }
        state.pendingExportKind = ExportKind::None;
        state.pendingExportNode = -1;
        state.pendingExportPreviewTexture = false;
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display(
            kBlenderAddonDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(720.0f, 420.0f),
            ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            ExportBlenderAddon(
                state,
                ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile));
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void DrawExportProgressModal(AppState& state) {
    const ExportSnapshot exportSnapshot = GetExportSnapshot(state);
    constexpr const char* popupName = "Export Progress";

    if (!exportSnapshot.active) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 viewportMin = viewport->Pos;
    const ImVec2 viewportMax = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImDrawList* background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled(viewportMin, viewportMax, IM_COL32(8, 10, 12, 154));

    ImGui::OpenPopup(popupName);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.02f, 0.025f, 0.030f, 0.62f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 18.0f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;
    if (ImGui::BeginPopupModal(popupName, nullptr, flags)) {
        const float fraction = exportSnapshot.totalSteps == 0
                                   ? static_cast<float>(std::fmod(ImGui::GetTime() * 0.35, 1.0))
                                   : static_cast<float>(exportSnapshot.stepsDone) /
                                         static_cast<float>(exportSnapshot.totalSteps);
        const std::string label = exportSnapshot.totalSteps == 0
                                      ? std::string("Exporting")
                                      : std::to_string(exportSnapshot.stepsDone) + " / " +
                                            std::to_string(exportSnapshot.totalSteps);

        ImGui::TextUnformatted("Exporting");
        ImGui::Spacing();
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-FLT_MIN, 0.0f), label.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("%s", exportSnapshot.message.empty() ? "Preparing export" : exportSnapshot.message.c_str());
        if (!exportSnapshot.currentPath.empty()) {
            ImGui::TextDisabled("%s", exportSnapshot.currentPath.c_str());
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void DrawEmptyState(AppState& state) {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    constexpr ImVec2 buttonSize(240.0f, 46.0f);
    const float yOffset = std::max(0.0f, (available.y - buttonSize.y) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
    ImGui::SetCursorPosX((available.x - buttonSize.x) * 0.5f);

    if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + "  Load GAMEDATA.GTC").c_str(), buttonSize)) {
        OpenGtcDialog();
    }

    if (state.status.starts_with("Load failed:") || state.status.starts_with("Last GTC")) {
        const ImVec2 textSize = ImGui::CalcTextSize(state.status.c_str());
        ImGui::SetCursorPosX(std::max(0.0f, (available.x - textSize.x) * 0.5f));
        ImGui::TextDisabled("%s", state.status.c_str());
    }
}

void DrawToolbar(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    state.fontSize = std::clamp(state.fontSize, 12.0f, 28.0f);
    io.FontGlobalScale = state.fontSize / 16.0f;

    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Settings")) {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat("Font Size", &state.fontSize, 12.0f, 28.0f, "%.0f px");
        state.fontSize = std::clamp(state.fontSize, 12.0f, 28.0f);
        io.FontGlobalScale = state.fontSize / 16.0f;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Plugins")) {
        if (ImGui::MenuItem("Blender Addon")) {
            OpenBlenderAddonDialog();
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("About")) {
        state.aboutOpen = true;
    }

    ImGui::EndMenuBar();
}

void DrawMainUi(AppState& state) {
    PollDumpTask(state);
    PollExportTask(state);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_MenuBar;

    ImGui::Begin("Lego Racers 2 Explorer", nullptr, windowFlags);

    DrawToolbar(state);
    DrawExplorer(state);
    DrawAboutWindow(state);
    DrawFileDialogs(state);
    DrawExportProgressModal(state);

    ImGui::End();
}

void ApplyAppStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.085f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.11f, 0.118f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.145f, 0.155f, 0.165f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.19f, 0.32f, 0.41f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.41f, 0.54f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.19f, 0.32f, 0.41f, 0.76f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.41f, 0.54f, 0.86f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.035f);
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFont* baseFont = nullptr;
    const std::filesystem::path segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (std::filesystem::exists(segoe)) {
        baseFont = io.Fonts->AddFontFromFileTTF(segoe.string().c_str(), 16.0f);
    }
    if (baseFont == nullptr) {
        baseFont = io.Fonts->AddFontDefault();
    }

    static constexpr ImWchar iconRanges[] = {0xf000, 0xf8ff, 0};
    if (std::filesystem::exists(FONT_AWESOME_SOLID_TTF)) {
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = 15.0f;
        io.Fonts->AddFontFromFileTTF(FONT_AWESOME_SOLID_TTF, 14.0f, &config, iconRanges);
    }
}
