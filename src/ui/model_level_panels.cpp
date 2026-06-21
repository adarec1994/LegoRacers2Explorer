std::string JoinNames(const std::vector<std::string>& names, std::size_t maxNames) {
    std::string text;
    const std::size_t count = std::min(names.size(), maxNames);
    for (std::size_t index = 0; index < count; ++index) {
        if (!text.empty()) {
            text += ", ";
        }
        text += names[index];
    }
    if (names.size() > count) {
        text += ", ...";
    }
    return text;
}

Vec3 RotateX(Vec3 value, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {
        value.x,
        value.y * cosine - value.z * sine,
        value.y * sine + value.z * cosine,
    };
}

Vec3 RotateY(Vec3 value, float radians) {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {
        value.x * cosine + value.z * sine,
        value.y,
        -value.x * sine + value.z * cosine,
    };
}

Vec3 ModelPointToView(const ModelPreview& preview, Vec3 point) {
    Vec3 view = Subtract(point, preview.center);
    view = RotateY(view, preview.yaw);
    view = RotateX(view, preview.pitch);
    return view;
}

ImVec2 ProjectModelPoint(
    const ModelPreview& preview,
    Vec3 point,
    ImVec2 canvasMin,
    ImVec2 canvasSize) {
    const float aspect = std::max(0.001f, canvasSize.x / std::max(1.0f, canvasSize.y));
    const float halfHeight = std::max(0.001f, preview.radius * 1.18f / std::max(preview.zoom, 0.001f));
    const float halfWidth = halfHeight * aspect;
    const Vec3 view = ModelPointToView(preview, point);
    const float normalizedX = std::clamp(view.x / halfWidth, -4.0f, 4.0f);
    const float normalizedY = std::clamp(view.y / halfHeight, -4.0f, 4.0f);
    return {
        canvasMin.x + (normalizedX * 0.5f + 0.5f) * canvasSize.x,
        canvasMin.y + (0.5f - normalizedY * 0.5f) * canvasSize.y,
    };
}

float DistancePointSegmentSquared(ImVec2 point, ImVec2 start, ImVec2 end) {
    const ImVec2 segment(end.x - start.x, end.y - start.y);
    const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
    if (lengthSquared <= 0.0001f) {
        const float dx = point.x - start.x;
        const float dy = point.y - start.y;
        return dx * dx + dy * dy;
    }

    const float t = std::clamp(
        ((point.x - start.x) * segment.x + (point.y - start.y) * segment.y) / lengthSquared,
        0.0f,
        1.0f);
    const ImVec2 closest(start.x + segment.x * t, start.y + segment.y * t);
    const float dx = point.x - closest.x;
    const float dy = point.y - closest.y;
    return dx * dx + dy * dy;
}

int PickModelBone(ModelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    if (!preview.showSkeleton || preview.skeleton.empty()) {
        return -1;
    }

    UpdateModelSkinning(preview);
    float bestDistanceSquared = 12.0f * 12.0f;
    int bestBone = -1;
    for (const SkeletonBone& bone : preview.skeleton) {
        const ImVec2 joint = ProjectModelPoint(preview, bone.worldPosition, canvasMin, canvasSize);
        const float jointDx = mouse.x - joint.x;
        const float jointDy = mouse.y - joint.y;
        const float jointDistanceSquared = jointDx * jointDx + jointDy * jointDy;
        if (jointDistanceSquared < bestDistanceSquared) {
            bestDistanceSquared = jointDistanceSquared;
            bestBone = bone.id;
        }

        if (bone.parent >= 0 && bone.parent < static_cast<int>(preview.skeleton.size())) {
            const ImVec2 parent = ProjectModelPoint(
                preview,
                preview.skeleton[bone.parent].worldPosition,
                canvasMin,
                canvasSize);
            const float segmentDistanceSquared = DistancePointSegmentSquared(mouse, parent, joint);
            if (segmentDistanceSquared < bestDistanceSquared) {
                bestDistanceSquared = segmentDistanceSquared;
                bestBone = bone.id;
            }
        }
    }

    return bestBone;
}

Vec3 CameraViewAxisModelSpace(const ModelPreview& preview) {
    Vec3 axis = {0.0f, 0.0f, 1.0f};
    axis = RotateX(axis, -preview.pitch);
    axis = RotateY(axis, -preview.yaw);
    return Normalize(axis);
}

