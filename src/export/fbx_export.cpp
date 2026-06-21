std::string FbxArray(const std::vector<float>& values) {
    std::ostringstream out;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            out << ",";
        }
        out << values[index];
    }
    return out.str();
}

std::string FbxEscapeString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

std::string FbxSafeName(std::string value) {
    if (value.empty()) {
        value = "export";
    }
    for (char& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 32 || character == ':' || character == ';' || character == ',' ||
            character == '"' || character == '\'' || character == '/' || character == '\\' ||
            character == '|' || character == '?' || character == '*') {
            character = '_';
        }
    }
    return value;
}

std::string FbxRelativePath(const std::filesystem::path& baseDirectory, const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path relative = std::filesystem::relative(path, baseDirectory, error);
    if (error || relative.empty()) {
        relative = path.filename();
    }
    std::string text = relative.string();
    std::replace(text.begin(), text.end(), '/', '\\');
    return text;
}

template <typename T>
void WriteFbxArrayProperty(std::ostream& out, const char* indent, const char* name, const std::vector<T>& values) {
    out << indent << name << ": *" << values.size() << " {\n";
    out << indent << "\ta: ";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            out << ",";
        }
        out << values[index];
    }
    out << "\n" << indent << "}\n";
}

template <std::size_t N>
void WriteFbxMatrixProperty(std::ostream& out, const char* indent, const char* name, const std::array<double, N>& values) {
    out << indent << name << ": *" << values.size() << " {\n";
    out << indent << "\ta: ";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            out << ",";
        }
        out << values[index];
    }
    out << "\n" << indent << "}\n";
}

struct FbxIdAllocator {
    std::int64_t next = 100000;

    std::int64_t Next() {
        return next++;
    }
};

struct FbxMaterialExport {
    std::int64_t materialId = 0;
    std::int64_t textureId = 0;
    std::int64_t videoId = 0;
    std::string name;
    std::filesystem::path texturePath;
    std::string relativeTexturePath;
    bool hasTexture = false;
};

struct FbxMeshExport {
    std::vector<double> positions;
    std::vector<int> polygonVertexIndices;
    std::vector<double> normals;
    std::vector<double> uvs;
    std::vector<int> uvIndices;
    std::vector<int> polygonMaterials;
    std::vector<std::uint32_t> sourceVertexIndices;
};

struct FbxSkinClusterExport {
    std::int64_t id = 0;
    int boneIndex = -1;
    std::vector<int> indices;
    std::vector<double> weights;
};

struct FbxAnimationCurveExport {
    std::int64_t id = 0;
    std::vector<std::int64_t> keyTimes;
    std::vector<double> values;
};

struct FbxAnimationBoneExport {
    int boneIndex = -1;
    std::int64_t rotationNodeId = 0;
    std::array<FbxAnimationCurveExport, 3> rotationCurves = {};
};

struct FbxAnimationClipExport {
    const ModelAnimationClip* clip = nullptr;
    std::int64_t stackId = 0;
    std::int64_t layerId = 0;
    std::vector<FbxAnimationBoneExport> bones;
};

Vec3 QuaternionToEulerDegrees(Quat rotation) {
    rotation = Normalize(rotation);
    constexpr double radiansToDegrees = 57.295779513082320876;

    const double sinr = 2.0 * (rotation.w * rotation.x + rotation.y * rotation.z);
    const double cosr = 1.0 - 2.0 * (rotation.x * rotation.x + rotation.y * rotation.y);
    const double x = std::atan2(sinr, cosr);

    const double sinp = 2.0 * (rotation.w * rotation.y - rotation.z * rotation.x);
    double y = 0.0;
    if (std::abs(sinp) >= 1.0) {
        y = std::copysign(1.57079632679489661923, sinp);
    } else {
        y = std::asin(sinp);
    }

    const double siny = 2.0 * (rotation.w * rotation.z + rotation.x * rotation.y);
    const double cosy = 1.0 - 2.0 * (rotation.y * rotation.y + rotation.z * rotation.z);
    const double z = std::atan2(siny, cosy);

    return {
        static_cast<float>(x * radiansToDegrees),
        static_cast<float>(y * radiansToDegrees),
        static_cast<float>(z * radiansToDegrees),
    };
}

