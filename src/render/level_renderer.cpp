struct PendingLevelRender {
    LevelPreview* preview = nullptr;
    ImVec2 canvasMin;
    ImVec2 canvasMax;
};

PendingLevelRender gPendingLevelRender;

std::array<float, 3> LevelTerrainColor(const LevelPreview& preview, float height) {
    const float span = std::max(0.001f, preview.boundsMax.y - preview.boundsMin.y);
    const float t = std::clamp((height - preview.boundsMin.y) / span, 0.0f, 1.0f);
    return {
        0.12f + 0.38f * t,
        0.30f + 0.42f * t,
        0.16f + 0.20f * t,
    };
}

bool IsCheckpointObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    return className == "ccheckpoint" || className.find("checkpoint") != std::string::npos;
}

std::string WorldObjectSearchText(const WorldObject& object) {
    std::string text = object.className + " " + object.name + " " + object.binding + " " +
                       object.assetPath + " " + object.modelPath;
    return ToLower(std::move(text));
}

bool IsStartPositionObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    const std::string path = ToLower(object.modelPath.empty() ? object.assetPath : object.modelPath);
    const std::string objectText = WorldObjectSearchText(object);
    return className == "cracestartpos" ||
           className == "cfoyerstartpos" ||
           className.find("startpos") != std::string::npos ||
           className.find("racestart") != std::string::npos ||
           path.find("cpfloater.md2") != std::string::npos ||
           objectText.find("cpfloater") != std::string::npos;
}

bool IsWeaponPickupObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    const std::string path = ToLower(object.modelPath.empty() ? object.assetPath : object.modelPath);
    return className == "cweaponpickup" ||
           className.find("weapon") != std::string::npos ||
           path.find("\\weapons\\") != std::string::npos ||
           path.find("weapon") != std::string::npos;
}

bool IsAiObject(const WorldObject& object) {
    const std::string className = ToLower(object.className);
    return className == "caisection" ||
           className == "caiquad" ||
           className == "cspline" ||
           className.rfind("cai", 0) == 0 ||
           className.find("ai") != std::string::npos;
}

bool HasActiveTerrainSection(const LevelPreview& preview) {
    return preview.selectedTerrainSection >= 0 &&
           preview.selectedTerrainSection < static_cast<int>(preview.terrainSections.size());
}

const LevelTerrainSection* ActiveTerrainSection(const LevelPreview& preview) {
    if (!HasActiveTerrainSection(preview)) {
        return nullptr;
    }
    return &preview.terrainSections[static_cast<std::size_t>(preview.selectedTerrainSection)];
}

std::uint32_t WrlSublevelLayerKey(std::uint32_t layer) {
    return layer & 0x00000fe0U;
}

bool WrlLayersMatchSublevel(std::uint32_t terrainLayer, std::uint32_t objectLayer) {
    if (terrainLayer == objectLayer) {
        return true;
    }

    const std::uint32_t terrainKey = WrlSublevelLayerKey(terrainLayer);
    const std::uint32_t objectKey = WrlSublevelLayerKey(objectLayer);
    return terrainKey != 0 && terrainKey == objectKey;
}

bool IsLayerVisibleForActiveSublevel(const LevelPreview& preview, bool hasLayer, std::uint32_t layer) {
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section == nullptr || !section->hasLayer) {
        return true;
    }
    return hasLayer && WrlLayersMatchSublevel(section->layer, layer);
}

bool IsObjectInActiveSublevel(const LevelPreview& preview, const WorldObject& object) {
    return IsLayerVisibleForActiveSublevel(preview, object.hasLayer, object.layer);
}

bool IsTerrainSectionVisibleForActiveSublevel(const LevelPreview& preview, std::size_t sectionIndex) {
    if (!HasActiveTerrainSection(preview)) {
        return true;
    }
    return sectionIndex == static_cast<std::size_t>(preview.selectedTerrainSection);
}

std::array<float, 4> LevelObjectColor(const WorldObject& object, bool selected) {
    if (selected) {
        return {1.0f, 0.90f, 0.14f, 1.0f};
    }

    const std::string className = ToLower(object.className);
    if (IsStartPositionObject(object)) {
        return {0.35f, 1.0f, 0.38f, 1.0f};
    }
    if (className.find("checkpoint") != std::string::npos) {
        return {1.0f, 0.32f, 0.20f, 1.0f};
    }
    if (className.find("pickup") != std::string::npos ||
        className.find("bonus") != std::string::npos ||
        className.find("weapon") != std::string::npos) {
        return {0.92f, 0.42f, 1.0f, 1.0f};
    }
    if (IsAiObject(object)) {
        return {0.42f, 0.78f, 1.0f, 1.0f};
    }
    if (className.find("water") != std::string::npos) {
        return {0.22f, 0.80f, 1.0f, 1.0f};
    }
    if (className.find("camera") != std::string::npos) {
        return {0.96f, 0.82f, 0.30f, 1.0f};
    }
    if (className.find("static") != std::string::npos ||
        className.find("mobile") != std::string::npos) {
        return {0.54f, 0.82f, 1.0f, 1.0f};
    }
    return {0.78f, 0.82f, 0.86f, 1.0f};
}