void ResetSelectedBonePose(ModelPreview& preview) {
    preview.rotatingBone = false;
    preview.boneEditAngle = 0.0f;
    UpdateModelSkinning(preview);
}

void DeselectModelBone(ModelPreview& preview) {
    preview.selectedBone = -1;
    ResetSelectedBonePose(preview);
}

void DrawModelRigControls(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        ImGui::BeginDisabled();
        bool showSkeleton = false;
        ImGui::Checkbox("Skeleton", &showSkeleton);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("No matching BSB skeleton was found for this model.");
        }
        return;
    }

    ImGui::Checkbox("Skeleton", &preview.showSkeleton);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%d bones", static_cast<int>(preview.skeleton.size()));
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%d bones", static_cast<int>(preview.skeleton.size()));
    if (preview.selectedBone >= 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("bone %d%s", preview.selectedBone, preview.rotatingBone ? " rotating" : "");
    }
    if (!preview.skeletonClips.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%d clips", static_cast<int>(preview.skeletonClips.size()));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", JoinNames(preview.skeletonClips, 12).c_str());
        }
    }
}

void DrawModelSectionControls(ModelPreview& preview) {
    if (preview.sections.size() <= 1) {
        return;
    }

    if (ImGui::SmallButton("All##ModelSections")) {
        for (ModelSection& section : preview.sections) {
            section.visible = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("None##ModelSections")) {
        for (ModelSection& section : preview.sections) {
            section.visible = false;
        }
    }
    ImGui::SameLine();

    ImGui::BeginChild(
        "ModelSectionControls",
        ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + 4.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    for (std::size_t index = 0; index < preview.sections.size(); ++index) {
        ModelSection& section = preview.sections[index];
        bool visible = section.visible;
        const std::string label =
            "M" + std::to_string(index + 1) + "##ModelSection" + std::to_string(index);
        if (ImGui::Checkbox(label.c_str(), &visible)) {
            section.visible = visible;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", section.name.c_str());
        }
        ImGui::SameLine();
    }
    ImGui::EndChild();
}

void DrawModelAnimationPanel(ModelPreview& preview, ImVec2 size) {
    ImGui::BeginChild(
        "ModelAnimationPanel",
        size,
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Animations");
    if (HasActiveModelAnimation(preview)) {
        const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
        const double duration = clip.fps > 0.0f
                                    ? static_cast<double>(clip.frameCount) / static_cast<double>(clip.fps)
                                    : 0.0;
        float progress = duration > 0.0 ? static_cast<float>(preview.animationTime / duration) : 0.0f;
        progress = std::clamp(progress, 0.0f, 1.0f);
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

        const char* playPauseIcon = preview.animationPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
        if (AudioIconButton(playPauseIcon, "ModelAnimationPlayPause", "Play / pause", ImVec2(36.0f, 0.0f))) {
            preview.animationPlaying = !preview.animationPlaying;
            preview.animationLastUpdateTime = ImGui::GetTime();
        }
        ImGui::SameLine();
        if (AudioIconButton(ICON_FA_STOP, "ModelAnimationStop", "Stop", ImVec2(36.0f, 0.0f))) {
            preview.animationPlaying = false;
            preview.animationTime = 0.0;
            preview.animationLastUpdateTime = ImGui::GetTime();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%d frames  %.0f fps",
            clip.frameCount,
            static_cast<double>(clip.fps));
    } else if (!preview.animations.empty()) {
        ImGui::TextDisabled("Select a clip");
    } else {
        ImGui::TextDisabled("No clips");
    }

    ImGui::Separator();
    ImGui::BeginChild("ModelAnimationList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(preview.animations.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            ModelAnimationClip& clip = preview.animations[index];
            const bool selected = index == preview.selectedAnimation;
            const std::string label =
                std::string(clip.loaded ? ICON_FA_PLAY : ICON_FA_FILE) + "  " +
                clip.name + "##ModelAnimation" + std::to_string(index);
            if (!clip.loaded) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable(label.c_str(), selected)) {
                PlayModelAnimation(preview, index);
            }
            if (!clip.loaded) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "%s%s%s",
                    clip.status.c_str(),
                    clip.path.empty() ? "" : "\n",
                    clip.path.c_str());
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawModelPreviewCanvas(AppState& state, ModelPreview& preview) {
    if (!preview.open) {
        return;
    }

    ImGui::BeginChild(
        "ModelPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    ImGui::InvisibleButton("##ModelPreviewInput", canvasSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();

    if (preview.selectedBone >= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        DeselectModelBone(preview);
        state.status = "Bone deselected.";
    }

    if (preview.selectedBone >= 0 && ImGui::IsKeyPressed(ImGuiKey_R)) {
        preview.rotatingBone = true;
        preview.boneRotationAxis = CameraViewAxisModelSpace(preview);
        preview.boneEditAngle = 0.0f;
        state.status = "Rotating bone " + std::to_string(preview.selectedBone);
    }

    if (preview.rotatingBone) {
        if (hovered && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            preview.boneEditAngle += (io.MouseDelta.x + io.MouseDelta.y * 0.35f) * 0.0125f;
            UpdateModelSkinning(preview);
        }
        if (hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
            ResetSelectedBonePose(preview);
            state.status = "Bone rotation reset.";
        }
    } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int pickedBone = PickModelBone(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedBone >= 0) {
            preview.selectedBone = pickedBone;
            preview.showSkeleton = true;
            preview.boneEditAngle = 0.0f;
            preview.rotatingBone = false;
            state.status = "Selected bone " + std::to_string(preview.selectedBone);
        } else if (preview.selectedBone >= 0) {
            DeselectModelBone(preview);
            state.status = "Bone deselected.";
        }
    } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && preview.selectedBone >= 0) {
        const int pickedBone = PickModelBone(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedBone < 0) {
            DeselectModelBone(preview);
            state.status = "Bone deselected.";
        }
    }

    if (!preview.rotatingBone && active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
            preview.yaw += io.MouseDelta.x * 0.010f;
            preview.pitch += io.MouseDelta.y * 0.010f;
            preview.pitch = std::clamp(preview.pitch, -1.45f, 1.45f);
        }
    }
    if (hovered && io.MouseWheel != 0.0f) {
        preview.zoom *= std::pow(1.12f, io.MouseWheel);
        preview.zoom = std::clamp(preview.zoom, 0.05f, 80.0f);
    }

    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 19, 20, 255));

    if (preview.vertices.empty() || preview.triangles.empty()) {
        const std::string message = preview.status.empty() ? "No drawable mesh." : preview.status;
        const ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
        drawList->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message.c_str());
        ImGui::EndChild();
        return;
    }

    gPendingModelRender.preview = &preview;
    gPendingModelRender.canvasMin = canvasMin;
    gPendingModelRender.canvasMax = canvasMax;
    drawList->AddCallback(RenderModelDrawCallback, &gPendingModelRender);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::EndChild();
}