std::array<double, 16> FbxTransformMatrix(Vec3 translation, Quat rotation) {
    rotation = Normalize(rotation);
    const double x = rotation.x;
    const double y = rotation.y;
    const double z = rotation.z;
    const double w = rotation.w;
    const double xx = x * x;
    const double yy = y * y;
    const double zz = z * z;
    const double xy = x * y;
    const double xz = x * z;
    const double yz = y * z;
    const double wx = w * x;
    const double wy = w * y;
    const double wz = w * z;

    return {
        1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy), 0.0,
        2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx), 0.0,
        2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy), 0.0,
        translation.x, translation.y, translation.z, 1.0,
    };
}

std::array<double, 16> FbxIdentityMatrix() {
    return {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };
}

std::vector<FbxMaterialExport> BuildFbxMaterials(const ModelPreview& model,
                                                 const std::filesystem::path& fbxPath,
                                                 FbxIdAllocator& ids) {
    const std::size_t materialCount = std::max<std::size_t>(1, model.materials.size());
    std::vector<FbxMaterialExport> materials;
    materials.reserve(materialCount);

    const std::filesystem::path baseDirectory =
        fbxPath.has_parent_path() ? fbxPath.parent_path() : std::filesystem::current_path();
    const std::filesystem::path textureDirectory = baseDirectory / (fbxPath.stem().string() + "_fbx_textures");

    for (std::size_t index = 0; index < materialCount; ++index) {
        FbxMaterialExport material;
        material.materialId = ids.Next();
        material.name = "Material_" + std::to_string(index);
        if (index < model.materials.size()) {
            const ModelMaterial& source = model.materials[index];
            const std::string sourceStem = FbxSafeName(std::filesystem::path(source.path).stem().string());
            if (!sourceStem.empty()) {
                material.name = sourceStem;
            }
            if (source.loaded && !source.texture.rgba.empty()) {
                std::filesystem::create_directories(textureDirectory);
                material.hasTexture = true;
                material.textureId = ids.Next();
                material.videoId = ids.Next();
                material.texturePath = textureDirectory / (FbxSafeName(material.name) + "_" + std::to_string(index) + ".png");
                WriteWicImageFile(source.texture, material.texturePath, GUID_ContainerFormatPng);
                material.relativeTexturePath = FbxRelativePath(baseDirectory, material.texturePath);
            }
        }
        materials.push_back(std::move(material));
    }
    return materials;
}

FbxMeshExport BuildFbxMeshExport(const ModelPreview& model, std::size_t materialCount) {
    FbxMeshExport mesh;

    for (const ModelSection& section : model.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }

        const int materialIndex = section.textureIndex < materialCount ? static_cast<int>(section.textureIndex) : 0;
        const std::size_t end = std::min(section.triangleStart + section.triangleCount, model.triangles.size());
        for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
            const ModelTriangle& triangle = model.triangles[triangleIndex];
            const std::array<std::uint32_t, 3> triangleIndices = {triangle.a, triangle.b, triangle.c};
            bool validTriangle = true;
            for (std::uint32_t vertexIndex : triangleIndices) {
                if (vertexIndex >= model.vertices.size()) {
                    validTriangle = false;
                    break;
                }
            }
            if (!validTriangle) {
                continue;
            }

            mesh.polygonMaterials.push_back(materialIndex);
            for (int corner = 0; corner < 3; ++corner) {
                const std::uint32_t modelVertexIndex = triangleIndices[static_cast<std::size_t>(corner)];
                const ModelVertex& vertex = model.vertices[modelVertexIndex];
                const int exportIndex = static_cast<int>(mesh.sourceVertexIndices.size());
                mesh.sourceVertexIndices.push_back(modelVertexIndex);

                mesh.positions.push_back(vertex.bindPosition.x);
                mesh.positions.push_back(vertex.bindPosition.y);
                mesh.positions.push_back(vertex.bindPosition.z);
                mesh.normals.push_back(vertex.bindNormal.x);
                mesh.normals.push_back(vertex.bindNormal.y);
                mesh.normals.push_back(vertex.bindNormal.z);

                unsigned char uvSet = section.textureCoordinateIndex;
                if (uvSet >= vertex.uvSetCount || uvSet >= vertex.uSets.size()) {
                    uvSet = 0;
                }
                mesh.uvs.push_back(vertex.uSets[uvSet]);
                mesh.uvs.push_back(vertex.vSets[uvSet]);
                mesh.uvIndices.push_back(exportIndex);

                mesh.polygonVertexIndices.push_back(corner == 2 ? -(exportIndex + 1) : exportIndex);
            }
        }
    }

    if (mesh.positions.empty()) {
        throw std::runtime_error("Model has no visible sections to export.");
    }
    return mesh;
}

