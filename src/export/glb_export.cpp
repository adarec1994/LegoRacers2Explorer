std::string JsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const unsigned char character : value) {
        switch (character) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (character < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(character);
            } else {
                out << static_cast<char>(character);
            }
            break;
        }
    }
    return out.str();
}

struct GlbBufferView {
    std::size_t offset = 0;
    std::size_t length = 0;
    int target = 0;
};

struct GlbAccessor {
    int bufferView = -1;
    int componentType = 5126;
    int count = 0;
    std::string type;
    std::vector<float> minValues;
    std::vector<float> maxValues;
};

struct GlbBuild {
    std::vector<unsigned char> binary;
    std::vector<GlbBufferView> views;
    std::vector<GlbAccessor> accessors;
    std::vector<int> imageViews;
    std::vector<std::string> imageNames;
};

void PadBytes(std::vector<unsigned char>& bytes, unsigned char pad = 0) {
    while ((bytes.size() % 4U) != 0U) {
        bytes.push_back(pad);
    }
}

int AppendBufferView(GlbBuild& glb, const void* data, std::size_t byteCount, int target = 0) {
    PadBytes(glb.binary);
    const std::size_t offset = glb.binary.size();
    if (byteCount > 0) {
        const auto* first = static_cast<const unsigned char*>(data);
        glb.binary.insert(glb.binary.end(), first, first + byteCount);
    }
    PadBytes(glb.binary);
    const int viewIndex = static_cast<int>(glb.views.size());
    glb.views.push_back({offset, byteCount, target});
    return viewIndex;
}

template <typename T>
int AppendVectorBufferView(GlbBuild& glb, const std::vector<T>& values, int target = 0) {
    return AppendBufferView(glb, values.empty() ? nullptr : values.data(), values.size() * sizeof(T), target);
}

int AddAccessor(GlbBuild& glb,
                int bufferView,
                int componentType,
                int count,
                std::string type,
                std::vector<float> minValues = {},
                std::vector<float> maxValues = {}) {
    const int accessorIndex = static_cast<int>(glb.accessors.size());
    glb.accessors.push_back({
        bufferView,
        componentType,
        count,
        std::move(type),
        std::move(minValues),
        std::move(maxValues),
    });
    return accessorIndex;
}

std::array<float, 16> InverseBindMatrix(const SkeletonBone& bone) {
    const Quat q = Normalize(Inverse(bone.bindWorldRotation));
    const Vec3 t = Rotate(q, {-bone.bindWorldPosition.x, -bone.bindWorldPosition.y, -bone.bindWorldPosition.z});

    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    return {
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f,
        2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f,
        2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f,
        t.x, t.y, t.z, 1.0f,
    };
}

bool IsRootBone(const ModelPreview& model, std::size_t boneIndex) {
    const SkeletonBone& bone = model.skeleton[boneIndex];
    return bone.parent < 0 ||
           bone.parent >= static_cast<int>(model.skeleton.size()) ||
           bone.parent == static_cast<int>(boneIndex);
}

Vec3 ExportBoneLocalPosition(const ModelPreview& model, std::size_t boneIndex) {
    Vec3 localPosition = model.skeleton[boneIndex].localPosition;
    if (IsRootBone(model, boneIndex) && model.hasSkeletonWorldOffset) {
        localPosition = Add(localPosition, model.skeletonWorldOffset);
    }
    return localPosition;
}

void AppendGlbVertex(const ModelVertex& vertex,
                     unsigned char coordinateIndex,
                     std::vector<float>& positions,
                     std::vector<float>& normals,
                     std::vector<float>& texcoords,
                     std::vector<std::uint16_t>& joints,
                     std::vector<float>& weights,
                     bool skinned) {
    positions.push_back(vertex.bindPosition.x);
    positions.push_back(vertex.bindPosition.y);
    positions.push_back(vertex.bindPosition.z);
    normals.push_back(vertex.bindNormal.x);
    normals.push_back(vertex.bindNormal.y);
    normals.push_back(vertex.bindNormal.z);

    unsigned char uvSet = coordinateIndex;
    if (uvSet >= vertex.uvSetCount || uvSet >= vertex.uSets.size()) {
        uvSet = 0;
    }
    texcoords.push_back(vertex.uSets[uvSet]);
    texcoords.push_back(vertex.vSets[uvSet]);

    if (!skinned) {
        return;
    }

    std::array<std::uint16_t, 4> jointValues = {};
    std::array<float, 4> weightValues = {};
    for (int influenceIndex = 0; influenceIndex < vertex.skinInfluenceCount && influenceIndex < 4; ++influenceIndex) {
        const SkinInfluence& influence = vertex.skinInfluences[influenceIndex];
        if (influence.bone >= 0) {
            jointValues[static_cast<std::size_t>(influenceIndex)] = static_cast<std::uint16_t>(influence.bone);
            weightValues[static_cast<std::size_t>(influenceIndex)] = influence.weight;
        }
    }
    for (int index = 0; index < 4; ++index) {
        joints.push_back(jointValues[static_cast<std::size_t>(index)]);
    }
    for (int index = 0; index < 4; ++index) {
        weights.push_back(weightValues[static_cast<std::size_t>(index)]);
    }
}