void DrawModelPreview(AppState& state) {
    ModelPreview& preview = state.modelPreview;
    if (!preview.open) {
        return;
    }

    AdvanceModelAnimation(preview);

    if (ImGui::SmallButton("x##CloseModelPreview")) {
        DestroyModelRenderTexture(preview);
        preview.open = false;
        return;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(preview.name.c_str());
    if (!preview.status.empty()) {
        ImGui::SameLine();
        const bool failed = preview.vertices.empty() || preview.triangles.empty();
        if (failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.36f, 1.0f), "%s", preview.status.c_str());
        } else {
            ImGui::TextDisabled("%s", preview.status.c_str());
        }
    }

    DrawModelRigControls(preview);
    DrawModelSectionControls(preview);
    ImGui::Separator();

    const bool showAnimationPanel = !preview.skeleton.empty();
    if (!showAnimationPanel) {
        DrawModelPreviewCanvas(state, preview);
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float panelWidth = std::clamp(available.x * 0.22f, 220.0f, 320.0f);
    const float canvasWidth = std::max(120.0f, available.x - panelWidth - spacing);

    ImGui::BeginChild("ModelPreviewCanvasDock", ImVec2(canvasWidth, 0.0f), ImGuiChildFlags_None);
    DrawModelPreviewCanvas(state, preview);
    ImGui::EndChild();
    ImGui::SameLine();
    DrawModelAnimationPanel(preview, ImVec2(panelWidth, 0.0f));
}