std::vector<FbxSkinClusterExport> BuildFbxSkinClusters(const ModelPreview& model,
                                                       const FbxMeshExport& mesh,
                                                       FbxIdAllocator& ids) {
    std::vector<FbxSkinClusterExport> clusters;
    if (!HasExportableSkinning(model)) {
        return clusters;
    }

    clusters.resize(model.skeleton.size());
    for (std::size_t boneIndex = 0; boneIndex < model.skeleton.size(); ++boneIndex) {
        clusters[boneIndex].id = ids.Next();
        clusters[boneIndex].boneIndex = static_cast<int>(boneIndex);
    }

    for (std::size_t controlPointIndex = 0; controlPointIndex < mesh.sourceVertexIndices.size(); ++controlPointIndex) {
        const std::uint32_t sourceIndex = mesh.sourceVertexIndices[controlPointIndex];
        if (sourceIndex >= model.vertices.size()) {
            continue;
        }
        const ModelVertex& vertex = model.vertices[sourceIndex];
        bool wroteInfluence = false;
        for (int influenceIndex = 0; influenceIndex < vertex.skinInfluenceCount; ++influenceIndex) {
            const SkinInfluence& influence = vertex.skinInfluences[static_cast<std::size_t>(influenceIndex)];
            if (influence.bone < 0 ||
                influence.bone >= static_cast<int>(clusters.size()) ||
                influence.weight <= 0.0f) {
                continue;
            }
            clusters[static_cast<std::size_t>(influence.bone)].indices.push_back(static_cast<int>(controlPointIndex));
            clusters[static_cast<std::size_t>(influence.bone)].weights.push_back(influence.weight);
            wroteInfluence = true;
        }
        if (!wroteInfluence && !clusters.empty()) {
            clusters.front().indices.push_back(static_cast<int>(controlPointIndex));
            clusters.front().weights.push_back(1.0);
        }
    }
    return clusters;
}

std::vector<FbxAnimationClipExport> BuildFbxAnimationExports(const ModelPreview& model, FbxIdAllocator& ids) {
    std::vector<FbxAnimationClipExport> clips;
    if (model.skeleton.empty()) {
        return clips;
    }

    constexpr double kFbxTimeSecond = 46186158000.0;
    for (const ModelAnimationClip& clip : model.animations) {
        if (!IsAnimationPlayable(clip)) {
            continue;
        }

        FbxAnimationClipExport exportClip;
        exportClip.clip = &clip;
        exportClip.stackId = ids.Next();
        exportClip.layerId = ids.Next();

        const int boneCount = std::min<int>(clip.boneCount, static_cast<int>(model.skeleton.size()));
        for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            FbxAnimationBoneExport bone;
            bone.boneIndex = boneIndex;
            bone.rotationNodeId = ids.Next();
            for (FbxAnimationCurveExport& curve : bone.rotationCurves) {
                curve.id = ids.Next();
                curve.keyTimes.reserve(static_cast<std::size_t>(clip.frameCount));
                curve.values.reserve(static_cast<std::size_t>(clip.frameCount));
            }

            for (int frameIndex = 0; frameIndex < clip.frameCount; ++frameIndex) {
                if (frameIndex >= static_cast<int>(clip.frames.size()) ||
                    boneIndex >= static_cast<int>(clip.frames[static_cast<std::size_t>(frameIndex)].localRotations.size())) {
                    continue;
                }

                const auto keyTime = static_cast<std::int64_t>(
                    std::llround((static_cast<double>(frameIndex) / std::max(0.001f, clip.fps)) * kFbxTimeSecond));
                const Vec3 euler = QuaternionToEulerDegrees(
                    clip.frames[static_cast<std::size_t>(frameIndex)].localRotations[static_cast<std::size_t>(boneIndex)]);
                const std::array<double, 3> values = {euler.x, euler.y, euler.z};
                for (int axis = 0; axis < 3; ++axis) {
                    bone.rotationCurves[static_cast<std::size_t>(axis)].keyTimes.push_back(keyTime);
                    bone.rotationCurves[static_cast<std::size_t>(axis)].values.push_back(values[static_cast<std::size_t>(axis)]);
                }
            }

            if (!bone.rotationCurves[0].keyTimes.empty()) {
                exportClip.bones.push_back(std::move(bone));
            }
        }

        if (!exportClip.bones.empty()) {
            clips.push_back(std::move(exportClip));
        }
    }
    return clips;
}

