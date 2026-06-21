void HandleNodeDoubleClick(AppState& state, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(state.browser.nodes.size())) {
        return;
    }

    const ArchiveNode& node = state.browser.nodes[nodeIndex];
    if (node.directory) {
        SelectFolder(state, nodeIndex);
        OpenFxPreviewForFolder(state, nodeIndex);
        return;
    }

    if (IsFxNode(node) && NodeExtensionLower(node) == ".ifl" && OpenFxPreviewForIfl(state, node)) {
        return;
    }

    if (IsTextureNode(node)) {
        OpenTexturePreview(state, node);
        return;
    }
    if (IsModelNode(node)) {
        OpenModelPreview(state, node);
        return;
    }
    if (IsLevelNode(node)) {
        OpenLevelPreview(state, node);
        return;
    }
    if (IsAudioNode(node)) {
        OpenAudioPreview(state, node);
        return;
    }
    if (IsTextNode(node)) {
        OpenTextPreview(state, node);
        return;
    }

    ShowUnsupportedPreview(state, node);
}

std::string FileTypeForNode(const AppState& state, const ArchiveNode& node) {
    if (node.directory) {
        return IsFxNode(node) ? "FX Folder" : "File folder";
    }
    if (IsAudioNode(node)) {
        return node.externalFile ? "Music Track" : "AIFF Audio";
    }
    if (IsTextNode(node)) {
        return "Text File";
    }
    if (IsLevelNode(node)) {
        const std::string extension = NodeExtensionLower(node);
        return extension == ".wrl" ? "Saved World" : "Terrain";
    }

    const std::filesystem::path fileName(node.name);
    std::string extension = fileName.extension().string();
    if (extension.empty()) {
        return "File";
    }
    if (extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return extension + " File";
}

std::string FileSizeForNode(const AppState& state, const ArchiveNode& node) {
    if (node.directory) {
        return "";
    }
    if (node.externalFile) {
        try {
            return gtc::FormatByteSize(std::filesystem::file_size(node.externalPath));
        } catch (...) {
            return "";
        }
    }
    if (node.entryIndex == static_cast<std::size_t>(-1)) {
        return "";
    }
    return gtc::FormatByteSize(state.archive.entries[node.entryIndex].size);
}

int ChildFolderCount(const ArchiveBrowser& browser, int nodeIndex) {
    return static_cast<int>(browser.nodes[nodeIndex].folders.size());
}

int ChildFileCount(const ArchiveBrowser& browser, int nodeIndex) {
    return static_cast<int>(browser.nodes[nodeIndex].files.size());
}

bool IsAncestorOrSelf(const ArchiveBrowser& browser, int maybeAncestor, int nodeIndex) {
    int current = nodeIndex;
    while (current >= 0 && current < static_cast<int>(browser.nodes.size())) {
        if (current == maybeAncestor) {
            return true;
        }
        current = browser.nodes[current].parent;
    }
    return false;
}

void SelectFolder(AppState& state, int nodeIndex) {
    state.browser.selectedFolder = nodeIndex;
    state.browser.selectedItem = nodeIndex;
    state.assetFilter = AssetFilter::All;
}

void DrawFolderTreeNode(AppState& state, int nodeIndex) {
    ArchiveBrowser& browser = state.browser;
    ArchiveNode& node = browser.nodes[nodeIndex];

    if (IsAncestorOrSelf(browser, nodeIndex, browser.selectedFolder)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.folders.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (browser.selectedFolder == nodeIndex) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const char* icon = browser.selectedFolder == nodeIndex ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
    if (nodeIndex == 0) {
        icon = ICON_FA_BOX_ARCHIVE;
    }

    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<intptr_t>(nodeIndex)),
        flags,
        "%s  %s",
        icon,
        node.name.c_str());

    if (ImGui::IsItemClicked()) {
        SelectFolder(state, nodeIndex);
    }

    if (open && !node.folders.empty()) {
        for (const int folderIndex : node.folders) {
            DrawFolderTreeNode(state, folderIndex);
        }
        ImGui::TreePop();
    }
}