void DrawLevelPreviewCanvas(AppState& state, LevelPreview& preview) {
    if (!preview.open) {
        return;
    }

    ImGui::BeginChild(
        "LevelPreviewCanvas",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        ImGui::EndChild();
        return;
    }

    ImGui::InvisibleButton("##LevelPreviewInput", canvasSize);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();

    EnsureLevelFlyCamera(preview);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        POINT cursor = {};
        if (GetCursorPos(&cursor)) {
            preview.mouseLookLocked = true;
            preview.mouseLookLockX = cursor.x;
            preview.mouseLookLockY = cursor.y;
        }
    }
    if (preview.mouseLookLocked && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        preview.mouseLookLocked = false;
    }

    const bool rightMouseLook = preview.mouseLookLocked && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (rightMouseLook) {
        POINT cursor = {};
        if (GetCursorPos(&cursor)) {
            const float deltaX = static_cast<float>(cursor.x - preview.mouseLookLockX);
            const float deltaY = static_cast<float>(cursor.y - preview.mouseLookLockY);
            preview.yaw -= deltaX * 0.0045f;
            preview.pitch -= deltaY * 0.0045f;
            SetCursorPos(preview.mouseLookLockX, preview.mouseLookLockY);
        }
        preview.pitch = std::clamp(preview.pitch, -1.50f, 1.50f);
    }

    if (hovered && !rightMouseLook && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int pickedObject = PickLevelModelObject(preview, io.MousePos, canvasMin, canvasSize);
        if (pickedObject >= 0) {
            preview.selectedObject = pickedObject;
            preview.scrollSelectedObjectIntoView = true;
            const WorldObject& object = preview.objects[static_cast<std::size_t>(pickedObject)];
            state.status = "Selected " + (object.name.empty() ? object.className : object.name);
        } else if (preview.selectedObject >= 0) {
            preview.selectedObject = -1;
            preview.scrollSelectedObjectIntoView = false;
            state.status = "Model selection cleared.";
        }
    }

    const bool flyActive = hovered || rightMouseLook;
    if (flyActive) {
        Vec3 move{};
        const Vec3 forward = LevelCameraForward(preview);
        const Vec3 right = LevelCameraRight(preview);
        const Vec3 up{0.0f, 1.0f, 0.0f};
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            move = Add(move, forward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            move = Subtract(move, forward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            move = Add(move, right);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            move = Subtract(move, right);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) {
            move = Add(move, up);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
            move = Subtract(move, up);
        }

        float speed = preview.flySpeed;
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
            speed *= 3.5f;
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt)) {
            speed *= 0.25f;
        }

        if (Length(move) > 0.0001f) {
            preview.flyCameraPosition = Add(
                preview.flyCameraPosition,
                Multiply(Normalize(move), speed * std::max(0.001f, io.DeltaTime)));
        }
    }

    if (rightMouseLook) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 19, 20, 255));

    if (preview.vertices.empty() && preview.objects.empty()) {
        const std::string message = preview.status.empty() ? "No level data loaded." : preview.status;
        const ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
        drawList->AddText(
            ImVec2(
                canvasMin.x + std::max(0.0f, (canvasSize.x - textSize.x) * 0.5f),
                canvasMin.y + std::max(0.0f, (canvasSize.y - textSize.y) * 0.5f)),
            IM_COL32(210, 210, 210, 255),
            message.c_str());
        ImGui::EndChild();
        return;
    }

    gPendingLevelRender.preview = &preview;
    gPendingLevelRender.canvasMin = canvasMin;
    gPendingLevelRender.canvasMax = canvasMax;
    drawList->AddCallback(RenderLevelDrawCallback, &gPendingLevelRender);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    ImGui::EndChild();
}

std::string VisibleImGuiLabel(const std::string& label) {
    const std::size_t idSeparator = label.find("##");
    return idSeparator == std::string::npos ? label : label.substr(0, idSeparator);
}