void WriteFbxModelProperties(std::ostream& out, Vec3 translation, Vec3 rotationDegrees) {
    out << "\t\tProperties70:  {\n";
    out << "\t\t\tP: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\","
        << translation.x << "," << translation.y << "," << translation.z << "\n";
    out << "\t\t\tP: \"Lcl Rotation\", \"Lcl Rotation\", \"\", \"A\","
        << rotationDegrees.x << "," << rotationDegrees.y << "," << rotationDegrees.z << "\n";
    out << "\t\t\tP: \"Lcl Scaling\", \"Lcl Scaling\", \"\", \"A\",1,1,1\n";
    out << "\t\t}\n";
}

void WriteFbxMaterial(std::ostream& out, const FbxMaterialExport& material) {
    out << "\tMaterial: " << material.materialId << ", \"Material::" << FbxEscapeString(material.name) << "\", \"\" {\n";
    out << "\t\tVersion: 102\n";
    out << "\t\tShadingModel: \"phong\"\n";
    out << "\t\tMultiLayer: 0\n";
    out << "\t\tProperties70:  {\n";
    out << "\t\t\tP: \"DiffuseColor\", \"Color\", \"\", \"A\",0.8,0.8,0.8\n";
    out << "\t\t\tP: \"AmbientColor\", \"Color\", \"\", \"A\",0.2,0.2,0.2\n";
    out << "\t\t\tP: \"SpecularColor\", \"Color\", \"\", \"A\",0.05,0.05,0.05\n";
    out << "\t\t}\n";
    out << "\t}\n";

    if (!material.hasTexture) {
        return;
    }

    const std::string absolute = material.texturePath.string();
    out << "\tVideo: " << material.videoId << ", \"Video::" << FbxEscapeString(material.name) << "\", \"Clip\" {\n";
    out << "\t\tType: \"Clip\"\n";
    out << "\t\tProperties70:  {\n";
    out << "\t\t\tP: \"Path\", \"KString\", \"XRefUrl\", \"\", \"" << FbxEscapeString(absolute) << "\"\n";
    out << "\t\t}\n";
    out << "\t\tFileName: \"" << FbxEscapeString(absolute) << "\"\n";
    out << "\t\tRelativeFilename: \"" << FbxEscapeString(material.relativeTexturePath) << "\"\n";
    out << "\t}\n";

    out << "\tTexture: " << material.textureId << ", \"Texture::" << FbxEscapeString(material.name) << "\", \"\" {\n";
    out << "\t\tType: \"TextureVideoClip\"\n";
    out << "\t\tVersion: 202\n";
    out << "\t\tTextureName: \"Texture::" << FbxEscapeString(material.name) << "\"\n";
    out << "\t\tMedia: \"Video::" << FbxEscapeString(material.name) << "\"\n";
    out << "\t\tFileName: \"" << FbxEscapeString(absolute) << "\"\n";
    out << "\t\tRelativeFilename: \"" << FbxEscapeString(material.relativeTexturePath) << "\"\n";
    out << "\t\tModelUVTranslation: 0,0\n";
    out << "\t\tModelUVScaling: 1,1\n";
    out << "\t\tTexture_Alpha_Source: \"None\"\n";
    out << "\t\tCropping: 0,0,0,0\n";
    out << "\t}\n";
}

void WriteFbxAnimationCurve(std::ostream& out, const FbxAnimationCurveExport& curve, const std::string& name) {
    out << "\tAnimationCurve: " << curve.id << ", \"AnimCurve::" << FbxEscapeString(name) << "\", \"\" {\n";
    out << "\t\tDefault: 0\n";
    out << "\t\tKeyVer: 4008\n";
    WriteFbxArrayProperty(out, "\t\t", "KeyTime", curve.keyTimes);
    WriteFbxArrayProperty(out, "\t\t", "KeyValueFloat", curve.values);
    out << "\t\tKeyAttrFlags: *1 {\n\t\t\ta: 24836\n\t\t}\n";
    out << "\t\tKeyAttrDataFloat: *4 {\n\t\t\ta: 0,0,0,0\n\t\t}\n";
    out << "\t\tKeyAttrRefCount: *1 {\n\t\t\ta: " << curve.values.size() << "\n\t\t}\n";
    out << "\t}\n";
}

std::string AssimpBoneName(std::size_t boneIndex) {
    return "Bone_" + std::to_string(boneIndex);
}

aiQuaternion ToAiQuat(Quat rotation) {
    rotation = Normalize(rotation);
    return aiQuaternion(rotation.w, rotation.x, rotation.y, rotation.z);
}

