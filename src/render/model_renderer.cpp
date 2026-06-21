std::array<float, 4> ModelMaterialColorFloats(std::uint32_t material) {
    static constexpr std::array<ImVec4, 12> kPalette = {
        ImVec4(0.95f, 0.68f, 0.18f, 1.0f),
        ImVec4(0.32f, 0.62f, 0.95f, 1.0f),
        ImVec4(0.84f, 0.28f, 0.24f, 1.0f),
        ImVec4(0.34f, 0.72f, 0.43f, 1.0f),
        ImVec4(0.78f, 0.55f, 0.95f, 1.0f),
        ImVec4(0.92f, 0.82f, 0.48f, 1.0f),
        ImVec4(0.48f, 0.72f, 0.78f, 1.0f),
        ImVec4(0.88f, 0.48f, 0.58f, 1.0f),
        ImVec4(0.74f, 0.74f, 0.78f, 1.0f),
        ImVec4(0.55f, 0.72f, 0.33f, 1.0f),
        ImVec4(0.93f, 0.57f, 0.32f, 1.0f),
        ImVec4(0.40f, 0.48f, 0.82f, 1.0f),
    };
    const ImVec4 base = kPalette[material % kPalette.size()];
    return {base.x, base.y, base.z, 1.0f};
}

Vec3 TransformModelPointToWorld(const LevelModelInstance& instance, Vec3 point) {
    point.x *= instance.scale.x;
    point.y *= instance.scale.y;
    point.z *= instance.scale.z;
    return Add(instance.position, Rotate(instance.rotation, point));
}

Vec3 LevelWorldToDisplay(Vec3 point) {
    return {-point.x, point.y, point.z};
}

struct PendingModelRender {
    ModelPreview* preview = nullptr;
    ImVec2 canvasMin;
    ImVec2 canvasMax;
};

PendingModelRender gPendingModelRender;

void EmitModelTexCoord(const ModelVertex& vertex, unsigned char coordinateIndex) {
    unsigned char uvSet = coordinateIndex;
    if (uvSet >= vertex.uvSetCount || uvSet >= vertex.uSets.size()) {
        uvSet = 0;
    }
    glTexCoord2f(vertex.uSets[uvSet], vertex.vSets[uvSet]);
}

void EmitModelVertex(const ModelVertex& vertex, unsigned char coordinateIndex = 0) {
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    EmitModelTexCoord(vertex, coordinateIndex);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitSkyboxVertex(const ModelVertex& vertex) {
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glTexCoord2f(vertex.u, vertex.v);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void ApplyModelTextureTiling(unsigned char) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void ResetSolidModelState() {
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

GLuint BuildModelDisplayList(const ModelPreview& preview, bool skyboxUv = false) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    glNewList(displayList, GL_COMPILE);
    ResetSolidModelState();
    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        ResetSolidModelState();
        GLuint textureId = 0;
        if (section.textureIndex < preview.materials.size()) {
            textureId = preview.materials[section.textureIndex].textureId;
        }
        if (textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureId);
            ApplyModelTextureTiling(section.textureTiling);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            if (section.alphaType == 0 || section.alphaType == 1 || section.alphaType == 4) {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.5f);
            }
        } else {
            const auto color = ModelMaterialColorFloats(section.textureIndex);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            glColor4f(color[0], color[1], color[2], color[3]);
        }

        glBegin(GL_TRIANGLES);
        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= preview.vertices.size() ||
                triangle.b >= preview.vertices.size() ||
                triangle.c >= preview.vertices.size()) {
                continue;
            }
            if (skyboxUv) {
                EmitSkyboxVertex(preview.vertices[triangle.a]);
                EmitSkyboxVertex(preview.vertices[triangle.b]);
                EmitSkyboxVertex(preview.vertices[triangle.c]);
            } else {
                EmitModelVertex(preview.vertices[triangle.a], section.textureCoordinateIndex);
                EmitModelVertex(preview.vertices[triangle.b], section.textureCoordinateIndex);
                EmitModelVertex(preview.vertices[triangle.c], section.textureCoordinateIndex);
            }
        }
        glEnd();
        ResetSolidModelState();
    }
    ResetSolidModelState();
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();
    return displayList;
}