void DrawBreadcrumb(AppState& state) {
    std::vector<int> chain;
    int current = state.browser.selectedFolder;
    while (current >= 0) {
        chain.push_back(current);
        current = state.browser.nodes[current].parent;
    }
    std::reverse(chain.begin(), chain.end());

    bool drewCrumb = false;
    for (std::size_t index = 0; index < chain.size(); ++index) {
        const int nodeIndex = chain[index];
        if (nodeIndex == 0) {
            continue;
        }

        const ArchiveNode& node = state.browser.nodes[nodeIndex];

        if (drewCrumb) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ICON_FA_CHEVRON_RIGHT);
            ImGui::SameLine();
        }

        const std::string label = (nodeIndex == 0 ? std::string(ICON_FA_BOX_ARCHIVE) : std::string(ICON_FA_FOLDER)) +
                                  "  " + node.name + "##crumb" + std::to_string(nodeIndex);
        if (ImGui::SmallButton(label.c_str())) {
            SelectFolder(state, nodeIndex);
        }
        drewCrumb = true;
    }
}

void DrawRightPane(AppState& state) {
    if (!state.archiveLoaded || state.browser.nodes.empty()) {
        ImGui::TextDisabled("No archive loaded.");
        return;
    }

    const float searchWidth = std::min(300.0f, std::max(180.0f, ImGui::GetContentRegionAvail().x * 0.32f));
    ImGui::SetNextItemWidth(searchWidth);
    const std::string searchHint = std::string(ICON_FA_MAGNIFYING_GLASS) + "  Search...";
    ImGui::InputTextWithHint("##ContentSearch", searchHint.c_str(), state.searchText.data(), state.searchText.size());
    ImGui::SameLine();
    if (state.assetFilter == AssetFilter::All) {
        DrawBreadcrumb(state);
    } else {
        ImGui::TextDisabled("%s", AssetFilterLabel(state.assetFilter));
    }
    ImGui::Separator();

    const ArchiveNode& currentFolder = state.browser.nodes[state.browser.selectedFolder];
    const std::string filter = ToLower(state.searchText.data());
    const ImVec2 tileSize(110.0f, 106.0f);
    const float tileStride = tileSize.x + ImGui::GetStyle().ItemSpacing.x;

    auto nodeMatchesFilter = [&](const ArchiveNode& node) {
        if (!NodeMatchesAssetFilter(node, state.assetFilter)) {
            return false;
        }

        if (filter.empty()) {
            return true;
        }

        const std::string haystack =
            state.assetFilter == AssetFilter::All ? ToLower(node.name) : ToLower(node.path);
        return haystack.find(filter) != std::string::npos;
    };

    auto iconForNode = [&](const ArchiveNode& node) -> const char* {
        if (node.directory) {
            return ICON_FA_FOLDER;
        }

        if (IsAudioNode(node)) {
            return ICON_FA_MUSIC;
        }
        if (IsFxNode(node)) {
            return ICON_FA_BOLT;
        }
        const std::string extension = ToLower(std::filesystem::path(node.name).extension().string());
        if (extension == ".tdf" || extension == ".wrl") {
            return ICON_FA_FILE_LINES;
        }
        if (extension == ".txt" || extension == ".inf") {
            return ICON_FA_FILE_LINES;
        }
        return ICON_FA_FILE;
    };

    auto drawTile = [&](int nodeIndex) {
        const ArchiveNode& node = state.browser.nodes[nodeIndex];
        const bool selected = state.browser.selectedItem == nodeIndex;

        ImGui::PushID(nodeIndex);
        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const ImVec2 tileMax(tileMin.x + tileSize.x, tileMin.y + tileSize.y);
        ImGui::InvisibleButton("tile", tileSize);
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            state.browser.selectedItem = nodeIndex;
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            HandleNodeDoubleClick(state, nodeIndex);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 background = ImGui::GetColorU32(
            selected ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_ChildBg);
        drawList->AddRectFilled(tileMin, tileMax, background, 4.0f);
        if (hovered || selected) {
            drawList->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
        }

        ImFont* font = ImGui::GetFont();
        const char* icon = iconForNode(node);
        const float iconSize = 44.0f;
        const ImVec2 iconTextSize = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
        const ImVec2 iconPos(
            tileMin.x + (tileSize.x - iconTextSize.x) * 0.5f,
            tileMin.y + 13.0f);
        const ImU32 iconColor = node.directory
                                    ? IM_COL32(255, 204, 74, 255)
                                    : IM_COL32(185, 196, 204, 255);
        drawList->AddText(font, iconSize, iconPos, iconColor, icon);

        const std::string displayName = node.name.size() > 32 ? node.name.substr(0, 29) + "..." : node.name;
        const float labelWrapWidth = tileSize.x - 12.0f;
        const float labelWidth = std::min(
            labelWrapWidth,
            font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, displayName.c_str()).x);
        const ImVec2 labelPos(tileMin.x + (tileSize.x - labelWidth) * 0.5f, tileMin.y + 66.0f);
        const ImVec4 labelClip(tileMin.x + 4.0f, tileMin.y + 64.0f, tileMax.x - 4.0f, tileMax.y - 4.0f);
        drawList->AddText(
            font,
            ImGui::GetFontSize(),
            labelPos,
            ImGui::GetColorU32(ImGuiCol_Text),
            displayName.c_str(),
            nullptr,
            labelWrapWidth,
            &labelClip);

        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(node.name.c_str());
            ImGui::TextDisabled("%s", FileTypeForNode(state, node).c_str());
            const std::string size = FileSizeForNode(state, node);
            if (!size.empty()) {
                ImGui::TextDisabled("%s", size.c_str());
            }
            ImGui::EndTooltip();
        }

        if (ImGui::BeginPopupContextItem("AssetContextMenu")) {
            state.browser.selectedItem = nodeIndex;
            if (IsTextureNode(node)) {
                if (ImGui::MenuItem("Export as PNG")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::TexturePng);
                }
                if (ImGui::MenuItem("Export as TIFF")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::TextureTiff);
                }
                if (ImGui::MenuItem("Export as DDS")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::TextureDds);
                }
            }
            if (IsModelNode(node)) {
                if (IsTextureNode(node)) {
                    ImGui::Separator();
                }
                if (ImGui::MenuItem("Export as GLB")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::ModelGlb);
                }
                if (ImGui::MenuItem("Export as FBX")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::ModelFbx);
                }
            }
            if (IsLevelNode(node) && NodeExtensionLower(node) == ".wrl") {
                if (IsTextureNode(node) || IsModelNode(node)) {
                    ImGui::Separator();
                }
                if (ImGui::MenuItem("Export Level")) {
                    OpenExportDialog(state, nodeIndex, ExportKind::LevelLr2);
                }
                std::vector<std::string> heightmapTargets;
                try {
                    heightmapTargets = LoadWrlHeightmapTargetLabels(state, node);
                } catch (const std::exception& error) {
                }

                auto openHeightmapPreview = [&](int terrainSectionIndex) {
                    try {
                        OpenWrlHeightmapPreview(state, node, terrainSectionIndex);
                    } catch (const std::exception& error) {
                        state.status = std::string("Heightmap failed: ") + error.what();
                    }
                };

                if (heightmapTargets.size() > 1) {
                    if (ImGui::BeginMenu("View Heightmap")) {
                        if (ImGui::MenuItem("All levels")) {
                            openHeightmapPreview(-1);
                        }
                        ImGui::Separator();
                        for (std::size_t targetIndex = 0; targetIndex < heightmapTargets.size(); ++targetIndex) {
                            const std::string itemLabel =
                                heightmapTargets[targetIndex] + "##ViewHeightmap" + std::to_string(targetIndex);
                            if (ImGui::MenuItem(itemLabel.c_str())) {
                                openHeightmapPreview(static_cast<int>(targetIndex));
                            }
                        }
                        ImGui::EndMenu();
                    }
                } else if (ImGui::MenuItem("View Heightmap")) {
                    openHeightmapPreview(-1);
                }

                if (ImGui::BeginMenu("Export Heightmap As")) {
                    auto drawHeightmapExportFormats = [&](int terrainSectionIndex, const std::string& targetLabel) {
                        if (ImGui::MenuItem("PNG")) {
                            OpenHeightmapExportDialog(
                                state, nodeIndex, ExportKind::HeightmapPng, terrainSectionIndex, targetLabel);
                        }
                        if (ImGui::MenuItem("TIFF")) {
                            OpenHeightmapExportDialog(
                                state, nodeIndex, ExportKind::HeightmapTiff, terrainSectionIndex, targetLabel);
                        }
                        if (ImGui::MenuItem("DDS")) {
                            OpenHeightmapExportDialog(
                                state, nodeIndex, ExportKind::HeightmapDds, terrainSectionIndex, targetLabel);
                        }
                    };

                    if (heightmapTargets.size() > 1) {
                        if (ImGui::BeginMenu("All levels")) {
                            drawHeightmapExportFormats(-1, {});
                            ImGui::EndMenu();
                        }
                        ImGui::Separator();
                        for (std::size_t targetIndex = 0; targetIndex < heightmapTargets.size(); ++targetIndex) {
                            const std::string menuLabel =
                                heightmapTargets[targetIndex] + "##ExportHeightmap" + std::to_string(targetIndex);
                            if (ImGui::BeginMenu(menuLabel.c_str())) {
                                drawHeightmapExportFormats(
                                    static_cast<int>(targetIndex),
                                    heightmapTargets[targetIndex]);
                                ImGui::EndMenu();
                            }
                        }
                    } else {
                        drawHeightmapExportFormats(-1, {});
                    }
                    ImGui::EndMenu();
                }
            }
            if (!IsTextureNode(node) && !IsModelNode(node) && !(IsLevelNode(node) && NodeExtensionLower(node) == ".wrl")) {
                ImGui::TextDisabled("No export actions");
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    };

    if (ImGui::BeginChild("ContentTiles", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
        const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tileStride));
        std::vector<int> visibleNodes;
        visibleNodes.reserve(state.assetFilter == AssetFilter::All
                                 ? currentFolder.folders.size() + currentFolder.files.size()
                                 : state.browser.nodes.size());

        auto addNodeIfVisible = [&](int nodeIndex) {
            const ArchiveNode& node = state.browser.nodes[nodeIndex];
            if (!nodeMatchesFilter(node)) {
                return;
            }
            visibleNodes.push_back(nodeIndex);
        };

        if (state.assetFilter == AssetFilter::All) {
            for (const int folderIndex : currentFolder.folders) {
                addNodeIfVisible(folderIndex);
            }
            for (const int fileIndex : currentFolder.files) {
                addNodeIfVisible(fileIndex);
            }
        } else {
            for (int nodeIndex = 1; nodeIndex < static_cast<int>(state.browser.nodes.size()); ++nodeIndex) {
                addNodeIfVisible(nodeIndex);
            }
        }

        if (visibleNodes.empty()) {
            ImGui::TextDisabled("No matching items.");
        } else {
            const int rowCount = static_cast<int>((visibleNodes.size() + columns - 1) / columns);
            const float rowHeight = tileSize.y + ImGui::GetStyle().ItemSpacing.y;
            ImGuiListClipper clipper;
            clipper.Begin(rowCount, rowHeight);
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const int firstIndex = row * columns;
                    const int lastIndex = std::min(firstIndex + columns, static_cast<int>(visibleNodes.size()));
                    for (int index = firstIndex; index < lastIndex; ++index) {
                        if (index > firstIndex) {
                            ImGui::SameLine();
                        }
                        drawTile(visibleNodes[index]);
                    }
                }
            }
        }
    }
    ImGui::EndChild();
}