aiMatrix4x4 ToAiTransform(Vec3 translation, Quat rotation) {
    aiMatrix4x4 transform = aiMatrix4x4(ToAiQuat(rotation).GetMatrix());
    transform.a4 = translation.x;
    transform.b4 = translation.y;
    transform.c4 = translation.z;
    return transform;
}

aiMatrix4x4 ToAiMatrixFromGltfColumnMajor(const std::array<float, 16>& matrix) {
    return aiMatrix4x4(
        matrix[0], matrix[4], matrix[8], matrix[12],
        matrix[1], matrix[5], matrix[9], matrix[13],
        matrix[2], matrix[6], matrix[10], matrix[14],
        matrix[3], matrix[7], matrix[11], matrix[15]);
}

aiMatrix4x4 ToAiInverseBindMatrix(const SkeletonBone& bone) {
    return ToAiMatrixFromGltfColumnMajor(InverseBindMatrix(bone));
}

struct AssimpMeshBuild {
    aiMesh* mesh = nullptr;
    std::vector<std::uint32_t> sourceVertexIndices;
};

void AddAssimpInfluence(std::vector<SkinInfluence>& influences, int bone, float weight) {
    if (bone < 0 || weight <= 0.0f) {
        return;
    }
    for (SkinInfluence& influence : influences) {
        if (influence.bone == bone) {
            influence.weight += weight;
            return;
        }
    }
    influences.push_back({bone, weight});
}

void AddAssimpSourceVertexWeights(const ModelPreview& model,
                                  std::uint32_t sourceIndex,
                                  unsigned int exportVertexIndex,
                                  std::vector<std::vector<aiVertexWeight>>& weightsByBone) {
    if (sourceIndex >= model.vertices.size()) {
        return;
    }

    const ModelVertex& vertex = model.vertices[sourceIndex];
    std::vector<SkinInfluence> influences;
    if (vertex.skinRecord >= 0 &&
        vertex.skinRecord < static_cast<int>(model.skinRecords.size())) {
        const SkinBlendRecord& record = model.skinRecords[static_cast<std::size_t>(vertex.skinRecord)];
        for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
            const SkinBlendInfluence& influence = record.influences[static_cast<std::size_t>(influenceIndex)];
            AddAssimpInfluence(influences, influence.bone, influence.weight);
        }
    }

    if (influences.empty()) {
        for (int influenceIndex = 0; influenceIndex < vertex.skinInfluenceCount; ++influenceIndex) {
            const SkinInfluence& influence = vertex.skinInfluences[static_cast<std::size_t>(influenceIndex)];
            AddAssimpInfluence(influences, influence.bone, influence.weight);
        }
    }

    float totalWeight = 0.0f;
    for (const SkinInfluence& influence : influences) {
        if (influence.bone >= 0 &&
            influence.bone < static_cast<int>(weightsByBone.size()) &&
            influence.weight > 0.0f) {
            totalWeight += influence.weight;
        }
    }

    if (totalWeight <= 0.0001f) {
        if (!weightsByBone.empty()) {
            weightsByBone.front().push_back(aiVertexWeight(exportVertexIndex, 1.0f));
        }
        return;
    }

    for (const SkinInfluence& influence : influences) {
        if (influence.bone < 0 ||
            influence.bone >= static_cast<int>(weightsByBone.size()) ||
            influence.weight <= 0.0f) {
            continue;
        }
        weightsByBone[static_cast<std::size_t>(influence.bone)].push_back(
            aiVertexWeight(exportVertexIndex, influence.weight / totalWeight));
    }
}