std::string BuildGlbJson(const ModelPreview& model,
                         const GlbBuild& glb,
                         const std::vector<std::string>& primitiveJson,
                         const std::vector<int>& materialTextureIndices,
                         int skinIndex,
                         const std::vector<int>& boneNodeIndices,
                         const std::vector<std::string>& animationJson) {
    std::ostringstream json;
    json << "{";
    json << "\"asset\":{\"version\":\"2.0\",\"generator\":\"Lego Racers 2 Explorer\"},";
    json << "\"scene\":0,";
    json << "\"scenes\":[{\"nodes\":[0";
    for (std::size_t boneIndex = 0; boneIndex < model.skeleton.size(); ++boneIndex) {
        if (model.skeleton[boneIndex].parent < 0 && boneIndex < boneNodeIndices.size()) {
            json << "," << boneNodeIndices[boneIndex];
        }
    }
    json << "]}],";

    json << "\"nodes\":[";
    json << "{\"name\":\"" << JsonEscape(model.name) << "\",\"mesh\":0";
    if (skinIndex >= 0) {
        json << ",\"skin\":" << skinIndex;
    }
    json << "}";
    for (std::size_t boneIndex = 0; boneIndex < model.skeleton.size(); ++boneIndex) {
        const SkeletonBone& bone = model.skeleton[boneIndex];
        const Vec3 localPosition = ExportBoneLocalPosition(model, boneIndex);
        json << ",{\"name\":\"Bone_" << boneIndex << "\",";
        json << "\"translation\":[" << localPosition.x << "," << localPosition.y << "," << localPosition.z << "],";
        json << "\"rotation\":[" << bone.localRotation.x << "," << bone.localRotation.y << "," << bone.localRotation.z << "," << bone.localRotation.w << "]";
        bool hasChild = false;
        for (std::size_t childIndex = 0; childIndex < model.skeleton.size(); ++childIndex) {
            if (model.skeleton[childIndex].parent == static_cast<int>(boneIndex)) {
                if (!hasChild) {
                    json << ",\"children\":[";
                    hasChild = true;
                } else {
                    json << ",";
                }
                json << boneNodeIndices[childIndex];
            }
        }
        if (hasChild) {
            json << "]";
        }
        json << "}";
    }
    json << "],";

    json << "\"meshes\":[{\"name\":\"" << JsonEscape(model.name) << "\",\"primitives\":[";
    for (std::size_t index = 0; index < primitiveJson.size(); ++index) {
        if (index != 0) {
            json << ",";
        }
        json << primitiveJson[index];
    }
    json << "]}],";

    json << "\"buffers\":[{\"byteLength\":" << glb.binary.size() << "}],";
    json << "\"bufferViews\":[";
    for (std::size_t index = 0; index < glb.views.size(); ++index) {
        const GlbBufferView& view = glb.views[index];
        if (index != 0) {
            json << ",";
        }
        json << "{\"buffer\":0,\"byteOffset\":" << view.offset << ",\"byteLength\":" << view.length;
        if (view.target != 0) {
            json << ",\"target\":" << view.target;
        }
        json << "}";
    }
    json << "],";

    json << "\"accessors\":[";
    for (std::size_t index = 0; index < glb.accessors.size(); ++index) {
        const GlbAccessor& accessor = glb.accessors[index];
        if (index != 0) {
            json << ",";
        }
        json << "{\"bufferView\":" << accessor.bufferView
             << ",\"componentType\":" << accessor.componentType
             << ",\"count\":" << accessor.count
             << ",\"type\":\"" << accessor.type << "\"";
        if (!accessor.minValues.empty()) {
            json << ",\"min\":[";
            for (std::size_t valueIndex = 0; valueIndex < accessor.minValues.size(); ++valueIndex) {
                if (valueIndex != 0) {
                    json << ",";
                }
                json << accessor.minValues[valueIndex];
            }
            json << "]";
        }
        if (!accessor.maxValues.empty()) {
            json << ",\"max\":[";
            for (std::size_t valueIndex = 0; valueIndex < accessor.maxValues.size(); ++valueIndex) {
                if (valueIndex != 0) {
                    json << ",";
                }
                json << accessor.maxValues[valueIndex];
            }
            json << "]";
        }
        json << "}";
    }
    json << "]";

    if (!model.materials.empty()) {
        json << ",\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],";
        json << "\"materials\":[";
        for (std::size_t index = 0; index < model.materials.size(); ++index) {
            if (index != 0) {
                json << ",";
            }
            json << "{\"name\":\"" << JsonEscape(std::filesystem::path(model.materials[index].path).stem().string()) << "\",";
            json << "\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";
            if (index < materialTextureIndices.size() && materialTextureIndices[index] >= 0) {
                json << ",\"baseColorTexture\":{\"index\":" << materialTextureIndices[index] << "}";
            }
            json << "},\"doubleSided\":true}";
        }
        json << "]";
    }

    if (!glb.imageViews.empty()) {
        json << ",\"textures\":[";
        for (std::size_t index = 0; index < glb.imageViews.size(); ++index) {
            if (index != 0) {
                json << ",";
            }
            json << "{\"sampler\":0,\"source\":" << index << "}";
        }
        json << "],\"images\":[";
        for (std::size_t index = 0; index < glb.imageViews.size(); ++index) {
            if (index != 0) {
                json << ",";
            }
            json << "{\"name\":\"" << JsonEscape(glb.imageNames[index])
                 << "\",\"bufferView\":" << glb.imageViews[index]
                 << ",\"mimeType\":\"image/png\"}";
        }
        json << "]";
    }

    if (skinIndex >= 0) {
        json << ",\"skins\":[{\"joints\":[";
        for (std::size_t index = 0; index < boneNodeIndices.size(); ++index) {
            if (index != 0) {
                json << ",";
            }
            json << boneNodeIndices[index];
        }
        json << "],\"inverseBindMatrices\":" << skinIndex << "}]";
    }

    if (!animationJson.empty()) {
        json << ",\"animations\":[";
        for (std::size_t index = 0; index < animationJson.size(); ++index) {
            if (index != 0) {
                json << ",";
            }
            json << animationJson[index];
        }
        json << "]";
    }

    json << "}";
    return json.str();
}