void DrawWrappedCheckbox(const std::string& label,
                         bool* value,
                         bool enabled,
                         const std::string& tooltip,
                         bool& firstOnLine) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string visibleLabel = VisibleImGuiLabel(label);
    const float checkboxWidth =
        ImGui::GetFrameHeight() +
        style.ItemInnerSpacing.x +
        ImGui::CalcTextSize(visibleLabel.c_str()).x;

    if (!firstOnLine) {
        const float nextRight =
            ImGui::GetItemRectMax().x +
            style.ItemSpacing.x +
            checkboxWidth;
        const float contentRight = ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x;
        if (nextRight <= contentRight) {
            ImGui::SameLine();
        }
    }

    if (!enabled) {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox(label.c_str(), value);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", tooltip.c_str());
    }
    firstOnLine = false;
}

std::string LevelControlLabel(const char* visibleLabel, const char* suffix) {
    return std::string(visibleLabel) + "##Level" + suffix + visibleLabel;
}

void DrawLevelVisibilityControls(LevelPreview& preview, const char* suffix) {
    bool firstOnLine = true;
    DrawWrappedCheckbox(LevelControlLabel("Terrain", suffix), &preview.showTerrain, true, {}, firstOnLine);

    const bool hasTerrainTexture = HasLevelTerrainTexture(preview);
    DrawWrappedCheckbox(
        LevelControlLabel("Texture", suffix),
        &preview.showTerrainTexture,
        hasTerrainTexture,
        hasTerrainTexture ? LevelTerrainTextureSummary(preview) : "No terrain texture loaded.",
        firstOnLine);

    DrawWrappedCheckbox(LevelControlLabel("Objects", suffix), &preview.showObjects, true, {}, firstOnLine);
    if (preview.showObjects) {
        DrawWrappedCheckbox(LevelControlLabel("Checkpoints", suffix), &preview.showCheckpoints, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Start Position", suffix), &preview.showStartPositions, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Weapon Pickups", suffix), &preview.showWeaponPickups, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("AI Stuff", suffix), &preview.showAiObjects, true, {}, firstOnLine);
        DrawWrappedCheckbox(LevelControlLabel("Markers", suffix), &preview.showObjectMarkers, true, {}, firstOnLine);
    }
    if (!preview.skyboxAssetIndices.empty()) {
        DrawWrappedCheckbox(LevelControlLabel("Skybox", suffix), &preview.showSkybox, true, {}, firstOnLine);
    }
    if (!preview.waterSheets.empty()) {
        DrawWrappedCheckbox(LevelControlLabel("Water", suffix), &preview.showWater, true, {}, firstOnLine);
    }
}

std::string LevelObjectVisibleLabel(const WorldObject& object) {
    std::string visibleLabel = object.className + "  " + object.name;
    if (!object.modelPath.empty()) {
        visibleLabel += "  ";
        visibleLabel += std::filesystem::path(object.modelPath).filename().string();
    }
    if (object.hasLayer) {
        visibleLabel += "  L " + FormatHex32(object.layer);
    }
    return visibleLabel;
}

std::string LevelTerrainSectionLabel(const LevelTerrainSection& section, int index) {
    std::filesystem::path terrainPath(section.path);
    std::string label = terrainPath.parent_path().filename().string();
    if (label.empty()) {
        label = terrainPath.stem().string();
    }
    if (label.empty()) {
        label = "Level " + std::to_string(index + 1);
    }
    if (section.hasLayer) {
        label += "  " + FormatHex32(section.layer);
    }
    return label;
}

