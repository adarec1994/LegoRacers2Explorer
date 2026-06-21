void DrawEmptyState(AppState& state);

void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cellSize) {
    const ImU32 dark = IM_COL32(45, 48, 50, 255);
    const ImU32 light = IM_COL32(72, 76, 80, 255);
    for (float y = min.y; y < max.y; y += cellSize) {
        for (float x = min.x; x < max.x; x += cellSize) {
            const bool alternate =
                (static_cast<int>((x - min.x) / cellSize) + static_cast<int>((y - min.y) / cellSize)) % 2 == 0;
            drawList->AddRectFilled(
                ImVec2(x, y),
                ImVec2(std::min(x + cellSize, max.x), std::min(y + cellSize, max.y)),
                alternate ? dark : light);
        }
    }
}

void UpdateTexturePreviewAnimation(TexturePreview& preview) {
    if (!preview.animated || preview.frames.size() <= 1 || preview.frameTicks.empty()) {
        return;
    }

    const double now = ImGui::GetTime();
    if (preview.lastFrameTime <= 0.0) {
        preview.lastFrameTime = now;
        return;
    }

    const int ticks = preview.frameIndex < static_cast<int>(preview.frameTicks.size())
                          ? preview.frameTicks[preview.frameIndex]
                          : 1;
    const double frameSeconds = static_cast<double>(std::max(1, ticks)) * 0.08;
    if (now - preview.lastFrameTime < frameSeconds) {
        return;
    }

    preview.lastFrameTime = now;
    preview.frameIndex = (preview.frameIndex + 1) % static_cast<int>(preview.frames.size());
    preview.decoded = preview.frames[preview.frameIndex];
    UploadPreviewTexture(preview);
}

void DrawTextPreview(AppState& state) {
    TextPreview& preview = state.textPreview;
    if (!preview.open) {
        return;
    }

    if (ImGui::SmallButton("x##CloseTextPreview")) {
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_FILE_LINES, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", preview.status.c_str());
    }

    if (!preview.path.empty()) {
        ImGui::TextDisabled("%s", preview.path.c_str());
    }
    ImGui::Separator();

    ImGui::BeginChild(
        "TextPreviewContent",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);
    if (preview.content.empty()) {
        ImGui::TextDisabled("Empty text file.");
    } else {
        ImGui::TextUnformatted(preview.content.c_str(), preview.content.c_str() + preview.content.size());
    }
    ImGui::EndChild();
}