void WriteGlbFile(const std::filesystem::path& path, const std::string& jsonText, const std::vector<unsigned char>& binary) {
    std::vector<unsigned char> jsonChunk(jsonText.begin(), jsonText.end());
    PadBytes(jsonChunk, 0x20);
    std::vector<unsigned char> binChunk = binary;
    PadBytes(binChunk);

    const std::uint32_t totalLength = static_cast<std::uint32_t>(
        12 + 8 + jsonChunk.size() + (binChunk.empty() ? 0 : 8 + binChunk.size()));

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Could not open GLB export file: " + path.string());
    }

    WritePod(file, std::uint32_t{0x46546C67});
    WritePod(file, std::uint32_t{2});
    WritePod(file, totalLength);
    WritePod(file, static_cast<std::uint32_t>(jsonChunk.size()));
    WritePod(file, std::uint32_t{0x4E4F534A});
    file.write(reinterpret_cast<const char*>(jsonChunk.data()), static_cast<std::streamsize>(jsonChunk.size()));
    if (!binChunk.empty()) {
        WritePod(file, static_cast<std::uint32_t>(binChunk.size()));
        WritePod(file, std::uint32_t{0x004E4942});
        file.write(reinterpret_cast<const char*>(binChunk.data()), static_cast<std::streamsize>(binChunk.size()));
    }
    if (!file) {
        throw std::runtime_error("Could not write GLB export file: " + path.string());
    }
}