void DrawStatusSummary(AppState& state) {
    const DumpSnapshot dump = GetDumpSnapshot(state);
    if (!dump.active) {
        return;
    }

    const float fraction = dump.totalFiles == 0
                               ? 0.0f
                               : static_cast<float>(dump.filesWritten) /
                                     static_cast<float>(dump.totalFiles);
    const std::string label =
        std::to_string(dump.filesWritten) + " / " + std::to_string(dump.totalFiles);
    ImGui::ProgressBar(fraction, ImVec2(280.0f, 0.0f), label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", dump.message.c_str());

    if (!dump.currentPath.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", dump.currentPath.c_str());
    }
}

void DrawAssetFilterButton(AppState& state, AssetFilter filter) {
    const bool selected = state.assetFilter == filter;
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
    }

    if (ImGui::Button(AssetFilterLabel(filter))) {
        state.assetFilter = filter;
    }

    if (selected) {
        ImGui::PopStyleColor(2);
    }
}

void DrawAssetFilterBar(AppState& state) {
    constexpr AssetFilter filters[] = {
        AssetFilter::All,
        AssetFilter::Textures,
        AssetFilter::Models,
        AssetFilter::Levels,
        AssetFilter::Fx,
        AssetFilter::Audio,
    };

    if (!state.archiveLoaded) {
        ImGui::BeginDisabled();
    }

    for (std::size_t index = 0; index < std::size(filters); ++index) {
        if (index > 0) {
            ImGui::SameLine();
        }
        DrawAssetFilterButton(state, filters[index]);
    }

    if (!state.archiveLoaded) {
        ImGui::EndDisabled();
    }
}