void DrawTexturePreview(AppState& state) {
    TexturePreview& preview = state.texturePreview;
    if (!preview.open) {
        return;
    }

    try {
        UpdateTexturePreviewAnimation(preview);
    } catch (const std::exception& error) {
        preview.status = std::string("Animation update failed: ") + error.what();
    }

    if (ImGui::SmallButton("x##CloseTexturePreview")) {
        DestroyPreviewTexture(preview);
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(preview.name.c_str());

    if (preview.decoded.width > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%dx%d  %d-bit  %d mips",
            preview.decoded.width,
            preview.decoded.height,
            preview.decoded.bitsPerPixel,
            preview.decoded.mipLevels);
        if (preview.animated) {
            ImGui::SameLine();
            ImGui::TextDisabled(
                "frame %d/%d",
                preview.frameIndex + 1,
                static_cast<int>(preview.frames.size()));
        }
    }

    bool changed = false;
    ImGui::SameLine();
    changed |= ImGui::Checkbox("R##PreviewRed", &preview.red);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("G##PreviewGreen", &preview.green);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("B##PreviewBlue", &preview.blue);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("A##PreviewAlpha", &preview.alpha);

    if (changed && !preview.decoded.rgba.empty()) {
        try {
            UploadPreviewTexture(preview);
            preview.status.clear();
        } catch (const std::exception& error) {
            preview.status = std::string("Preview update failed: ") + error.what();
        }
    }

    if (!preview.status.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
    }

    ImGui::Separator();
    if (preview.textureId == 0 || preview.decoded.width <= 0 || preview.decoded.height <= 0) {
        return;
    }

    ImGui::BeginChild(
        "TexturePreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    const float scale = std::clamp(
        std::min(
            canvasSize.x / static_cast<float>(preview.decoded.width),
            canvasSize.y / static_cast<float>(preview.decoded.height)),
        0.05f,
        16.0f);
    const ImVec2 imageSize(
        static_cast<float>(preview.decoded.width) * scale,
        static_cast<float>(preview.decoded.height) * scale);
    const ImVec2 imageMin(
        canvasMin.x + std::max(0.0f, (canvasSize.x - imageSize.x) * 0.5f),
        canvasMin.y + std::max(0.0f, (canvasSize.y - imageSize.y) * 0.5f));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawCheckerboard(drawList, imageMin, imageMax, 12.0f);
    ImGui::SetCursorScreenPos(imageMin);
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<std::uintptr_t>(preview.textureId));
    ImGui::Image(ImTextureRef(textureId), imageSize);
    if (preview.generatedHeightmap && ImGui::BeginPopupContextItem("HeightmapPreviewContextMenu")) {
        if (ImGui::BeginMenu("Export Heightmap As")) {
            if (ImGui::MenuItem("PNG")) {
                OpenPreviewTextureExportDialog(state, ExportKind::HeightmapPng);
            }
            if (ImGui::MenuItem("TIFF")) {
                OpenPreviewTextureExportDialog(state, ExportKind::HeightmapTiff);
            }
            if (ImGui::MenuItem("DDS")) {
                OpenPreviewTextureExportDialog(state, ExportKind::HeightmapDds);
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

std::string FormatAudioTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const int totalSeconds = static_cast<int>(std::floor(seconds + 0.5));
    const int minutes = totalSeconds / 60;
    const int remainingSeconds = totalSeconds % 60;
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, remainingSeconds);
    return buffer;
}

bool AudioIconButton(const char* icon, const char* id, const char* tooltip, ImVec2 size) {
    const std::string label = std::string(icon) + "##" + id;
    const bool clicked = ImGui::Button(label.c_str(), size);
    if (ImGui::IsItemHovered() && tooltip != nullptr) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

void UpdateFxPreviewAnimation(FxPreview& preview) {
    if (!preview.open || !preview.playing || preview.frames.size() <= 1) {
        return;
    }

    const double now = ImGui::GetTime();
    if (preview.lastFrameTime <= 0.0) {
        preview.lastFrameTime = now;
        return;
    }

    const int ticks = preview.frameIndex < static_cast<int>(preview.frameTicks.size())
                          ? preview.frameTicks[preview.frameIndex]
                          : 1;
    const double frameSeconds =
        static_cast<double>(std::max(1, ticks)) / static_cast<double>(std::max(1.0f, preview.fps));
    if (now - preview.lastFrameTime < frameSeconds) {
        return;
    }

    int nextFrame = preview.frameIndex + 1;
    if (nextFrame >= static_cast<int>(preview.frames.size())) {
        if (!preview.loop) {
            preview.playing = false;
            preview.lastFrameTime = now;
            return;
        }
        nextFrame = 0;
    }

    preview.lastFrameTime = now;
    SetFxPreviewFrame(preview, nextFrame);
}

void DrawFxPreview(AppState& state) {
    FxPreview& preview = state.fxPreview;
    if (!preview.open) {
        return;
    }

    try {
        UpdateFxPreviewAnimation(preview);
    } catch (const std::exception& error) {
        preview.status = std::string("FX animation failed: ") + error.what();
    }

    if (ImGui::SmallButton("x##CloseFxPreview")) {
        StopFxPreview(preview);
        state.status = "FX preview closed.";
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_BOLT, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.frames.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    if (!preview.frames.empty()) {
        if (preview.frames.size() > 1) {
            if (AudioIconButton(
                    preview.playing ? ICON_FA_PAUSE : ICON_FA_PLAY,
                    "FxPlayPause",
                    preview.playing ? "Pause" : "Play",
                    ImVec2(34.0f, 0.0f))) {
                preview.playing = !preview.playing;
                preview.lastFrameTime = ImGui::GetTime();
            }
            ImGui::SameLine();
            if (AudioIconButton(ICON_FA_STOP, "FxStop", "Stop", ImVec2(34.0f, 0.0f))) {
                preview.playing = false;
                SetFxPreviewFrame(preview, 0);
                preview.lastFrameTime = ImGui::GetTime();
            }
            ImGui::SameLine();
            const std::string loopLabel = std::string(ICON_FA_REPEAT) + "##FxLoop";
            ImGui::Checkbox(loopLabel.c_str(), &preview.loop);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Loop");
            }

            int frameNumber = preview.frameIndex + 1;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::SliderInt(
                    "##FxFrame",
                    &frameNumber,
                    1,
                    static_cast<int>(preview.frames.size()),
                    "frame %d")) {
                preview.playing = false;
                SetFxPreviewFrame(preview, frameNumber - 1);
                preview.lastFrameTime = ImGui::GetTime();
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderFloat("##FxFps", &preview.fps, 1.0f, 30.0f, "%.1f fps")) {
                preview.lastFrameTime = ImGui::GetTime();
            }
        }

        if (!preview.frameNames.empty() &&
            preview.frameIndex >= 0 &&
            preview.frameIndex < static_cast<int>(preview.frameNames.size())) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", std::filesystem::path(preview.frameNames[preview.frameIndex]).filename().string().c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", preview.frameNames[preview.frameIndex].c_str());
            }
        }
    }

    ImGui::Separator();
    ImGui::BeginChild(
        "FxPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    if (preview.textureId == 0 || preview.decoded.width <= 0 || preview.decoded.height <= 0) {
        const char* message = preview.status.empty() ? "No FX frame loaded." : preview.status.c_str();
        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message);
        ImGui::EndChild();
        return;
    }

    const float scale = std::clamp(
        std::min(
            canvasSize.x / static_cast<float>(preview.decoded.width),
            canvasSize.y / static_cast<float>(preview.decoded.height)),
        0.05f,
        18.0f);
    const ImVec2 imageSize(
        static_cast<float>(preview.decoded.width) * scale,
        static_cast<float>(preview.decoded.height) * scale);
    const ImVec2 imageMin(
        canvasMin.x + std::max(0.0f, (canvasSize.x - imageSize.x) * 0.5f),
        canvasMin.y + std::max(0.0f, (canvasSize.y - imageSize.y) * 0.5f));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawCheckerboard(drawList, imageMin, imageMax, 12.0f);
    ImGui::SetCursorScreenPos(imageMin);
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<std::uintptr_t>(preview.textureId));
    ImGui::Image(ImTextureRef(textureId), imageSize);

    ImGui::EndChild();
}