void ExportModelGlb(const ModelPreview& model, const std::filesystem::path& requestedPath) {
    if (model.vertices.empty() || model.triangles.empty()) {
        throw std::runtime_error("Model has no mesh data to export.");
    }

    GlbBuild glb;
    std::vector<int> materialTextureIndices(model.materials.size(), -1);
    for (std::size_t materialIndex = 0; materialIndex < model.materials.size(); ++materialIndex) {
        const ModelMaterial& material = model.materials[materialIndex];
        if (!material.loaded || material.texture.rgba.empty()) {
            continue;
        }
        const std::vector<unsigned char> png = EncodeWicImageToMemory(material.texture, GUID_ContainerFormatPng);
        const int view = AppendBufferView(glb, png.data(), png.size());
        materialTextureIndices[materialIndex] = static_cast<int>(glb.imageViews.size());
        glb.imageViews.push_back(view);
        glb.imageNames.push_back(std::filesystem::path(material.path).filename().string());
    }

    const bool skinned = HasExportableSkinning(model);
    int skinAccessor = -1;
    std::vector<int> boneNodeIndices;
    if (skinned) {
        std::vector<float> inverseBindMatrices;
        inverseBindMatrices.reserve(model.skeleton.size() * 16U);
        for (const SkeletonBone& bone : model.skeleton) {
            const std::array<float, 16> matrix = InverseBindMatrix(bone);
            inverseBindMatrices.insert(inverseBindMatrices.end(), matrix.begin(), matrix.end());
        }
        const int view = AppendVectorBufferView(glb, inverseBindMatrices);
        skinAccessor = AddAccessor(glb, view, 5126, static_cast<int>(model.skeleton.size()), "MAT4");
        boneNodeIndices.reserve(model.skeleton.size());
        for (std::size_t index = 0; index < model.skeleton.size(); ++index) {
            boneNodeIndices.push_back(static_cast<int>(index) + 1);
        }
    }

    std::vector<std::string> primitiveJson;
    for (const ModelSection& section : model.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<std::uint16_t> joints;
        std::vector<float> weights;
        std::vector<std::uint32_t> indices;
        Vec3 minPosition{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        Vec3 maxPosition{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

        const std::size_t end = std::min(section.triangleStart + section.triangleCount, model.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = model.triangles[triangleIndex];
            const std::array<std::uint32_t, 3> triangleIndices = {triangle.a, triangle.b, triangle.c};
            for (const std::uint32_t modelVertexIndex : triangleIndices) {
                if (modelVertexIndex >= model.vertices.size()) {
                    continue;
                }
                const ModelVertex& vertex = model.vertices[modelVertexIndex];
                const std::uint32_t exportIndex = static_cast<std::uint32_t>(indices.size());
                indices.push_back(exportIndex);
                AppendGlbVertex(vertex, section.textureCoordinateIndex, positions, normals, texcoords, joints, weights, skinned);
                minPosition.x = std::min(minPosition.x, vertex.bindPosition.x);
                minPosition.y = std::min(minPosition.y, vertex.bindPosition.y);
                minPosition.z = std::min(minPosition.z, vertex.bindPosition.z);
                maxPosition.x = std::max(maxPosition.x, vertex.bindPosition.x);
                maxPosition.y = std::max(maxPosition.y, vertex.bindPosition.y);
                maxPosition.z = std::max(maxPosition.z, vertex.bindPosition.z);
            }
        }
        if (indices.empty()) {
            continue;
        }

        const int positionView = AppendVectorBufferView(glb, positions, 34962);
        const int normalView = AppendVectorBufferView(glb, normals, 34962);
        const int uvView = AppendVectorBufferView(glb, texcoords, 34962);
        const int indexView = AppendVectorBufferView(glb, indices, 34963);
        const int positionAccessor = AddAccessor(
            glb,
            positionView,
            5126,
            static_cast<int>(positions.size() / 3),
            "VEC3",
            {minPosition.x, minPosition.y, minPosition.z},
            {maxPosition.x, maxPosition.y, maxPosition.z});
        const int normalAccessor = AddAccessor(glb, normalView, 5126, static_cast<int>(normals.size() / 3), "VEC3");
        const int uvAccessor = AddAccessor(glb, uvView, 5126, static_cast<int>(texcoords.size() / 2), "VEC2");
        const int indexAccessor = AddAccessor(glb, indexView, 5125, static_cast<int>(indices.size()), "SCALAR");

        int jointsAccessor = -1;
        int weightsAccessor = -1;
        if (skinned && !joints.empty() && !weights.empty()) {
            const int jointsView = AppendVectorBufferView(glb, joints, 34962);
            const int weightsView = AppendVectorBufferView(glb, weights, 34962);
            jointsAccessor = AddAccessor(glb, jointsView, 5123, static_cast<int>(joints.size() / 4), "VEC4");
            weightsAccessor = AddAccessor(glb, weightsView, 5126, static_cast<int>(weights.size() / 4), "VEC4");
        }

        std::ostringstream primitive;
        primitive << "{\"attributes\":{\"POSITION\":" << positionAccessor
                  << ",\"NORMAL\":" << normalAccessor
                  << ",\"TEXCOORD_0\":" << uvAccessor;
        if (jointsAccessor >= 0 && weightsAccessor >= 0) {
            primitive << ",\"JOINTS_0\":" << jointsAccessor
                      << ",\"WEIGHTS_0\":" << weightsAccessor;
        }
        primitive << "},\"indices\":" << indexAccessor << ",\"mode\":4";
        if (section.textureIndex < model.materials.size()) {
            primitive << ",\"material\":" << section.textureIndex;
        }
        primitive << "}";
        primitiveJson.push_back(primitive.str());
    }

    if (primitiveJson.empty()) {
        throw std::runtime_error("Model has no visible sections to export.");
    }

    std::vector<std::string> animationsJson;
    if (skinned) {
        for (const ModelAnimationClip& clip : model.animations) {
            if (!IsAnimationPlayable(clip)) {
                continue;
            }
            std::vector<std::string> samplers;
            std::vector<std::string> channels;
            for (int boneIndex = 0; boneIndex < clip.boneCount && boneIndex < static_cast<int>(boneNodeIndices.size()); ++boneIndex) {
                std::vector<float> times;
                std::vector<float> rotations;
                times.reserve(static_cast<std::size_t>(clip.frameCount));
                rotations.reserve(static_cast<std::size_t>(clip.frameCount) * 4U);
                for (int frameIndex = 0; frameIndex < clip.frameCount; ++frameIndex) {
                    if (frameIndex >= static_cast<int>(clip.frames.size()) ||
                        boneIndex >= static_cast<int>(clip.frames[frameIndex].localRotations.size())) {
                        continue;
                    }
                    times.push_back(static_cast<float>(frameIndex) / std::max(0.001f, clip.fps));
                    const Quat rotation = Normalize(clip.frames[frameIndex].localRotations[boneIndex]);
                    rotations.push_back(rotation.x);
                    rotations.push_back(rotation.y);
                    rotations.push_back(rotation.z);
                    rotations.push_back(rotation.w);
                }
                if (times.empty()) {
                    continue;
                }
                const int timeView = AppendVectorBufferView(glb, times);
                const int rotationView = AppendVectorBufferView(glb, rotations);
                const int timeAccessor = AddAccessor(
                    glb,
                    timeView,
                    5126,
                    static_cast<int>(times.size()),
                    "SCALAR",
                    {times.front()},
                    {times.back()});
                const int rotationAccessor = AddAccessor(glb, rotationView, 5126, static_cast<int>(rotations.size() / 4), "VEC4");
                const int samplerIndex = static_cast<int>(samplers.size());
                samplers.push_back("{\"input\":" + std::to_string(timeAccessor) +
                                   ",\"interpolation\":\"LINEAR\",\"output\":" +
                                   std::to_string(rotationAccessor) + "}");
                channels.push_back("{\"sampler\":" + std::to_string(samplerIndex) +
                                   ",\"target\":{\"node\":" + std::to_string(boneNodeIndices[boneIndex]) +
                                   ",\"path\":\"rotation\"}}");
            }
            if (!samplers.empty()) {
                std::ostringstream animation;
                animation << "{\"name\":\"" << JsonEscape(clip.name) << "\",\"samplers\":[";
                for (std::size_t index = 0; index < samplers.size(); ++index) {
                    if (index != 0) {
                        animation << ",";
                    }
                    animation << samplers[index];
                }
                animation << "],\"channels\":[";
                for (std::size_t index = 0; index < channels.size(); ++index) {
                    if (index != 0) {
                        animation << ",";
                    }
                    animation << channels[index];
                }
                animation << "]}";
                animationsJson.push_back(animation.str());
            }
        }
    }

    const std::string json = BuildGlbJson(
        model,
        glb,
        primitiveJson,
        materialTextureIndices,
        skinAccessor,
        boneNodeIndices,
        animationsJson);
    WriteGlbFile(EnsureExtension(requestedPath, ".glb"), json, glb.binary);
}