void EmitLevelVertex(const LevelPreview& preview,
                     const LevelVertex& vertex,
                     bool textured,
                     float textureScaleX = 1.0f,
                     float textureScaleY = 1.0f) {
    if (textured) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    } else {
        const auto color = LevelTerrainColor(preview, vertex.position.y);
        glColor4f(color[0], color[1], color[2], 1.0f);
    }
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitLevelVertexLayer(const LevelVertex& vertex,
                          int layer,
                          float textureScaleX,
                          float textureScaleY) {
    glColor4ub(255, 255, 255, vertex.mix[static_cast<std::size_t>(layer)]);
    glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

void EmitLevelVertexTerrainShader(const LevelVertex& vertex,
                                  float textureScaleX,
                                  float textureScaleY) {
    glColor4ub(vertex.mix[0], vertex.mix[1], vertex.mix[2], vertex.mix[3]);
    glTexCoord2f(vertex.u * textureScaleX, vertex.v * textureScaleY);
    glNormal3f(vertex.normal.x, vertex.normal.y, vertex.normal.z);
    glVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
}

bool HasWhirledTerrainTextures(const LevelTerrainSection& section) {
    return std::any_of(section.layerTextureIds.begin(), section.layerTextureIds.end(), [](GLuint textureId) {
        return textureId != 0;
    });
}

std::vector<std::uint32_t> CollectTerrainMaterialKeys(const std::vector<LevelTriangle>& triangles) {
    std::vector<std::uint32_t> keys;
    for (const LevelTriangle& triangle : triangles) {
        if (std::find(keys.begin(), keys.end(), triangle.materialKey) == keys.end()) {
            keys.push_back(triangle.materialKey);
        }
    }
    return keys;
}

GLuint BuildLevelTerrainDisplayList(const LevelPreview& preview,
                                    const std::vector<LevelVertex>& vertices,
                                    const std::vector<LevelTriangle>& triangles,
                                    bool textured,
                                    float textureScaleX,
                                    float textureScaleY) {
    if (vertices.empty() || triangles.empty()) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    glNewList(displayList, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : triangles) {
        if (triangle.a >= vertices.size() ||
            triangle.b >= vertices.size() ||
            triangle.c >= vertices.size()) {
            continue;
        }
        EmitLevelVertex(preview, vertices[triangle.a], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.b], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.c], textured, textureScaleX, textureScaleY);
    }
    glEnd();
    glEndList();
    return displayList;
}