void DrawAudioPreview(AppState& state) {
    AudioPreview& preview = state.audioPreview;
    if (!preview.open) {
        return;
    }

    if (ImGui::SmallButton("x##CloseAudioPreview")) {
        StopAudioPreview(preview);
        state.status = "Audio preview closed.";
        return;
    }
    ImGui::SameLine();
    ImGui::Text("%s  %s", ICON_FA_MUSIC, preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.decoded.samples.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    ImGui::Separator();
    ImGui::BeginChild(
        "AudioPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const float panelWidth = std::clamp(canvasSize.x - 48.0f, 300.0f, 720.0f);
    const float panelHeight = 164.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (canvasSize.y - panelHeight) * 0.5f));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (canvasSize.x - panelWidth) * 0.5f));

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + panelWidth);
    ImGui::TextDisabled("%s", preview.path.c_str());
    ImGui::PopTextWrapPos();

    const double duration =
        preview.decoded.sampleRate > 0
            ? static_cast<double>(preview.decoded.frameCount) / static_cast<double>(preview.decoded.sampleRate)
            : 0.0;
    std::uint64_t cursor = preview.cursorFrame.load(std::memory_order_relaxed);
    cursor = std::min(cursor, preview.decoded.frameCount);
    const double elapsed =
        preview.decoded.sampleRate > 0
            ? static_cast<double>(cursor) / static_cast<double>(preview.decoded.sampleRate)
            : 0.0;

    float progress =
        preview.decoded.frameCount > 0
            ? static_cast<float>(static_cast<double>(cursor) / static_cast<double>(preview.decoded.frameCount))
            : 0.0f;
    ImGui::SetNextItemWidth(panelWidth);
    if (ImGui::SliderFloat("##AudioSeek", &progress, 0.0f, 1.0f, "")) {
        const auto target =
            static_cast<std::uint64_t>(std::clamp(progress, 0.0f, 1.0f) * static_cast<float>(preview.decoded.frameCount));
        preview.cursorFrame.store(std::min(target, preview.decoded.frameCount), std::memory_order_relaxed);
    }

    ImGui::TextDisabled(
        "%s / %s  %d Hz",
        FormatAudioTime(elapsed).c_str(),
        FormatAudioTime(duration).c_str(),
        preview.decoded.sampleRate);

    const bool hasAudio = !preview.decoded.samples.empty();
    if (!hasAudio) {
        ImGui::BeginDisabled();
    }

    const bool playing = preview.playing.load(std::memory_order_relaxed);
    if (AudioIconButton(
            playing ? ICON_FA_PAUSE : ICON_FA_PLAY,
            "AudioPlayPause",
            playing ? "Pause" : "Play",
            ImVec2(46.0f, 38.0f))) {
        try {
            if (playing) {
                preview.playing.store(false, std::memory_order_relaxed);
            } else {
                if (preview.cursorFrame.load(std::memory_order_relaxed) >= preview.decoded.frameCount) {
                    preview.cursorFrame.store(0, std::memory_order_relaxed);
                }
                if (!preview.deviceInitialized) {
                    StartAudioPlayback(preview);
                } else {
                    preview.playing.store(true, std::memory_order_relaxed);
                }
                state.status = "Playing " + preview.path;
            }
        } catch (const std::exception& error) {
            preview.status = std::string("Audio playback failed: ") + error.what();
            state.status = preview.status;
        }
    }
    ImGui::SameLine();
    if (AudioIconButton(ICON_FA_STOP, "AudioStop", "Stop", ImVec2(46.0f, 38.0f))) {
        preview.playing.store(false, std::memory_order_relaxed);
        preview.cursorFrame.store(0, std::memory_order_relaxed);
        state.status = "Stopped " + preview.path;
    }

    ImGui::SameLine();
    bool loop = preview.loop.load(std::memory_order_relaxed);
    const std::string loopLabel = std::string(ICON_FA_REPEAT) + "##AudioLoop";
    if (ImGui::Checkbox(loopLabel.c_str(), &loop)) {
        preview.loop.store(loop, std::memory_order_relaxed);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Loop");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(ICON_FA_VOLUME_HIGH);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(90.0f, panelWidth - 260.0f));
    float volumePercent = preview.volumeUi * 100.0f;
    if (ImGui::SliderFloat("##AudioVolume", &volumePercent, 0.0f, 100.0f, "%.0f%%")) {
        preview.volumeUi = std::clamp(volumePercent / 100.0f, 0.0f, 1.0f);
        preview.volume.store(std::clamp(preview.volumeUi, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    if (!hasAudio) {
        ImGui::EndDisabled();
    }

    ImGui::EndGroup();
    ImGui::EndChild();
}