void RenderModelSelectionHighlight(const ModelPreview& preview) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.10f, 1.0f, 0.18f, 0.28f);

    glBegin(GL_TRIANGLES);
    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= preview.vertices.size() ||
                triangle.b >= preview.vertices.size() ||
                triangle.c >= preview.vertices.size()) {
                continue;
            }
            glVertex3f(
                preview.vertices[triangle.a].position.x,
                preview.vertices[triangle.a].position.y,
                preview.vertices[triangle.a].position.z);
            glVertex3f(
                preview.vertices[triangle.b].position.x,
                preview.vertices[triangle.b].position.y,
                preview.vertices[triangle.b].position.z);
            glVertex3f(
                preview.vertices[triangle.c].position.x,
                preview.vertices[triangle.c].position.y,
                preview.vertices[triangle.c].position.z);
        }
    }
    glEnd();

    const Vec3 min = preview.boundsMin;
    const Vec3 max = preview.boundsMax;
    const std::array<Vec3, 8> corners = {
        Vec3{min.x, min.y, min.z},
        Vec3{max.x, min.y, min.z},
        Vec3{max.x, max.y, min.z},
        Vec3{min.x, max.y, min.z},
        Vec3{min.x, min.y, max.z},
        Vec3{max.x, min.y, max.z},
        Vec3{max.x, max.y, max.z},
        Vec3{min.x, max.y, max.z},
    };
    static constexpr std::array<std::array<int, 2>, 12> kEdges = {{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glLineWidth(3.0f);
    glColor4f(0.0f, 1.0f, 0.12f, 1.0f);
    glBegin(GL_LINES);
    for (const auto& edge : kEdges) {
        const Vec3 a = corners[static_cast<std::size_t>(edge[0])];
        const Vec3 b = corners[static_cast<std::size_t>(edge[1])];
        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
    }
    glEnd();
}

void RenderModelSkeletonOpenGl(const ModelPreview& preview) {
    if (!preview.showSkeleton || preview.skeleton.empty()) {
        return;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);
    glPointSize(5.0f);

    for (const SkeletonBone& bone : preview.skeleton) {
        if (bone.parent < 0 || bone.parent >= static_cast<int>(preview.skeleton.size())) {
            continue;
        }
        const bool selectedBranch =
            preview.selectedBone >= 0 &&
            IsBoneAffectedBySelection(preview, bone.id, preview.selectedBone);
        if (selectedBranch) {
            glColor4f(1.0f, 0.22f, 0.12f, 1.0f);
            glLineWidth(3.0f);
        } else {
            glColor4f(0.15f, 0.85f, 1.0f, 1.0f);
            glLineWidth(2.0f);
        }
        glBegin(GL_LINES);
        const Vec3 parent = preview.skeleton[bone.parent].worldPosition;
        const Vec3 child = bone.worldPosition;
        glVertex3f(parent.x, parent.y, parent.z);
        glVertex3f(child.x, child.y, child.z);
        glEnd();
    }

    glPointSize(5.0f);
    glColor4f(1.0f, 0.92f, 0.24f, 1.0f);
    glBegin(GL_POINTS);
    for (const SkeletonBone& bone : preview.skeleton) {
        if (bone.id != preview.selectedBone) {
            glVertex3f(bone.worldPosition.x, bone.worldPosition.y, bone.worldPosition.z);
        }
    }
    glEnd();

    if (preview.selectedBone >= 0 && preview.selectedBone < static_cast<int>(preview.skeleton.size())) {
        glPointSize(8.0f);
        glColor4f(1.0f, 0.08f, 0.04f, 1.0f);
        glBegin(GL_POINTS);
        const Vec3 selected = preview.skeleton[preview.selectedBone].worldPosition;
        glVertex3f(selected.x, selected.y, selected.z);
        glEnd();
    }
}

void RenderModelOpenGl(ModelPreview& preview, ImVec2 canvasMin, ImVec2 canvasMax) {
    if (preview.vertices.empty() || preview.triangles.empty()) {
        return;
    }

    UpdateModelSkinning(preview);
    const std::vector<ModelVertex>& renderVertices =
        preview.skinnedVertices.size() == preview.vertices.size() ? preview.skinnedVertices : preview.vertices;

    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scaleX = io.DisplayFramebufferScale.x;
    const float scaleY = io.DisplayFramebufferScale.y;

    int x = static_cast<int>((canvasMin.x - viewport->Pos.x) * scaleX);
    int y = static_cast<int>(static_cast<float>(gClientHeight) - (canvasMax.y - viewport->Pos.y) * scaleY);
    int width = static_cast<int>((canvasMax.x - canvasMin.x) * scaleX);
    int height = static_cast<int>((canvasMax.y - canvasMin.y) * scaleY);

    x = std::clamp(x, 0, gClientWidth);
    y = std::clamp(y, 0, gClientHeight);
    width = std::clamp(width, 0, gClientWidth - x);
    height = std::clamp(height, 0, gClientHeight - y);
    if (width <= 1 || height <= 1) {
        return;
    }

    GLint previousProgram = 0;
    if (gGlUseProgram != nullptr) {
        glGetIntegerv(kGlCurrentProgram, &previousProgram);
        gGlUseProgram(0);
    }

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_TEXTURE_BIT |
        GL_LIGHTING_BIT |
        GL_CURRENT_BIT |
        GL_VIEWPORT_BIT |
        GL_SCISSOR_BIT |
        GL_POLYGON_BIT |
        GL_LINE_BIT |
        GL_POINT_BIT);

    glViewport(x, y, width, height);
    glScissor(x, y, width, height);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    ResetSolidModelState();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.070f, 0.074f, 0.078f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float halfHeight = std::max(0.001f, preview.radius * 1.18f / std::max(preview.zoom, 0.001f));
    const float halfWidth = halfHeight * aspect;
    const float depth = std::max(1.0f, preview.radius * 4.0f);
    glOrtho(-halfWidth, halfWidth, -halfHeight, halfHeight, -depth, depth);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(preview.pitch * 57.2957795f, 1.0f, 0.0f, 0.0f);
    glRotatef(preview.yaw * 57.2957795f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-preview.center.x, -preview.center.y, -preview.center.z);

    for (const ModelSection& section : preview.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        GLuint textureId = 0;
        if (section.textureIndex < preview.materials.size()) {
            textureId = preview.materials[section.textureIndex].textureId;
        }

        ResetSolidModelState();
        if (textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureId);
            ApplyModelTextureTiling(section.textureTiling);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            if (section.alphaType == 0 || section.alphaType == 1 || section.alphaType == 4) {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.5f);
            }
        } else {
            const auto color = ModelMaterialColorFloats(section.textureIndex);
            glDisable(GL_TEXTURE_2D);
            glColor4f(color[0], color[1], color[2], color[3]);
        }

        glBegin(GL_TRIANGLES);
        const std::size_t end = std::min(section.triangleStart + section.triangleCount, preview.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = preview.triangles[triangleIndex];
            if (triangle.a >= renderVertices.size() ||
                triangle.b >= renderVertices.size() ||
                triangle.c >= renderVertices.size()) {
                continue;
            }
            EmitModelVertex(renderVertices[triangle.a], section.textureCoordinateIndex);
            EmitModelVertex(renderVertices[triangle.b], section.textureCoordinateIndex);
            EmitModelVertex(renderVertices[triangle.c], section.textureCoordinateIndex);
        }
        glEnd();
        ResetSolidModelState();
    }

    RenderModelSkeletonOpenGl(preview);

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    if (gGlUseProgram != nullptr) {
        gGlUseProgram(static_cast<GLuint>(previousProgram));
    }
}

void RenderModelDrawCallback(const ImDrawList*, const ImDrawCmd* command) {
    auto* request = static_cast<PendingModelRender*>(command->UserCallbackData);
    if (request == nullptr || request->preview == nullptr) {
        return;
    }
    RenderModelOpenGl(*request->preview, request->canvasMin, request->canvasMax);
}