AssimpMeshBuild BuildAssimpSectionMesh(const ModelPreview& model,
                                       const ModelSection& section,
                                       std::size_t materialCount,
                                       bool skinned) {
    std::vector<std::uint32_t> sourceIndices;
    std::vector<const ModelVertex*> vertices;
    std::vector<std::array<unsigned int, 3>> faces;

    const std::size_t end = std::min(section.triangleStart + section.triangleCount, model.triangles.size());
    for (std::size_t triangleIndex = section.triangleStart; triangleIndex < end; ++triangleIndex) {
        const ModelTriangle& triangle = model.triangles[triangleIndex];
        const std::array<std::uint32_t, 3> triangleIndices = {triangle.a, triangle.b, triangle.c};
        bool validTriangle = true;
        for (std::uint32_t vertexIndex : triangleIndices) {
            if (vertexIndex >= model.vertices.size()) {
                validTriangle = false;
                break;
            }
        }
        if (!validTriangle) {
            continue;
        }

        std::array<unsigned int, 3> face = {};
        for (int corner = 0; corner < 3; ++corner) {
            const std::uint32_t sourceIndex = triangleIndices[static_cast<std::size_t>(corner)];
            face[static_cast<std::size_t>(corner)] = static_cast<unsigned int>(vertices.size());
            vertices.push_back(&model.vertices[sourceIndex]);
            sourceIndices.push_back(sourceIndex);
        }
        faces.push_back(face);
    }

    if (vertices.empty() || faces.empty()) {
        return {};
    }

    aiMesh* mesh = new aiMesh();
    mesh->mName = aiString(section.name.empty() ? "MeshSection" : section.name);
    mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
    mesh->mMaterialIndex = section.textureIndex < materialCount ? section.textureIndex : 0;
    mesh->mNumVertices = static_cast<unsigned int>(vertices.size());
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];
    mesh->mNormals = new aiVector3D[mesh->mNumVertices];
    mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
    mesh->mNumUVComponents[0] = 2;

    for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
        const ModelVertex& vertex = *vertices[vertexIndex];
        mesh->mVertices[vertexIndex] = aiVector3D(vertex.bindPosition.x, vertex.bindPosition.y, vertex.bindPosition.z);
        mesh->mNormals[vertexIndex] = aiVector3D(vertex.bindNormal.x, vertex.bindNormal.y, vertex.bindNormal.z);

        unsigned char uvSet = section.textureCoordinateIndex;
        if (uvSet >= vertex.uvSetCount || uvSet >= vertex.uSets.size()) {
            uvSet = 0;
        }
        mesh->mTextureCoords[0][vertexIndex] = aiVector3D(vertex.uSets[uvSet], vertex.vSets[uvSet], 0.0f);
    }

    mesh->mNumFaces = static_cast<unsigned int>(faces.size());
    mesh->mFaces = new aiFace[mesh->mNumFaces];
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        aiFace& face = mesh->mFaces[faceIndex];
        face.mNumIndices = 3;
        face.mIndices = new unsigned int[3];
        face.mIndices[0] = faces[faceIndex][0];
        face.mIndices[1] = faces[faceIndex][1];
        face.mIndices[2] = faces[faceIndex][2];
    }

    if (skinned) {
        std::vector<std::vector<aiVertexWeight>> weightsByBone(model.skeleton.size());
        for (std::size_t vertexIndex = 0; vertexIndex < sourceIndices.size(); ++vertexIndex) {
            AddAssimpSourceVertexWeights(
                model,
                sourceIndices[vertexIndex],
                static_cast<unsigned int>(vertexIndex),
                weightsByBone);
        }

        std::vector<aiBone*> bones;
        bones.reserve(model.skeleton.size());
        for (std::size_t boneIndex = 0; boneIndex < weightsByBone.size(); ++boneIndex) {
            if (weightsByBone[boneIndex].empty()) {
                continue;
            }

            aiBone* bone = new aiBone();
            bone->mName = aiString(AssimpBoneName(boneIndex));
            bone->mOffsetMatrix = ToAiInverseBindMatrix(model.skeleton[boneIndex]);
            bone->mNumWeights = static_cast<unsigned int>(weightsByBone[boneIndex].size());
            bone->mWeights = new aiVertexWeight[bone->mNumWeights];
            std::copy(weightsByBone[boneIndex].begin(), weightsByBone[boneIndex].end(), bone->mWeights);
            bones.push_back(bone);
        }

        if (!bones.empty()) {
            mesh->mNumBones = static_cast<unsigned int>(bones.size());
            mesh->mBones = new aiBone*[mesh->mNumBones];
            std::copy(bones.begin(), bones.end(), mesh->mBones);
        }
    }

    return {mesh, std::move(sourceIndices)};
}

aiNode* BuildAssimpBoneNodeTree(const ModelPreview& model,
                                const std::vector<aiNode*>& boneNodes,
                                std::size_t boneIndex,
                                aiNode* parent) {
    aiNode* node = boneNodes[boneIndex];
    node->mParent = parent;

    std::vector<aiNode*> children;
    for (std::size_t childIndex = 0; childIndex < model.skeleton.size(); ++childIndex) {
        if (model.skeleton[childIndex].parent == static_cast<int>(boneIndex)) {
            children.push_back(BuildAssimpBoneNodeTree(model, boneNodes, childIndex, node));
        }
    }

    if (!children.empty()) {
        node->mNumChildren = static_cast<unsigned int>(children.size());
        node->mChildren = new aiNode*[node->mNumChildren];
        std::copy(children.begin(), children.end(), node->mChildren);
    }
    return node;
}