GLuint BuildWhirledTerrainFixedDisplayList(const LevelTerrainSection& section) {
    if (section.vertices.empty() || section.triangles.empty() || !HasWhirledTerrainTextures(section)) {
        return 0;
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    (void)BlackTextureId();
    const std::vector<std::uint32_t> materialKeys = CollectTerrainMaterialKeys(section.triangles);
    glNewList(displayList, GL_COMPILE);
    glDisable(GL_CULL_FACE);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : section.triangles) {
        if (triangle.a >= section.vertices.size() ||
            triangle.b >= section.vertices.size() ||
            triangle.c >= section.vertices.size()) {
            continue;
        }
        const LevelVertex& a = section.vertices[triangle.a];
        const LevelVertex& b = section.vertices[triangle.b];
        const LevelVertex& c = section.vertices[triangle.c];
        glNormal3f(a.normal.x, a.normal.y, a.normal.z);
        glVertex3f(a.position.x, a.position.y, a.position.z);
        glNormal3f(b.normal.x, b.normal.y, b.normal.z);
        glVertex3f(b.position.x, b.position.y, b.position.z);
        glNormal3f(c.normal.x, c.normal.y, c.normal.z);
        glVertex3f(c.position.x, c.position.y, c.position.z);
    }
    glEnd();

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    for (std::uint32_t materialKey : materialKeys) {
        for (int layer = 0; layer < 4; ++layer) {
            const auto textureIndex = static_cast<unsigned char>((materialKey >> (layer * 8)) & 0xffU);
            if (textureIndex == 0xffU || section.layerTextureIds[textureIndex] == 0) {
                continue;
            }

            glBindTexture(GL_TEXTURE_2D, section.layerTextureIds[textureIndex]);
            glBegin(GL_TRIANGLES);
            for (const LevelTriangle& triangle : section.triangles) {
                if (triangle.materialKey != materialKey ||
                    triangle.a >= section.vertices.size() ||
                    triangle.b >= section.vertices.size() ||
                    triangle.c >= section.vertices.size()) {
                    continue;
                }
                EmitLevelVertexLayer(section.vertices[triangle.a], layer, section.textureScaleX, section.textureScaleY);
                EmitLevelVertexLayer(section.vertices[triangle.b], layer, section.textureScaleX, section.textureScaleY);
                EmitLevelVertexLayer(section.vertices[triangle.c], layer, section.textureScaleX, section.textureScaleY);
            }
            glEnd();
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEndList();
    return displayList;
}

void BindTerrainMaterialTextures(const LevelTerrainSection& section, std::uint32_t materialKey) {
    const GLuint blackTexture = BlackTextureId();
    for (int layer = 0; layer < 4; ++layer) {
        GLuint textureId = blackTexture;
        const auto textureIndex = static_cast<unsigned char>((materialKey >> (layer * 8)) & 0xffU);
        if (textureIndex != 0xffU && section.layerTextureIds[textureIndex] != 0) {
            textureId = section.layerTextureIds[textureIndex];
        }
        gGlActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(layer));
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
    gGlActiveTexture(GL_TEXTURE0);
}

void ResetTerrainMaterialTextures() {
    if (gGlActiveTexture == nullptr) {
        return;
    }
    for (int layer = 3; layer >= 1; --layer) {
        gGlActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(layer));
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
    gGlActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BuildWhirledTerrainDisplayList(const LevelTerrainSection& section) {
    if (section.vertices.empty() || section.triangles.empty() || !HasWhirledTerrainTextures(section)) {
        return 0;
    }
    if (EnsureTerrainShaderProgram() == 0 || gGlActiveTexture == nullptr) {
        return BuildWhirledTerrainFixedDisplayList(section);
    }

    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    const std::vector<std::uint32_t> materialKeys = CollectTerrainMaterialKeys(section.triangles);
    glNewList(displayList, GL_COMPILE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    for (std::uint32_t materialKey : materialKeys) {
        BindTerrainMaterialTextures(section, materialKey);
        glBegin(GL_TRIANGLES);
        for (const LevelTriangle& triangle : section.triangles) {
            if (triangle.materialKey != materialKey ||
                triangle.a >= section.vertices.size() ||
                triangle.b >= section.vertices.size() ||
                triangle.c >= section.vertices.size()) {
                continue;
            }
            EmitLevelVertexTerrainShader(section.vertices[triangle.a], section.textureScaleX, section.textureScaleY);
            EmitLevelVertexTerrainShader(section.vertices[triangle.b], section.textureScaleX, section.textureScaleY);
            EmitLevelVertexTerrainShader(section.vertices[triangle.c], section.textureScaleX, section.textureScaleY);
        }
        glEnd();
    }
    ResetTerrainMaterialTextures();
    glEndList();
    return displayList;
}

void RenderLevelTriangleList(const LevelPreview& preview,
                             const std::vector<LevelVertex>& vertices,
                             const std::vector<LevelTriangle>& triangles,
                             GLuint textureId,
                             const DecodedTexture& texture,
                             float textureScaleX,
                             float textureScaleY) {
    const bool textured =
        preview.showTerrainTexture &&
        textureId != 0 &&
        texture.width > 0 &&
        texture.height > 0;
    if (textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
    } else {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBegin(GL_TRIANGLES);
    for (const LevelTriangle& triangle : triangles) {
        if (triangle.a >= vertices.size() ||
            triangle.b >= vertices.size() ||
            triangle.c >= vertices.size()) {
            continue;
        }
        EmitLevelVertex(preview, vertices[triangle.a], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.b], textured, textureScaleX, textureScaleY);
        EmitLevelVertex(preview, vertices[triangle.c], textured, textureScaleX, textureScaleY);
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderLevelTerrainSection(LevelPreview& preview, LevelTerrainSection& section) {
    const bool whirledTextured = preview.showTerrainTexture && HasWhirledTerrainTextures(section);
    if (whirledTextured) {
        const GLuint terrainShader = EnsureTerrainShaderProgram();
        const bool useTerrainShader = terrainShader != 0 && gGlActiveTexture != nullptr;
        if (section.displayListTextured == 0) {
            section.displayListTextured = BuildWhirledTerrainDisplayList(section);
        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_LIGHTING);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if (useTerrainShader) {
            gGlUseProgram(terrainShader);
            for (int layer = 0; layer < 4; ++layer) {
                if (gTerrainTextureUniforms[layer] >= 0) {
                    gGlUniform1i(gTerrainTextureUniforms[layer], layer);
                }
            }
            if (gTerrainLightDirUniform >= 0) {
                gGlUniform3f(gTerrainLightDirUniform, 0.0f, 0.70710677f, 0.70710677f);
            }
            if (gTerrainAmbientUniform >= 0) {
                gGlUniform1f(gTerrainAmbientUniform, 0.48f);
            }
        }
        if (section.displayListTextured != 0) {
            glCallList(section.displayListTextured);
        }
        if (useTerrainShader) {
            ResetTerrainMaterialTextures();
            gGlUseProgram(0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    const bool textured =
        preview.showTerrainTexture &&
        section.textureId != 0 &&
        section.texture.width > 0 &&
        section.texture.height > 0;
    if (textured) {
        if (section.displayListTextured == 0) {
            section.displayListTextured = BuildLevelTerrainDisplayList(
                preview,
                section.vertices,
                section.triangles,
                true,
                section.textureScaleX,
                section.textureScaleY);
        }
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, section.textureId);
        if (section.displayListTextured != 0) {
            glCallList(section.displayListTextured);
        }
    } else {
        if (section.displayListColored == 0) {
            section.displayListColored = BuildLevelTerrainDisplayList(
                preview,
                section.vertices,
                section.triangles,
                false,
                1.0f,
                1.0f);
        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (section.displayListColored != 0) {
            glCallList(section.displayListColored);
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MultLevelTransform(Vec3 position, Quat rotation, Vec3 scale) {
    rotation = Normalize(rotation);
    if (Length(Vec3{rotation.x, rotation.y, rotation.z}) <= 0.00001f && std::abs(rotation.w) <= 0.00001f) {
        rotation = {};
    }

    const float x = rotation.x;
    const float y = rotation.y;
    const float z = rotation.z;
    const float w = rotation.w;
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    const float r00 = 1.0f - 2.0f * (yy + zz);
    const float r01 = 2.0f * (xy - wz);
    const float r02 = 2.0f * (xz + wy);
    const float r10 = 2.0f * (xy + wz);
    const float r11 = 1.0f - 2.0f * (xx + zz);
    const float r12 = 2.0f * (yz - wx);
    const float r20 = 2.0f * (xz - wy);
    const float r21 = 2.0f * (yz + wx);
    const float r22 = 1.0f - 2.0f * (xx + yy);

    const GLfloat matrix[16] = {
        r00 * scale.x, r10 * scale.x, r20 * scale.x, 0.0f,
        r01 * scale.y, r11 * scale.y, r21 * scale.y, 0.0f,
        r02 * scale.z, r12 * scale.z, r22 * scale.z, 0.0f,
        position.x, position.y, position.z, 1.0f,
    };
    glMultMatrixf(matrix);
}

bool IsLevelModelInstanceHidden(const LevelPreview& preview, const LevelModelInstance& instance) {
    if (instance.objectIndex < 0 || instance.objectIndex >= static_cast<int>(preview.objects.size())) {
        return false;
    }
    const WorldObject& object = preview.objects[instance.objectIndex];
    if (!IsObjectInActiveSublevel(preview, object)) {
        return true;
    }
    return (!preview.showCheckpoints && IsCheckpointObject(object)) ||
           (!preview.showStartPositions && IsStartPositionObject(object)) ||
           (!preview.showWeaponPickups && IsWeaponPickupObject(object)) ||
           (!preview.showAiObjects && IsAiObject(object));
}

void RenderLevelModelsOpenGl(LevelPreview& preview) {
    if (!preview.showObjects || preview.modelInstances.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    LevelModelInstance* selectedInstance = nullptr;
    LevelModelAsset* selectedAsset = nullptr;

    for (LevelModelInstance& instance : preview.modelInstances) {
        if (instance.assetIndex < 0 || instance.assetIndex >= static_cast<int>(preview.modelAssets.size())) {
            continue;
        }
        if (IsLevelModelInstanceHidden(preview, instance)) {
            continue;
        }

        LevelModelAsset& asset = preview.modelAssets[instance.assetIndex];
        if (!asset.loaded || asset.model.vertices.empty() || asset.model.triangles.empty()) {
            continue;
        }
        if (asset.displayList == 0) {
            asset.displayList = BuildModelDisplayList(asset.model, false);
        }
        if (asset.displayList == 0) {
            continue;
        }

        glPushMatrix();
        MultLevelTransform(instance.position, instance.rotation, instance.scale);
        glCallList(asset.displayList);
        glPopMatrix();

        if (instance.objectIndex == preview.selectedObject) {
            selectedInstance = &instance;
            selectedAsset = &asset;
        }
    }

    if (selectedInstance != nullptr && selectedAsset != nullptr) {
        glPushMatrix();
        MultLevelTransform(selectedInstance->position, selectedInstance->rotation, selectedInstance->scale);
        RenderModelSelectionHighlight(selectedAsset->model);
        glPopMatrix();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint BuildWaterSheetDisplayList(const LevelWaterSheet& waterSheet) {
    const GLuint displayList = glGenLists(1);
    if (displayList == 0) {
        return 0;
    }

    const float halfWidth = waterSheet.width * 0.5f;
    const float halfDepth = waterSheet.depth * 0.5f;
    glNewList(displayList, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, 0.0f);
    glVertex3f(halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, waterSheet.vScale);
    glVertex3f(halfWidth, 0.0f, halfDepth);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-halfWidth, 0.0f, -halfDepth);
    glTexCoord2f(waterSheet.uScale, waterSheet.vScale);
    glVertex3f(halfWidth, 0.0f, halfDepth);
    glTexCoord2f(0.0f, waterSheet.vScale);
    glVertex3f(-halfWidth, 0.0f, halfDepth);
    glEnd();
    glEndList();
    return displayList;
}

void RenderLevelWaterOpenGl(LevelPreview& preview) {
    if (!preview.showWater || preview.waterSheets.empty()) {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (LevelWaterSheet& waterSheet : preview.waterSheets) {
        if (waterSheet.objectIndex >= 0 && waterSheet.objectIndex < static_cast<int>(preview.objects.size()) &&
            !IsObjectInActiveSublevel(preview, preview.objects[waterSheet.objectIndex])) {
            continue;
        }
        if (waterSheet.displayList == 0) {
            waterSheet.displayList = BuildWaterSheetDisplayList(waterSheet);
        }
        if (waterSheet.displayList == 0) {
            continue;
        }

        if (waterSheet.textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, waterSheet.textureId);
            glColor4f(1.0f, 1.0f, 1.0f, waterSheet.alpha);
        } else {
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            glColor4f(0.26f, 0.58f, 0.82f, 0.58f);
        }

        glPushMatrix();
        MultLevelTransform(waterSheet.position, waterSheet.rotation, {1.0f, 1.0f, 1.0f});
        glCallList(waterSheet.displayList);
        glPopMatrix();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void RenderLevelSkyboxesOpenGl(LevelPreview& preview) {
    if (!preview.showSkybox || preview.skyboxAssetIndices.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);

    for (int assetIndex : preview.skyboxAssetIndices) {
        if (assetIndex < 0 || assetIndex >= static_cast<int>(preview.modelAssets.size())) {
            continue;
        }

        LevelModelAsset& asset = preview.modelAssets[assetIndex];
        if (!asset.loaded || asset.model.vertices.empty() || asset.model.triangles.empty()) {
            continue;
        }
        if (asset.skyboxDisplayList == 0) {
            asset.skyboxDisplayList = BuildModelDisplayList(asset.model, true);
        }
        if (asset.skyboxDisplayList == 0) {
            continue;
        }

        const float modelRadius = std::max(1.0f, asset.model.radius);
        const float targetRadius = std::max(512.0f, preview.radius * 2.75f);
        const float scale = targetRadius / modelRadius;
        glPushMatrix();
        glScalef(scale, scale, scale);
        glCallList(asset.skyboxDisplayList);
        glPopMatrix();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void RenderLevelObjectsOpenGl(const LevelPreview& preview) {
    if (!preview.showObjects ||
        preview.objects.empty() ||
        (!preview.showObjectMarkers && preview.selectedObject < 0)) {
        return;
    }

    const float markerHeight = std::clamp(preview.radius * 0.025f, 4.0f, 36.0f);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glPointSize(5.0f);

    glBegin(GL_LINES);
    for (std::size_t index = 0; index < preview.objects.size(); ++index) {
        const WorldObject& object = preview.objects[index];
        if (!IsObjectInActiveSublevel(preview, object)) {
            continue;
        }
        if (!object.hasPosition) {
            continue;
        }
        if (!preview.showCheckpoints && IsCheckpointObject(object)) {
            continue;
        }
        const bool startPosition = IsStartPositionObject(object);
        const bool weaponPickup = IsWeaponPickupObject(object);
        const bool aiObject = IsAiObject(object);
        const bool selected = static_cast<int>(index) == preview.selectedObject;
        if (!selected && startPosition && !preview.showStartPositions) {
            continue;
        }
        if (!selected && weaponPickup && !preview.showWeaponPickups) {
            continue;
        }
        if (!selected && aiObject && !preview.showAiObjects) {
            continue;
        }
        if (!selected && !preview.showObjectMarkers) {
            continue;
        }
        if (!selected && !object.modelPath.empty()) {
            continue;
        }
        const auto color = LevelObjectColor(object, selected);
        glColor4f(color[0], color[1], color[2], selected ? 1.0f : 0.70f);
        glVertex3f(object.position.x, object.position.y, object.position.z);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
    }
    glEnd();

    glBegin(GL_POINTS);
    for (std::size_t index = 0; index < preview.objects.size(); ++index) {
        const WorldObject& object = preview.objects[index];
        if (!IsObjectInActiveSublevel(preview, object)) {
            continue;
        }
        if (!object.hasPosition) {
            continue;
        }
        if (!preview.showCheckpoints && IsCheckpointObject(object)) {
            continue;
        }
        const bool startPosition = IsStartPositionObject(object);
        const bool weaponPickup = IsWeaponPickupObject(object);
        const bool aiObject = IsAiObject(object);
        const bool selected = static_cast<int>(index) == preview.selectedObject;
        if (!selected && startPosition && !preview.showStartPositions) {
            continue;
        }
        if (!selected && weaponPickup && !preview.showWeaponPickups) {
            continue;
        }
        if (!selected && aiObject && !preview.showAiObjects) {
            continue;
        }
        if (!selected && !preview.showObjectMarkers) {
            continue;
        }
        if (!selected && !object.modelPath.empty()) {
            continue;
        }
        const auto color = LevelObjectColor(object, selected);
        glColor4f(color[0], color[1], color[2], 1.0f);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
    }
    glEnd();

    if (preview.selectedObject >= 0 &&
        preview.selectedObject < static_cast<int>(preview.objects.size()) &&
        preview.objects[preview.selectedObject].hasPosition &&
        IsObjectInActiveSublevel(preview, preview.objects[preview.selectedObject])) {
        const WorldObject& object = preview.objects[preview.selectedObject];
        glPointSize(10.0f);
        glColor4f(1.0f, 0.96f, 0.12f, 1.0f);
        glBegin(GL_POINTS);
        glVertex3f(object.position.x, object.position.y + markerHeight, object.position.z);
        glEnd();
    }
}

bool ComputeActiveSublevelBounds(const LevelPreview& preview, Vec3& boundsMin, Vec3& boundsMax) {
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section == nullptr) {
        return false;
    }

    bool initialized = false;
    auto include = [&](Vec3 point) {
        if (!initialized) {
            boundsMin = point;
            boundsMax = point;
            initialized = true;
            return;
        }
        boundsMin.x = std::min(boundsMin.x, point.x);
        boundsMin.y = std::min(boundsMin.y, point.y);
        boundsMin.z = std::min(boundsMin.z, point.z);
        boundsMax.x = std::max(boundsMax.x, point.x);
        boundsMax.y = std::max(boundsMax.y, point.y);
        boundsMax.z = std::max(boundsMax.z, point.z);
    };

    for (const LevelVertex& vertex : section->vertices) {
        include(vertex.position);
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition && IsObjectInActiveSublevel(preview, object)) {
            include(object.position);
        }
    }
    return initialized;
}

Vec3 ActiveLevelCenter(const LevelPreview& preview) {
    Vec3 boundsMin{};
    Vec3 boundsMax{};
    if (ComputeActiveSublevelBounds(preview, boundsMin, boundsMax)) {
        return {
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f,
        };
    }
    return preview.center;
}

float ActiveLevelRadius(const LevelPreview& preview) {
    Vec3 boundsMin{};
    Vec3 boundsMax{};
    if (!ComputeActiveSublevelBounds(preview, boundsMin, boundsMax)) {
        return preview.radius;
    }

    const Vec3 center{
        (boundsMin.x + boundsMax.x) * 0.5f,
        (boundsMin.y + boundsMax.y) * 0.5f,
        (boundsMin.z + boundsMax.z) * 0.5f,
    };
    float radius = 0.0f;
    const LevelTerrainSection* section = ActiveTerrainSection(preview);
    if (section != nullptr) {
        for (const LevelVertex& vertex : section->vertices) {
            radius = std::max(radius, Length(Subtract(vertex.position, center)));
        }
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition && IsObjectInActiveSublevel(preview, object)) {
            radius = std::max(radius, Length(Subtract(object.position, center)));
        }
    }
    return radius > 0.0001f && std::isfinite(radius) ? radius : preview.radius;
}

Vec3 LevelDisplayCenter(const LevelPreview& preview) {
    const Vec3 center = ActiveLevelCenter(preview);
    return {-center.x, center.y, center.z};
}

Vec3 LevelCameraForward(const LevelPreview& preview) {
    const float cp = std::cos(preview.pitch);
    return Normalize(Vec3{
        -std::sin(preview.yaw) * cp,
        std::sin(preview.pitch),
        -std::cos(preview.yaw) * cp,
    });
}

Vec3 LevelCameraRight(const LevelPreview& preview) {
    return Normalize(Vec3{std::cos(preview.yaw), 0.0f, -std::sin(preview.yaw)});
}

void EnsureLevelFlyCamera(LevelPreview& preview);

struct LevelPickRay {
    Vec3 origin;
    Vec3 direction;
};

LevelPickRay BuildLevelPickRay(const LevelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    const float normalizedX =
        ((mouse.x - canvasMin.x) / std::max(1.0f, canvasSize.x)) * 2.0f - 1.0f;
    const float normalizedY =
        1.0f - ((mouse.y - canvasMin.y) / std::max(1.0f, canvasSize.y)) * 2.0f;
    const float aspect = std::max(0.001f, canvasSize.x / std::max(1.0f, canvasSize.y));
    const float tanHalfFov = std::tan(60.0f * 0.01745329252f * 0.5f);
    const Vec3 forward = LevelCameraForward(preview);
    const Vec3 right = LevelCameraRight(preview);
    Vec3 up = Normalize(Cross(right, forward));
    if (Length(up) <= 0.0001f) {
        up = {0.0f, 1.0f, 0.0f};
    }

    return {
        preview.flyCameraPosition,
        Normalize(Add(
            Add(forward, Multiply(right, normalizedX * tanHalfFov * aspect)),
            Multiply(up, normalizedY * tanHalfFov))),
    };
}

bool RayIntersectsAabb(Vec3 origin, Vec3 direction, Vec3 boundsMin, Vec3 boundsMax, float& hitDistance) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float originAxis = GetAxis(origin, axis);
        const float directionAxis = GetAxis(direction, axis);
        const float minAxis = GetAxis(boundsMin, axis);
        const float maxAxis = GetAxis(boundsMax, axis);
        if (std::abs(directionAxis) <= 0.000001f) {
            if (originAxis < minAxis || originAxis > maxAxis) {
                return false;
            }
            continue;
        }

        float t1 = (minAxis - originAxis) / directionAxis;
        float t2 = (maxAxis - originAxis) / directionAxis;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return false;
        }
    }

    hitDistance = tMin >= 0.0f ? tMin : tMax;
    return hitDistance >= 0.0f && std::isfinite(hitDistance);
}

bool ComputeLevelModelDisplayBounds(const LevelModelInstance& instance,
                                    const ModelPreview& model,
                                    Vec3& boundsMin,
                                    Vec3& boundsMax) {
    if (model.vertices.empty() || model.triangles.empty()) {
        return false;
    }

    const Vec3 min = model.boundsMin;
    const Vec3 max = model.boundsMax;
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

    bool initialized = false;
    for (Vec3 corner : corners) {
        const Vec3 displayPoint = LevelWorldToDisplay(TransformModelPointToWorld(instance, corner));
        if (!initialized) {
            boundsMin = displayPoint;
            boundsMax = displayPoint;
            initialized = true;
            continue;
        }
        boundsMin.x = std::min(boundsMin.x, displayPoint.x);
        boundsMin.y = std::min(boundsMin.y, displayPoint.y);
        boundsMin.z = std::min(boundsMin.z, displayPoint.z);
        boundsMax.x = std::max(boundsMax.x, displayPoint.x);
        boundsMax.y = std::max(boundsMax.y, displayPoint.y);
        boundsMax.z = std::max(boundsMax.z, displayPoint.z);
    }
    return initialized;
}

int PickLevelModelObject(LevelPreview& preview, ImVec2 mouse, ImVec2 canvasMin, ImVec2 canvasSize) {
    if (!preview.showObjects || preview.modelInstances.empty()) {
        return -1;
    }

    EnsureLevelFlyCamera(preview);
    const LevelPickRay ray = BuildLevelPickRay(preview, mouse, canvasMin, canvasSize);
    if (Length(ray.direction) <= 0.0001f) {
        return -1;
    }

    float bestDistance = std::numeric_limits<float>::max();
    int bestObject = -1;
    for (const LevelModelInstance& instance : preview.modelInstances) {
        if (instance.objectIndex < 0 ||
            instance.objectIndex >= static_cast<int>(preview.objects.size()) ||
            instance.assetIndex < 0 ||
            instance.assetIndex >= static_cast<int>(preview.modelAssets.size()) ||
            IsLevelModelInstanceHidden(preview, instance)) {
            continue;
        }

        const LevelModelAsset& asset = preview.modelAssets[instance.assetIndex];
        if (!asset.loaded) {
            continue;
        }

        Vec3 boundsMin{};
        Vec3 boundsMax{};
        if (!ComputeLevelModelDisplayBounds(instance, asset.model, boundsMin, boundsMax)) {
            continue;
        }

        float hitDistance = 0.0f;
        if (RayIntersectsAabb(ray.origin, ray.direction, boundsMin, boundsMax, hitDistance) &&
            hitDistance < bestDistance) {
            bestDistance = hitDistance;
            bestObject = instance.objectIndex;
        }
    }

    return bestObject;
}

void EnsureLevelFlyCamera(LevelPreview& preview) {
    if (preview.flyCameraInitialized) {
        return;
    }

    preview.yaw = 0.0f;
    preview.pitch = -0.22f;
    const Vec3 center = LevelDisplayCenter(preview);
    const Vec3 forward = LevelCameraForward(preview);
    const float activeRadius = ActiveLevelRadius(preview);
    const float distance = std::max(48.0f, activeRadius * 1.45f);
    preview.flyCameraPosition = Subtract(center, Multiply(forward, distance));
    preview.flySpeed = std::max(12.0f, activeRadius * 0.42f);
    preview.flyCameraInitialized = true;
}

void LoadPerspectiveProjection(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float top = std::tan(fovYRadians * 0.5f) * nearPlane;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void RenderLevelOpenGl(LevelPreview& preview, ImVec2 canvasMin, ImVec2 canvasMax) {
    if (preview.vertices.empty() && preview.objects.empty()) {
        return;
    }
    EnsureLevelFlyCamera(preview);

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
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.070f, 0.074f, 0.078f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float nearPlane = std::max(0.08f, preview.radius * 0.0005f);
    const float farPlane = std::max(4096.0f, preview.radius * 40.0f);
    LoadPerspectiveProjection(60.0f * 0.01745329252f, aspect, nearPlane, farPlane);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef(-preview.pitch * 57.2957795f, 1.0f, 0.0f, 0.0f);
    glRotatef(-preview.yaw * 57.2957795f, 0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glScalef(-1.0f, 1.0f, 1.0f);
    RenderLevelSkyboxesOpenGl(preview);
    glPopMatrix();
    glTranslatef(-preview.flyCameraPosition.x, -preview.flyCameraPosition.y, -preview.flyCameraPosition.z);
    glScalef(-1.0f, 1.0f, 1.0f);

    if (preview.showTerrain) {
        if (!preview.terrainSections.empty()) {
            for (std::size_t sectionIndex = 0; sectionIndex < preview.terrainSections.size(); ++sectionIndex) {
                LevelTerrainSection& section = preview.terrainSections[sectionIndex];
                if (!IsTerrainSectionVisibleForActiveSublevel(preview, sectionIndex)) {
                    continue;
                }
                if (!section.show || section.vertices.empty() || section.triangles.empty()) {
                    continue;
                }
                RenderLevelTerrainSection(preview, section);
            }
        } else if (!preview.vertices.empty() && !preview.triangles.empty()) {
            RenderLevelTriangleList(
                preview,
                preview.vertices,
                preview.triangles,
                preview.terrainTextureId,
                preview.terrainTexture,
                1.0f,
                1.0f);
        }
    }

    RenderLevelModelsOpenGl(preview);
    RenderLevelWaterOpenGl(preview);
    RenderLevelObjectsOpenGl(preview);

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

void RenderLevelDrawCallback(const ImDrawList*, const ImDrawCmd* command) {
    auto* request = static_cast<PendingLevelRender*>(command->UserCallbackData);
    if (request == nullptr || request->preview == nullptr) {
        return;
    }
    RenderLevelOpenGl(*request->preview, request->canvasMin, request->canvasMax);
}

bool HasLevelTerrainTexture(const LevelPreview& preview) {
    if (preview.terrainTextureId != 0) {
        return true;
    }
    return std::any_of(preview.terrainSections.begin(), preview.terrainSections.end(), [](const LevelTerrainSection& section) {
        if (section.textureId != 0) {
            return true;
        }
        return std::any_of(section.layerTextureIds.begin(), section.layerTextureIds.end(), [](GLuint textureId) {
            return textureId != 0;
        });
    });
}

std::string LevelTerrainTextureSummary(const LevelPreview& preview) {
    if (!preview.terrainTexturePath.empty()) {
        return preview.terrainTexturePath;
    }

    int textureCount = 0;
    std::string firstTexture;
    int layerTextureCount = 0;
    std::string firstLayerTexture;
    for (const LevelTerrainSection& section : preview.terrainSections) {
        if (section.textureId != 0 && !section.texturePath.empty()) {
            ++textureCount;
            if (firstTexture.empty()) {
                firstTexture = section.texturePath;
            }
        }
        for (std::size_t index = 0; index < section.layerTextureIds.size(); ++index) {
            if (section.layerTextureIds[index] == 0 || section.layerTexturePaths[index].empty()) {
                continue;
            }
            ++layerTextureCount;
            if (firstLayerTexture.empty()) {
                firstLayerTexture = section.layerTexturePaths[index];
            }
        }
    }
    if (layerTextureCount > 0) {
        return std::to_string(layerTextureCount) + " Whirled terrain layer textures loaded. First: " + firstLayerTexture;
    }
    if (textureCount == 0) {
        return "No baked terrain texture loaded.";
    }
    if (textureCount == 1) {
        return firstTexture;
    }
    return std::to_string(textureCount) + " terrain textures loaded. First: " + firstTexture;
}