void DrawLevelSublevelSelector(LevelPreview& preview) {
    if (preview.terrainSections.size() <= 1) {
        preview.selectedTerrainSection = -1;
        return;
    }

    if (!HasActiveTerrainSection(preview)) {
        preview.selectedTerrainSection = -1;
    }

    const std::string current =
        HasActiveTerrainSection(preview)
            ? LevelTerrainSectionLabel(
                  preview.terrainSections[static_cast<std::size_t>(preview.selectedTerrainSection)],
                  preview.selectedTerrainSection)
            : std::string("All levels");

    if (ImGui::BeginCombo("Level", current.c_str())) {
        const bool allSelected = preview.selectedTerrainSection < 0;
        if (ImGui::Selectable("All levels", allSelected)) {
            preview.selectedTerrainSection = -1;
            preview.selectedObject = -1;
            preview.scrollSelectedObjectIntoView = false;
            preview.flyCameraInitialized = false;
        }
        if (allSelected) {
            ImGui::SetItemDefaultFocus();
        }

        for (std::size_t index = 0; index < preview.terrainSections.size(); ++index) {
            const bool selected = preview.selectedTerrainSection == static_cast<int>(index);
            const std::string label = LevelTerrainSectionLabel(preview.terrainSections[index], static_cast<int>(index));
            if (ImGui::Selectable(label.c_str(), selected)) {
                preview.selectedTerrainSection = static_cast<int>(index);
                preview.selectedObject = -1;
                preview.scrollSelectedObjectIntoView = false;
                preview.flyCameraInitialized = false;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void DrawLevelObjectPanel(LevelPreview& preview, ImVec2 size) {
    ImGui::BeginChild(
        "LevelObjectPanel",
        size,
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("World");
    DrawLevelSublevelSelector(preview);

    ImGui::Separator();
    ImGui::BeginChild("LevelObjectList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    std::vector<int> visibleObjects;
    visibleObjects.reserve(preview.objects.size());
    for (int index = 0; index < static_cast<int>(preview.objects.size()); ++index) {
        if (IsObjectInActiveSublevel(preview, preview.objects[static_cast<std::size_t>(index)])) {
            visibleObjects.push_back(index);
        }
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visibleObjects.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const int index = visibleObjects[static_cast<std::size_t>(row)];
            const WorldObject& object = preview.objects[index];
            const bool selected = index == preview.selectedObject;
            const std::string visibleLabel = LevelObjectVisibleLabel(object);
            const std::string label = visibleLabel + "##LevelObject" + std::to_string(index);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.03f, 0.46f, 0.13f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.04f, 0.58f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.03f, 0.68f, 0.20f, 1.0f));
            }
            if (ImGui::Selectable(label.c_str(), selected)) {
                preview.selectedObject = index;
                preview.scrollSelectedObjectIntoView = false;
            }
            if (selected) {
                ImGui::PopStyleColor(3);
                if (preview.scrollSelectedObjectIntoView) {
                    ImGui::SetScrollHereY(0.5f);
                    preview.scrollSelectedObjectIntoView = false;
                }
            }
            if (ImGui::IsItemHovered()) {
                std::string tooltip = object.name + "\n" + object.className;
                if (object.hasLayer) {
                    tooltip += "\nlayer " + FormatHex32(object.layer);
                }
                if (!object.binding.empty()) {
                    tooltip += "\nbinding " + object.binding;
                }
                if (object.recordSize > 0) {
                    tooltip += "\nrecord " + std::to_string(object.recordSize) + " bytes";
                }
                if (object.hasPosition) {
                    tooltip +=
                        "\nposition " +
                        std::to_string(object.position.x) + ", " +
                        std::to_string(object.position.y) + ", " +
                        std::to_string(object.position.z);
                }
                for (const std::string& assetPath : object.assetPaths) {
                    tooltip += "\n" + assetPath;
                }
                if (object.assetPaths.empty() && !object.assetPath.empty()) {
                    tooltip += "\n" + object.assetPath;
                }
                ImGui::SetTooltip("%s", tooltip.c_str());
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawLevelPreview(AppState& state) {
    LevelPreview& preview = state.levelPreview;
    if (!preview.open) {
        return;
    }

    DrawLevelVisibilityControls(preview, "Top");
    ImGui::Separator();
    if (preview.objects.empty()) {
        DrawLevelPreviewCanvas(state, preview);
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float maxPanelWidth = std::max(0.0f, available.x - 180.0f - spacing);
    if (maxPanelWidth < 220.0f) {
        DrawLevelPreviewCanvas(state, preview);
        return;
    }
    const float panelWidth = std::min(std::clamp(available.x * 0.24f, 240.0f, 360.0f), maxPanelWidth);
    const float canvasWidth = std::max(1.0f, available.x - panelWidth - spacing);

    ImGui::BeginChild("LevelPreviewCanvasDock", ImVec2(canvasWidth, 0.0f), ImGuiChildFlags_None);
    DrawLevelPreviewCanvas(state, preview);
    ImGui::EndChild();
    ImGui::SameLine();
    DrawLevelObjectPanel(preview, ImVec2(panelWidth, 0.0f));
}