std::unique_ptr<aiScene> BuildAssimpSceneForFbx(const ModelPreview& model,
                                                const std::vector<FbxMaterialExport>& materials) {
    const bool skinned = HasExportableSkinning(model);
    auto scene = std::make_unique<aiScene>();
    scene->mRootNode = new aiNode("Root");

    scene->mNumMaterials = static_cast<unsigned int>(materials.size());
    scene->mMaterials = new aiMaterial*[scene->mNumMaterials];
    for (std::size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex) {
        const FbxMaterialExport& materialExport = materials[materialIndex];
        aiMaterial* material = new aiMaterial();
        aiString materialName(materialExport.name);
        material->AddProperty(&materialName, AI_MATKEY_NAME);
        aiColor3D diffuse(0.8f, 0.8f, 0.8f);
        material->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);
        if (materialExport.hasTexture) {
            aiString texturePath(materialExport.relativeTexturePath);
            material->AddProperty(&texturePath, AI_MATKEY_TEXTURE_DIFFUSE(0));
        }
        scene->mMaterials[materialIndex] = material;
    }

    std::vector<aiMesh*> meshes;
    meshes.reserve(model.sections.size());
    for (const ModelSection& section : model.sections) {
        if (!section.visible || section.triangleCount == 0) {
            continue;
        }
        AssimpMeshBuild mesh = BuildAssimpSectionMesh(model, section, materials.size(), skinned);
        if (mesh.mesh != nullptr) {
            meshes.push_back(mesh.mesh);
        }
    }
    if (meshes.empty()) {
        throw std::runtime_error("Model has no visible sections to export.");
    }

    scene->mNumMeshes = static_cast<unsigned int>(meshes.size());
    scene->mMeshes = new aiMesh*[scene->mNumMeshes];
    std::copy(meshes.begin(), meshes.end(), scene->mMeshes);

    const std::string modelName = FbxSafeName(std::filesystem::path(model.name).stem().string());
    std::vector<aiNode*> rootChildren;
    rootChildren.reserve(static_cast<std::size_t>(scene->mNumMeshes) + model.skeleton.size());
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiNode* meshNode = new aiNode(modelName + "_Mesh_" + std::to_string(meshIndex + 1));
        meshNode->mParent = scene->mRootNode;
        meshNode->mNumMeshes = 1;
        meshNode->mMeshes = new unsigned int[1];
        meshNode->mMeshes[0] = meshIndex;
        rootChildren.push_back(meshNode);
    }
    if (skinned) {
        std::vector<aiNode*> boneNodes(model.skeleton.size(), nullptr);
        for (std::size_t boneIndex = 0; boneIndex < model.skeleton.size(); ++boneIndex) {
            const SkeletonBone& bone = model.skeleton[boneIndex];
            aiNode* boneNode = new aiNode(AssimpBoneName(boneIndex));
            boneNode->mTransformation = ToAiTransform(ExportBoneLocalPosition(model, boneIndex), bone.localRotation);
            boneNodes[boneIndex] = boneNode;
        }

        for (std::size_t boneIndex = 0; boneIndex < model.skeleton.size(); ++boneIndex) {
            const int parent = model.skeleton[boneIndex].parent;
            if (parent < 0 ||
                parent >= static_cast<int>(model.skeleton.size()) ||
                parent == static_cast<int>(boneIndex)) {
                rootChildren.push_back(BuildAssimpBoneNodeTree(model, boneNodes, boneIndex, scene->mRootNode));
            }
        }
    }

    scene->mRootNode->mNumChildren = static_cast<unsigned int>(rootChildren.size());
    scene->mRootNode->mChildren = new aiNode*[scene->mRootNode->mNumChildren];
    std::copy(rootChildren.begin(), rootChildren.end(), scene->mRootNode->mChildren);

    if (skinned) {
        std::vector<aiAnimation*> animations;
        for (const ModelAnimationClip& clip : model.animations) {
            if (!IsAnimationPlayable(clip)) {
                continue;
            }

            const int boneCount = std::min<int>(clip.boneCount, static_cast<int>(model.skeleton.size()));
            if (boneCount <= 0) {
                continue;
            }

            aiAnimation* animation = new aiAnimation();
            animation->mName = aiString(FbxSafeName(clip.name));
            animation->mTicksPerSecond = std::max(0.001f, clip.fps);
            animation->mDuration = std::max(0, clip.frameCount - 1);
            animation->mNumChannels = static_cast<unsigned int>(boneCount);
            animation->mChannels = new aiNodeAnim*[animation->mNumChannels];

            for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
                const SkeletonBone& bone = model.skeleton[static_cast<std::size_t>(boneIndex)];
                aiNodeAnim* channel = new aiNodeAnim();
                channel->mNodeName = aiString(AssimpBoneName(static_cast<std::size_t>(boneIndex)));

                channel->mNumPositionKeys = static_cast<unsigned int>(clip.frameCount);
                channel->mPositionKeys = new aiVectorKey[channel->mNumPositionKeys];
                channel->mNumRotationKeys = static_cast<unsigned int>(clip.frameCount);
                channel->mRotationKeys = new aiQuatKey[channel->mNumRotationKeys];
                channel->mNumScalingKeys = 1;
                channel->mScalingKeys = new aiVectorKey[1];
                channel->mScalingKeys[0] = aiVectorKey(0.0, aiVector3D(1.0f, 1.0f, 1.0f));

                for (int frameIndex = 0; frameIndex < clip.frameCount; ++frameIndex) {
                    const double time = static_cast<double>(frameIndex);
                    const Vec3 position = ExportBoneLocalPosition(model, static_cast<std::size_t>(boneIndex));
                    Quat rotation = bone.localRotation;
                    if (frameIndex < static_cast<int>(clip.frames.size())) {
                        const ModelAnimationFrame& frame = clip.frames[static_cast<std::size_t>(frameIndex)];
                        if (boneIndex < static_cast<int>(frame.localRotations.size())) {
                            rotation = frame.localRotations[static_cast<std::size_t>(boneIndex)];
                        }
                    }
                    channel->mPositionKeys[frameIndex] =
                        aiVectorKey(time, aiVector3D(position.x, position.y, position.z));
                    channel->mRotationKeys[frameIndex] = aiQuatKey(time, ToAiQuat(rotation));
                }

                animation->mChannels[static_cast<std::size_t>(boneIndex)] = channel;
            }

            animations.push_back(animation);
        }

        if (!animations.empty()) {
            scene->mNumAnimations = static_cast<unsigned int>(animations.size());
            scene->mAnimations = new aiAnimation*[scene->mNumAnimations];
            std::copy(animations.begin(), animations.end(), scene->mAnimations);
        }
    }

    return scene;
}