void DrawBottomPanel(AppState& state) {
    ImGui::BeginChild("ContentBrowserDock", ImVec2(0.0f, state.bottomPanelHeight), ImGuiChildFlags_Borders);

    const float fullWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = std::max(180.0f, fullWidth * 0.20f);
    leftWidth = std::min(leftWidth, std::max(120.0f, fullWidth - 320.0f));

    const bool dumpActive = IsDumpActive(state);
    const float actionButtonSize = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    DrawAssetFilterBar(state);
    ImGui::SameLine();

    const float buttonStart =
        ImGui::GetWindowContentRegionMax().x - (actionButtonSize * 2.0f) - spacing;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), buttonStart));

    if (dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(actionButtonSize, 0.0f))) {
        OpenGtcDialog();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Browse for GAMEDATA.GTC");
    }
    if (dumpActive) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!state.archiveLoaded || dumpActive) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_FA_DOWNLOAD, ImVec2(actionButtonSize, 0.0f))) {
        OpenDumpDirectoryDialog(state);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Dump all files");
    }
    if (!state.archiveLoaded || dumpActive) {
        ImGui::EndDisabled();
    }

    const float statusHeight = dumpActive ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    const float bodyHeight = std::max(60.0f, ImGui::GetContentRegionAvail().y - statusHeight);
    if (ImGui::BeginChild("ContentBrowserBody", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None)) {
        ImGui::BeginChild("ArchiveTree", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders);
        if (state.archiveLoaded && !state.browser.nodes.empty()) {
            for (const int folderIndex : state.browser.nodes[0].folders) {
                DrawFolderTreeNode(state, folderIndex);
            }
        } else {
            ImGui::TextDisabled("No archive loaded.");
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("FolderView", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        DrawRightPane(state);
        ImGui::EndChild();
    }
    ImGui::EndChild();

    if (dumpActive) {
        DrawStatusSummary(state);
    }
    ImGui::EndChild();
}

void DrawHorizontalSplitter(AppState& state, float availableHeight) {
    constexpr float splitterHeight = 7.0f;
    const float minHeight = 150.0f;
    const float maxHeight = std::max(minHeight, availableHeight * 0.70f);
    state.bottomPanelHeight = std::clamp(state.bottomPanelHeight, minHeight, maxHeight);

    ImGui::InvisibleButton("##BottomSplitter", ImVec2(-FLT_MIN, splitterHeight));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (active) {
        state.bottomPanelHeight = std::clamp(
            state.bottomPanelHeight - ImGui::GetIO().MouseDelta.y,
            minHeight,
            maxHeight);
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 color = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(min, max, color, 1.0f);
}