std::string FindAssimpFbxExportFormatId(Assimp::Exporter& exporter) {
    for (std::size_t index = 0; index < exporter.GetExportFormatCount(); ++index) {
        const aiExportFormatDesc* description = exporter.GetExportFormatDescription(index);
        if (description == nullptr || description->id == nullptr || description->fileExtension == nullptr) {
            continue;
        }
        const std::string id = description->id;
        const std::string extension = ToLower(description->fileExtension);
        if (id == "fbx" || extension == "fbx") {
            return id;
        }
    }
    return {};
}

void ExportModelFbx(const ModelPreview& model, const std::filesystem::path& requestedPath) {
    if (model.vertices.empty() || model.triangles.empty()) {
        throw std::runtime_error("Model has no mesh data to export.");
    }

    const std::filesystem::path fbxPath = EnsureExtension(requestedPath, ".fbx");
    FbxIdAllocator ids;
    std::vector<FbxMaterialExport> materials = BuildFbxMaterials(model, fbxPath, ids);

    std::unique_ptr<aiScene> scene = BuildAssimpSceneForFbx(model, materials);
    Assimp::Exporter exporter;
    const std::string formatId = FindAssimpFbxExportFormatId(exporter);
    if (formatId.empty()) {
        throw std::runtime_error("Assimp was built without an FBX exporter.");
    }

    Assimp::ExportProperties properties;
    properties.SetPropertyBool("bJoinIdenticalVertices", false);
    const aiReturn result = exporter.Export(scene.get(), formatId, fbxPath.string(), 0, &properties);
    if (result != aiReturn_SUCCESS) {
        throw std::runtime_error(std::string("Assimp FBX export failed: ") + exporter.GetErrorString());
    }
    if (!std::filesystem::exists(fbxPath) || std::filesystem::file_size(fbxPath) == 0) {
        throw std::runtime_error("Assimp completed FBX export but did not create a valid FBX file.");
    }
}
