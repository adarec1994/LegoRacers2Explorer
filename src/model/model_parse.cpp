void RequireModelRange(std::size_t offset,
                       std::size_t size,
                       std::size_t limit,
                       const char* message) {
    if (offset > limit || size > limit - offset) {
        throw std::runtime_error(message);
    }
}

std::size_t FindFourCc(const std::vector<char>& bytes, const char* fourCc) {
    const auto found = std::search(bytes.begin(), bytes.end(), fourCc, fourCc + 4);
    if (found == bytes.end()) {
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(std::distance(bytes.begin(), found));
}

ModelAnimationClip DecodeBsaAnimation(const std::vector<char>& bytes, std::string name, std::string path) {
    if (bytes.size() < 32 || std::memcmp(bytes.data(), "ANIMBIN", 7) != 0) {
        throw std::runtime_error("Not a supported LR2 BSA animation.");
    }

    const std::uint32_t version = ReadU32Le(bytes, 8);
    const std::uint32_t fileSize = ReadU32Le(bytes, 12);
    const std::uint16_t frameCount = ReadU16Le(bytes, 16);
    const std::uint16_t flags = ReadU16Le(bytes, 18);
    float fps = ReadF32Le(bytes, 20);
    const std::uint32_t boneCount = ReadU32Le(bytes, 24);

    if (version != 2) {
        throw std::runtime_error("BSA animation version is unsupported.");
    }
    if (frameCount == 0 || frameCount > 10000 || boneCount == 0 || boneCount > 512) {
        throw std::runtime_error("BSA animation dimensions are unsupported.");
    }
    if (!std::isfinite(fps) || fps <= 0.0f) {
        fps = 15.0f;
    }

    const std::size_t streamLimit = fileSize > 0 && fileSize <= bytes.size() ? fileSize : bytes.size();
    constexpr std::size_t kHeaderSize = 32;
    const std::size_t frameStride = 24 + static_cast<std::size_t>(boneCount) * 16;
    RequireModelRange(kHeaderSize, static_cast<std::size_t>(frameCount) * frameStride, streamLimit,
                      "BSA animation frames are truncated.");

    ModelAnimationClip clip;
    clip.name = std::move(name);
    clip.path = std::move(path);
    clip.loaded = true;
    clip.rootMotion = (flags & 1U) != 0;
    clip.loop = clip.rootMotion || ToLower(clip.name).find("loop") != std::string::npos;
    clip.fps = fps;
    clip.frameCount = frameCount;
    clip.boneCount = static_cast<int>(boneCount);
    clip.frames.reserve(frameCount);

    for (std::uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const std::size_t frameOffset = kHeaderSize + static_cast<std::size_t>(frameIndex) * frameStride;
        ModelAnimationFrame frame;
        frame.rootPosition = {
            ReadF32Le(bytes, frameOffset + 0),
            ReadF32Le(bytes, frameOffset + 4),
            ReadF32Le(bytes, frameOffset + 8),
        };
        frame.rootMotion = {
            ReadF32Le(bytes, frameOffset + 12),
            ReadF32Le(bytes, frameOffset + 16),
            ReadF32Le(bytes, frameOffset + 20),
        };
        Sanitize(frame.rootPosition);
        Sanitize(frame.rootMotion);

        frame.localRotations.reserve(boneCount);
        const std::size_t rotationsOffset = frameOffset + 24;
        for (std::uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            const std::size_t rotationOffset = rotationsOffset + static_cast<std::size_t>(boneIndex) * 16;
            Quat rotation = Normalize({
                ReadF32Le(bytes, rotationOffset + 0),
                ReadF32Le(bytes, rotationOffset + 4),
                ReadF32Le(bytes, rotationOffset + 8),
                ReadF32Le(bytes, rotationOffset + 12),
            });
            if (!std::isfinite(rotation.x) ||
                !std::isfinite(rotation.y) ||
                !std::isfinite(rotation.z) ||
                !std::isfinite(rotation.w)) {
                rotation = {};
            }
            frame.localRotations.push_back(rotation);
        }
        clip.frames.push_back(std::move(frame));
    }

    clip.status =
        std::to_string(clip.frameCount) + " frames @ " +
        std::to_string(static_cast<int>(std::round(clip.fps))) + " fps";
    return clip;
}

void Sanitize(Vec3& value) {
    if (!std::isfinite(value.x)) {
        value.x = 0.0f;
    }
    if (!std::isfinite(value.y)) {
        value.y = 0.0f;
    }
    if (!std::isfinite(value.z)) {
        value.z = 0.0f;
    }
}

void IncludeInBounds(ModelPreview& model, Vec3 position) {
    model.boundsMin.x = std::min(model.boundsMin.x, position.x);
    model.boundsMin.y = std::min(model.boundsMin.y, position.y);
    model.boundsMin.z = std::min(model.boundsMin.z, position.z);
    model.boundsMax.x = std::max(model.boundsMax.x, position.x);
    model.boundsMax.y = std::max(model.boundsMax.y, position.y);
    model.boundsMax.z = std::max(model.boundsMax.z, position.z);
}

std::string ReadFixedString(const std::vector<char>& bytes, std::size_t offset, std::size_t maxLength) {
    if (offset >= bytes.size()) {
        return {};
    }

    maxLength = std::min(maxLength, bytes.size() - offset);
    std::size_t length = 0;
    while (length < maxLength && bytes[offset + length] != '\0') {
        ++length;
    }
    return Trim(std::string(bytes.data() + offset, bytes.data() + offset + length));
}

std::string ReadBoundedString(const std::vector<char>& bytes,
                              std::size_t offset,
                              std::size_t end,
                              std::size_t maxLength) {
    if (offset >= end || offset >= bytes.size()) {
        return {};
    }

    end = std::min(end, bytes.size());
    maxLength = std::min(maxLength, end - offset);
    std::size_t length = 0;
    while (length < maxLength && bytes[offset + length] != '\0') {
        const auto character = static_cast<unsigned char>(bytes[offset + length]);
        if (character < 32 && character != '\t') {
            break;
        }
        ++length;
    }
    return Trim(std::string(bytes.data() + offset, bytes.data() + offset + length));
}

void UpdateSkeletonWorldPose(std::vector<SkeletonBone>& bones) {
    for (SkeletonBone& bone : bones) {
        const bool hasParent =
            bone.parent >= 0 &&
            bone.parent < static_cast<int>(bones.size()) &&
            bone.parent != bone.id;
        if (!hasParent) {
            bone.worldRotation = Normalize(bone.localRotation);
            bone.worldPosition = bone.localPosition;
            continue;
        }

        const SkeletonBone& parent = bones[bone.parent];
        bone.worldRotation = Multiply(parent.worldRotation, bone.localRotation);
        bone.worldPosition = Add(parent.worldPosition, Rotate(parent.worldRotation, bone.localPosition));
    }
}

ModelPreview DecodeBsbSkeleton(const std::vector<char>& bytes) {
    if (bytes.size() < 0x30 || std::memcmp(bytes.data(), "SKELBIN", 7) != 0) {
        throw std::runtime_error("Not a supported LR2 BSB skeleton.");
    }

    const std::uint32_t fileSize = ReadU32Le(bytes, 12);
    const std::uint32_t boneCount = ReadU32Le(bytes, 16);
    const std::uint32_t boneTableOffset = ReadU32Le(bytes, 20);
    const std::uint32_t clipCount = ReadU32Le(bytes, 24);
    const std::uint32_t clipNameOffset = ReadU32Le(bytes, 28);
    constexpr std::size_t kBoneRecordSize = 32;
    constexpr std::size_t kClipNameSize = 32;

    if (fileSize != 0 && fileSize > bytes.size()) {
        throw std::runtime_error("BSB skeleton file size is invalid.");
    }
    if (boneCount == 0 || boneCount > 512) {
        throw std::runtime_error("BSB skeleton bone count is unsupported.");
    }
    RequireModelRange(
        boneTableOffset,
        static_cast<std::size_t>(boneCount) * kBoneRecordSize,
        bytes.size(),
        "BSB skeleton bone table is truncated.");

    std::vector<SkeletonBone> rawBones;
    rawBones.reserve(boneCount);
    for (std::uint32_t recordIndex = 0; recordIndex < boneCount; ++recordIndex) {
        const std::size_t offset = boneTableOffset + static_cast<std::size_t>(recordIndex) * kBoneRecordSize;
        SkeletonBone bone;
        bone.id = static_cast<int>(ReadU16Le(bytes, offset));
        const std::uint16_t parent = ReadU16Le(bytes, offset + 2);
        bone.parent = parent == 0xffffU ? -1 : static_cast<int>(parent);
        bone.localRotation = Normalize({
            ReadF32Le(bytes, offset + 4),
            ReadF32Le(bytes, offset + 8),
            ReadF32Le(bytes, offset + 12),
            ReadF32Le(bytes, offset + 16),
        });
        bone.bindLocalRotation = bone.localRotation;
        bone.localPosition = {
            ReadF32Le(bytes, offset + 20),
            ReadF32Le(bytes, offset + 24),
            ReadF32Le(bytes, offset + 28),
        };
        Sanitize(bone.localPosition);
        rawBones.push_back(bone);
    }

    std::unordered_map<int, int> idToIndex;
    idToIndex.reserve(rawBones.size());
    for (int index = 0; index < static_cast<int>(rawBones.size()); ++index) {
        idToIndex[rawBones[index].id] = index;
    }

    std::vector<SkeletonBone> bones = rawBones;
    for (SkeletonBone& bone : bones) {
        if (bone.parent < 0) {
            continue;
        }
        const auto found = idToIndex.find(bone.parent);
        bone.parent = found == idToIndex.end() ? -1 : found->second;
    }
    for (int index = 0; index < static_cast<int>(bones.size()); ++index) {
        bones[index].id = index;
    }
    UpdateSkeletonWorldPose(bones);
    for (SkeletonBone& bone : bones) {
        bone.bindWorldPosition = bone.worldPosition;
        bone.bindWorldRotation = bone.worldRotation;
    }

    ModelPreview skeleton;
    skeleton.skeleton = std::move(bones);
    if (clipCount > 0 && clipCount <= 256 && clipNameOffset < bytes.size()) {
        for (std::uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex) {
            const std::size_t offset = clipNameOffset + static_cast<std::size_t>(clipIndex) * kClipNameSize;
            const std::string clipName = ReadFixedString(bytes, offset, kClipNameSize);
            if (!clipName.empty()) {
                skeleton.skeletonClips.push_back(clipName);
            }
        }
    }
    return skeleton;
}

std::array<int, 3> AxesSortedByExtent(Vec3 extent) {
    std::array<int, 3> axes = {0, 1, 2};
    std::sort(axes.begin(), axes.end(), [&](int left, int right) {
        return GetAxis(extent, left) > GetAxis(extent, right);
    });
    return axes;
}

void ApplySkeletonAlignment(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        return;
    }

    Vec3 skeletonMin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 skeletonMax{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    for (const SkeletonBone& bone : preview.skeleton) {
        skeletonMin.x = std::min(skeletonMin.x, bone.worldPosition.x);
        skeletonMin.y = std::min(skeletonMin.y, bone.worldPosition.y);
        skeletonMin.z = std::min(skeletonMin.z, bone.worldPosition.z);
        skeletonMax.x = std::max(skeletonMax.x, bone.worldPosition.x);
        skeletonMax.y = std::max(skeletonMax.y, bone.worldPosition.y);
        skeletonMax.z = std::max(skeletonMax.z, bone.worldPosition.z);
    }

    const Vec3 skeletonExtent = Subtract(skeletonMax, skeletonMin);
    const Vec3 modelExtent = Subtract(preview.boundsMax, preview.boundsMin);
    const std::array<int, 3> skeletonAxes = AxesSortedByExtent(skeletonExtent);
    const std::array<int, 3> modelAxes = AxesSortedByExtent(modelExtent);

    std::array<int, 3> sourceAxisForModel = {0, 1, 2};
    for (int rank = 0; rank < 3; ++rank) {
        sourceAxisForModel[modelAxes[rank]] = skeletonAxes[rank];
    }

    for (SkeletonBone& bone : preview.skeleton) {
        Vec3 aligned{};
        for (int modelAxis = 0; modelAxis < 3; ++modelAxis) {
            const int sourceAxis = sourceAxisForModel[modelAxis];
            const float sourceExtent = std::max(0.0001f, GetAxis(skeletonExtent, sourceAxis));
            const float modelExtentAxis = std::max(0.0001f, GetAxis(modelExtent, modelAxis));
            const float normalized =
                (GetAxis(bone.worldPosition, sourceAxis) - GetAxis(skeletonMin, sourceAxis)) / sourceExtent;
            const float component = GetAxis(preview.boundsMin, modelAxis) + normalized * modelExtentAxis;
            SetAxis(aligned, modelAxis, component);
        }
        bone.worldPosition = aligned;
        bone.bindWorldPosition = aligned;
    }
}

bool HasModelSkinning(const ModelPreview& preview) {
    return !preview.skinRecords.empty() && !preview.skinLookupGroups.empty();
}

bool HasExportableSkinning(const ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        return false;
    }
    return std::any_of(preview.vertices.begin(), preview.vertices.end(), [](const ModelVertex& vertex) {
        return vertex.skinInfluenceCount > 0;
    });
}

Vec3 TransformSkinInfluence(const SkeletonBone& bone, const SkinBlendInfluence& influence, bool bindPose) {
    const Vec3 bonePosition = bindPose ? bone.bindWorldPosition : bone.worldPosition;
    const Quat boneRotation = bindPose ? bone.bindWorldRotation : bone.worldRotation;
    return Add(bonePosition, Rotate(boneRotation, influence.localPosition));
}

Vec3 ComputeSkinRecordPosition(const ModelPreview& preview, const SkinBlendRecord& record, bool bindPose) {
    Vec3 position{};
    float totalWeight = 0.0f;
    for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
        const SkinBlendInfluence& influence = record.influences[influenceIndex];
        if (influence.bone < 0 ||
            influence.bone >= static_cast<int>(preview.skeleton.size()) ||
            influence.weight <= 0.0f) {
            continue;
        }
        position = Add(
            position,
            Multiply(TransformSkinInfluence(preview.skeleton[influence.bone], influence, bindPose), influence.weight));
        totalWeight += influence.weight;
    }

    if (totalWeight <= 0.0001f) {
        return {};
    }
    return Multiply(position, 1.0f / totalWeight);
}

bool ApplySkinDerivedSkeletonAlignment(ModelPreview& preview) {
    if (preview.skeleton.empty() || !HasModelSkinning(preview)) {
        return false;
    }

    std::vector<Vec3> bindRecordPositions(preview.skinRecords.size());
    std::vector<bool> validRecord(preview.skinRecords.size(), false);
    for (std::size_t recordIndex = 0; recordIndex < preview.skinRecords.size(); ++recordIndex) {
        const Vec3 position = ComputeSkinRecordPosition(preview, preview.skinRecords[recordIndex], true);
        if (std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z)) {
            bindRecordPositions[recordIndex] = position;
            validRecord[recordIndex] = preview.skinRecords[recordIndex].influenceCount > 0;
        }
    }

    Vec3 accumulatedOffset{};
    std::size_t sampleCount = 0;
    for (const SkinLookupGroup& group : preview.skinLookupGroups) {
        const std::size_t vertexCount = std::min(group.vertexCount, group.recordIndices.size());
        for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
            const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
            if (modelVertexIndex >= preview.vertices.size() ||
                recordIndex >= bindRecordPositions.size() ||
                !validRecord[recordIndex]) {
                continue;
            }
            accumulatedOffset = Add(
                accumulatedOffset,
                Subtract(preview.vertices[modelVertexIndex].bindPosition, bindRecordPositions[recordIndex]));
            ++sampleCount;
        }
    }

    if (sampleCount == 0) {
        return false;
    }

    const Vec3 offset = Multiply(accumulatedOffset, 1.0f / static_cast<float>(sampleCount));
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z)) {
        return false;
    }

    for (SkeletonBone& bone : preview.skeleton) {
        bone.bindWorldPosition = Add(bone.bindWorldPosition, offset);
        bone.worldPosition = bone.bindWorldPosition;
    }
    preview.skeletonWorldOffset = offset;
    preview.hasSkeletonWorldOffset = true;
    return true;
}

bool IsBoneAffectedBySelection(const ModelPreview& preview, int boneIndex, int selectedBone) {
    int current = boneIndex;
    while (current >= 0 && current < static_cast<int>(preview.skeleton.size())) {
        if (current == selectedBone) {
            return true;
        }
        current = preview.skeleton[current].parent;
    }
    return false;
}

void BuildGeneratedSkinWeights(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        preview.skinnedVertices = preview.vertices;
        return;
    }

    for (ModelVertex& vertex : preview.vertices) {
        vertex.skinInfluenceCount = 0;
        vertex.skinInfluences = {};

        std::array<std::pair<float, int>, 2> best = {
            std::make_pair(std::numeric_limits<float>::max(), -1),
            std::make_pair(std::numeric_limits<float>::max(), -1),
        };

        for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
            const SkeletonBone& bone = preview.skeleton[boneIndex];
            const float distanceSquared =
                bone.parent >= 0 && bone.parent < static_cast<int>(preview.skeleton.size())
                    ? DistancePointSegmentSquared(
                          vertex.bindPosition,
                          preview.skeleton[bone.parent].bindWorldPosition,
                          bone.bindWorldPosition)
                    : DistanceSquared(vertex.bindPosition, bone.bindWorldPosition);

            if (distanceSquared < best[0].first) {
                best[1] = best[0];
                best[0] = {distanceSquared, boneIndex};
            } else if (distanceSquared < best[1].first) {
                best[1] = {distanceSquared, boneIndex};
            }
        }

        if (best[0].second < 0) {
            continue;
        }

        const float firstDistance = std::sqrt(std::max(0.0f, best[0].first));
        const float secondDistance = best[1].second >= 0 ? std::sqrt(std::max(0.0f, best[1].first)) : std::numeric_limits<float>::max();
        const bool useSecond = best[1].second >= 0 && secondDistance <= firstDistance * 2.25f + 0.35f;
        const float firstInfluence = 1.0f / (firstDistance + 0.05f);
        const float secondInfluence = useSecond ? 1.0f / (secondDistance + 0.05f) : 0.0f;
        const float totalInfluence = std::max(0.0001f, firstInfluence + secondInfluence);

        vertex.skinInfluences[0] = {best[0].second, firstInfluence / totalInfluence};
        vertex.skinInfluenceCount = 1;
        if (useSecond) {
            vertex.skinInfluences[1] = {best[1].second, secondInfluence / totalInfluence};
            vertex.skinInfluenceCount = 2;
        }
    }

    preview.skinnedVertices = preview.vertices;
}

bool IsAnimationPlayable(const ModelAnimationClip& clip) {
    return clip.loaded && !clip.frames.empty() && clip.frameCount > 0 && clip.boneCount > 0;
}

bool HasActiveModelAnimation(const ModelPreview& preview) {
    return preview.selectedAnimation >= 0 &&
           preview.selectedAnimation < static_cast<int>(preview.animations.size()) &&
           IsAnimationPlayable(preview.animations[preview.selectedAnimation]);
}

void PlayModelAnimation(ModelPreview& preview, int animationIndex) {
    if (animationIndex < 0 || animationIndex >= static_cast<int>(preview.animations.size()) ||
        !IsAnimationPlayable(preview.animations[animationIndex])) {
        return;
    }

    preview.selectedAnimation = animationIndex;
    preview.animationPlaying = true;
    preview.animationTime = 0.0;
    preview.animationLastUpdateTime = ImGui::GetTime();
    preview.boneEditAngle = 0.0f;
    preview.rotatingBone = false;
}

void AdvanceModelAnimation(ModelPreview& preview) {
    const double now = ImGui::GetTime();
    if (!HasActiveModelAnimation(preview)) {
        preview.animationLastUpdateTime = now;
        return;
    }

    if (preview.animationLastUpdateTime <= 0.0) {
        preview.animationLastUpdateTime = now;
    }
    const double delta = std::max(0.0, now - preview.animationLastUpdateTime);
    preview.animationLastUpdateTime = now;

    if (!preview.animationPlaying) {
        return;
    }

    const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
    preview.animationTime += delta;
    const double duration = clip.fps > 0.0f
                                ? static_cast<double>(clip.frameCount) / static_cast<double>(clip.fps)
                                : 0.0;
    if (duration > 0.0 && preview.animationTime >= duration) {
        if (clip.loop) {
            preview.animationTime = std::fmod(preview.animationTime, duration);
        } else {
            preview.animationTime = duration;
            preview.animationPlaying = false;
        }
    }
}

Quat SampleAnimationRotation(const ModelAnimationClip& clip, int boneIndex, double animationTime) {
    if (!IsAnimationPlayable(clip) || boneIndex < 0 || boneIndex >= clip.boneCount) {
        return {};
    }

    double frameValue = animationTime * static_cast<double>(clip.fps);
    if (clip.loop) {
        frameValue = std::fmod(frameValue, static_cast<double>(clip.frameCount));
        if (frameValue < 0.0) {
            frameValue += static_cast<double>(clip.frameCount);
        }
    } else {
        frameValue = std::clamp(frameValue, 0.0, static_cast<double>(std::max(0, clip.frameCount - 1)));
    }

    int firstFrame = static_cast<int>(std::floor(frameValue));
    firstFrame = std::clamp(firstFrame, 0, clip.frameCount - 1);
    int secondFrame = firstFrame + 1;
    if (secondFrame >= clip.frameCount) {
        secondFrame = clip.loop ? 0 : firstFrame;
    }
    const float t = static_cast<float>(frameValue - static_cast<double>(firstFrame));
    if (boneIndex >= static_cast<int>(clip.frames[firstFrame].localRotations.size()) ||
        boneIndex >= static_cast<int>(clip.frames[secondFrame].localRotations.size())) {
        return {};
    }

    return Slerp(
        clip.frames[firstFrame].localRotations[boneIndex],
        clip.frames[secondFrame].localRotations[boneIndex],
        t);
}

bool ApplyModelAnimationPose(ModelPreview& preview) {
    if (!HasActiveModelAnimation(preview) || preview.skeleton.empty()) {
        return false;
    }

    const ModelAnimationClip& clip = preview.animations[preview.selectedAnimation];
    for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
        SkeletonBone& bone = preview.skeleton[boneIndex];
        if (boneIndex < clip.boneCount) {
            bone.localRotation = SampleAnimationRotation(clip, boneIndex, preview.animationTime);
        } else {
            bone.localRotation = bone.bindLocalRotation;
        }

        const bool hasParent =
            bone.parent >= 0 &&
            bone.parent < static_cast<int>(preview.skeleton.size()) &&
            bone.parent != bone.id;
        if (!hasParent) {
            bone.worldRotation = Normalize(bone.localRotation);
            bone.worldPosition = Add(bone.localPosition, preview.hasSkeletonWorldOffset ? preview.skeletonWorldOffset : Vec3{});
            continue;
        }

        const SkeletonBone& parent = preview.skeleton[bone.parent];
        bone.worldRotation = Multiply(parent.worldRotation, bone.localRotation);
        bone.worldPosition = Add(parent.worldPosition, Rotate(parent.worldRotation, bone.localPosition));
    }

    return true;
}

void UpdateModelSkinning(ModelPreview& preview) {
    if (preview.skeleton.empty()) {
        preview.skinnedVertices = preview.vertices;
        return;
    }

    const bool hasAnimation = ApplyModelAnimationPose(preview);
    if (!hasAnimation) {
        for (SkeletonBone& bone : preview.skeleton) {
            bone.localRotation = bone.bindLocalRotation;
            bone.worldPosition = bone.bindWorldPosition;
            bone.worldRotation = bone.bindWorldRotation;
        }
    }

    const bool hasEdit =
        preview.selectedBone >= 0 &&
        preview.selectedBone < static_cast<int>(preview.skeleton.size()) &&
        std::abs(preview.boneEditAngle) > 0.0001f;
    if (preview.skinnedVertices.size() != preview.vertices.size()) {
        preview.skinnedVertices = preview.vertices;
    }

    std::vector<bool> affected(preview.skeleton.size(), false);
    if (!hasEdit && !hasAnimation) {
        for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
            preview.skinnedVertices[index].position = preview.vertices[index].bindPosition;
            preview.skinnedVertices[index].normal = preview.vertices[index].bindNormal;
        }
        return;
    }

    if (hasEdit) {
        const Vec3 pivot = preview.skeleton[preview.selectedBone].worldPosition;
        const Quat editRotation = QuatFromAxisAngle(preview.boneRotationAxis, preview.boneEditAngle);
        for (int boneIndex = 0; boneIndex < static_cast<int>(preview.skeleton.size()); ++boneIndex) {
            affected[boneIndex] = IsBoneAffectedBySelection(preview, boneIndex, preview.selectedBone);
            if (affected[boneIndex]) {
                preview.skeleton[boneIndex].worldPosition =
                    Add(pivot, Rotate(editRotation, Subtract(preview.skeleton[boneIndex].worldPosition, pivot)));
                preview.skeleton[boneIndex].worldRotation =
                    Multiply(editRotation, preview.skeleton[boneIndex].worldRotation);
            }
        }
    }

    if (HasModelSkinning(preview)) {
        std::vector<Vec3> recordPositions(preview.skinRecords.size());
        for (std::size_t recordIndex = 0; recordIndex < preview.skinRecords.size(); ++recordIndex) {
            recordPositions[recordIndex] =
                ComputeSkinRecordPosition(preview, preview.skinRecords[recordIndex], false);
        }

        for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
            preview.skinnedVertices[index] = preview.vertices[index];
        }

        for (const SkinLookupGroup& group : preview.skinLookupGroups) {
            const std::size_t vertexCount = std::min(group.vertexCount, group.recordIndices.size());
            for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
                const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
                if (modelVertexIndex >= preview.vertices.size() ||
                    recordIndex >= preview.skinRecords.size() ||
                    recordIndex >= recordPositions.size() ||
                    preview.skinRecords[recordIndex].influenceCount <= 0) {
                    continue;
                }

                const ModelVertex& bindVertex = preview.vertices[modelVertexIndex];
                ModelVertex& skinnedVertex = preview.skinnedVertices[modelVertexIndex];
                skinnedVertex = bindVertex;
                skinnedVertex.position = recordPositions[recordIndex];

                Vec3 normal{};
                float totalWeight = 0.0f;
                const SkinBlendRecord& record = preview.skinRecords[recordIndex];
                for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
                    const SkinBlendInfluence& influence = record.influences[influenceIndex];
                    if (influence.bone < 0 ||
                        influence.bone >= static_cast<int>(preview.skeleton.size()) ||
                        influence.weight <= 0.0f) {
                        continue;
                    }
                    const SkeletonBone& bone = preview.skeleton[influence.bone];
                    const Quat bindToCurrent = Multiply(bone.worldRotation, Inverse(bone.bindWorldRotation));
                    normal = Add(normal, Multiply(Rotate(bindToCurrent, bindVertex.bindNormal), influence.weight));
                    totalWeight += influence.weight;
                }
                if (totalWeight > 0.0001f) {
                    skinnedVertex.normal = Normalize(Multiply(normal, 1.0f / totalWeight));
                }
            }
        }
        return;
    }

    for (std::size_t index = 0; index < preview.vertices.size(); ++index) {
        const ModelVertex& bindVertex = preview.vertices[index];
        ModelVertex& skinnedVertex = preview.skinnedVertices[index];
        skinnedVertex = bindVertex;

        if (bindVertex.skinInfluenceCount <= 0) {
            continue;
        }

        Vec3 position{};
        Vec3 normal{};
        float totalWeight = 0.0f;
        for (int influenceIndex = 0; influenceIndex < bindVertex.skinInfluenceCount; ++influenceIndex) {
            const SkinInfluence& influence = bindVertex.skinInfluences[influenceIndex];
            if (influence.bone < 0 || influence.bone >= static_cast<int>(affected.size()) || influence.weight <= 0.0f) {
                continue;
            }

            const SkeletonBone& bone = preview.skeleton[influence.bone];
            const Quat bindToCurrent = Multiply(bone.worldRotation, Inverse(bone.bindWorldRotation));
            const Vec3 influencedPosition =
                Add(bone.worldPosition, Rotate(bindToCurrent, Subtract(bindVertex.bindPosition, bone.bindWorldPosition)));
            const Vec3 influencedNormal = Rotate(bindToCurrent, bindVertex.bindNormal);
            position = Add(position, Multiply(influencedPosition, influence.weight));
            normal = Add(normal, Multiply(influencedNormal, influence.weight));
            totalWeight += influence.weight;
        }

        if (totalWeight > 0.0001f) {
            skinnedVertex.position = Multiply(position, 1.0f / totalWeight);
            skinnedVertex.normal = Normalize(Multiply(normal, 1.0f / totalWeight));
        }
    }
}

void AppendMd2GeometryBlocks(const std::vector<char>& bytes,
                             std::size_t chunkEnd,
                             std::size_t& blockOffset,
                             std::uint32_t blockCount,
                             ModelPreview& model,
                             const std::vector<std::uint32_t>& materialAlphaTypes,
                             const std::vector<float>& materialAlphas) {
    constexpr std::size_t kBlockHeaderSize = 0x64;
    constexpr std::size_t kMaxPreviewVertices = 100000;
    constexpr std::size_t kMaxPreviewTriangles = 100000;
    constexpr std::uint16_t kVertexFlagVector = 1U << 0U;
    constexpr std::uint16_t kVertexFlagNormal = 1U << 1U;
    constexpr std::uint16_t kVertexFlagUv = 1U << 3U;

    for (std::uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
        RequireModelRange(blockOffset, kBlockHeaderSize, chunkEnd, "MD2 geometry block is truncated.");

        const std::uint32_t vectorOffset = ReadU32Le(bytes, blockOffset + 0x3C);
        const std::uint32_t normalOffset = ReadU32Le(bytes, blockOffset + 0x40);
        const std::uint32_t texcoordOffset = ReadU32Le(bytes, blockOffset + 0x48);
        const std::uint32_t vertexStride = ReadU32Le(bytes, blockOffset + 0x4c);
        const std::uint32_t texcoordCount = ReadU32Le(bytes, blockOffset + 0x50);
        const std::uint16_t vertexFlags = ReadU16Le(bytes, blockOffset + 0x54);
        const std::uint16_t vertexCount = ReadU16Le(bytes, blockOffset + 0x56);

        if (vertexCount == 0 ||
            vertexStride < 12 || vertexStride > 256 ||
            (vertexFlags & kVertexFlagVector) == 0 ||
            vectorOffset + 12 > vertexStride) {
            throw std::runtime_error("MD2 geometry block has an unsupported layout.");
        }
        if (model.vertices.size() + vertexCount > kMaxPreviewVertices) {
            throw std::runtime_error("MD2 model is too large for the preview renderer.");
        }

        const std::uint32_t materialIndex = ReadU16Le(bytes, blockOffset + 0x04);
        std::uint32_t textureIndex = ReadU16Le(bytes, blockOffset + 0x20);
        if (textureIndex >= model.materials.size()) {
            textureIndex = model.materials.empty() ? 0 : textureIndex % static_cast<std::uint32_t>(model.materials.size());
        }
        const auto textureCoordinateIndex = static_cast<unsigned char>(bytes[blockOffset + 0x22]);
        const auto textureTiling = static_cast<unsigned char>(bytes[blockOffset + 0x23]);

        const std::size_t vertexStart = blockOffset + kBlockHeaderSize;
        const std::size_t vertexBytes = static_cast<std::size_t>(vertexCount) * vertexStride;
        RequireModelRange(vertexStart, vertexBytes, chunkEnd, "MD2 vertex data is truncated.");

        const std::uint32_t baseVertex = static_cast<std::uint32_t>(model.vertices.size());
        model.vertices.reserve(model.vertices.size() + vertexCount);
        for (std::uint16_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t vertexOffset = vertexStart + static_cast<std::size_t>(vertexIndex) * vertexStride;
            Vec3 position{
                ReadF32Le(bytes, vertexOffset + vectorOffset + 0),
                ReadF32Le(bytes, vertexOffset + vectorOffset + 4),
                ReadF32Le(bytes, vertexOffset + vectorOffset + 8),
            };
            Sanitize(position);

            Vec3 normal{0.0f, 1.0f, 0.0f};
            if ((vertexFlags & kVertexFlagNormal) != 0 && normalOffset + 12 <= vertexStride) {
                normal = {
                    ReadF32Le(bytes, vertexOffset + normalOffset + 0),
                    ReadF32Le(bytes, vertexOffset + normalOffset + 4),
                    ReadF32Le(bytes, vertexOffset + normalOffset + 8),
                };
                Sanitize(normal);
                normal = Normalize(normal);
                if (Length(normal) <= 0.0001f) {
                    normal = {0.0f, 1.0f, 0.0f};
                }
            }

            float u = 0.0f;
            float v = 0.0f;
            std::array<float, 4> uSets = {};
            std::array<float, 4> vSets = {};
            unsigned char uvSetCount = 1;
            if ((vertexFlags & kVertexFlagUv) != 0 && texcoordCount > 0 && texcoordOffset + 8 <= vertexStride) {
                uvSetCount = static_cast<unsigned char>(std::clamp<std::uint32_t>(texcoordCount, 1, 4));
                for (unsigned char uvSet = 0; uvSet < uvSetCount; ++uvSet) {
                    const std::uint32_t uvOffset = texcoordOffset + static_cast<std::uint32_t>(uvSet) * 8U;
                    if (uvOffset + 8 > vertexStride) {
                        break;
                    }
                    float setU = ReadF32Le(bytes, vertexOffset + uvOffset + 0);
                    float setV = ReadF32Le(bytes, vertexOffset + uvOffset + 4);
                    if (!std::isfinite(setU)) {
                        setU = 0.0f;
                    }
                    if (!std::isfinite(setV)) {
                        setV = 0.0f;
                    }
                    uSets[uvSet] = setU;
                    vSets[uvSet] = setV;
                }
                u = uSets[0];
                v = vSets[0];
            }

            ModelVertex vertex{position, normal, u, v, position, normal};
            vertex.uSets = uSets;
            vertex.vSets = vSets;
            vertex.uvSetCount = uvSetCount;
            model.vertices.push_back(vertex);
            IncludeInBounds(model, position);
        }

        const std::uint32_t sectionIndex = static_cast<std::uint32_t>(model.sections.size());
        const std::size_t triangleStart = model.triangles.size();
        std::size_t indexOffset = vertexStart + vertexBytes;
        RequireModelRange(indexOffset, 12, chunkEnd, "MD2 index header is truncated.");
        (void)ReadU32Le(bytes, indexOffset);
        const std::uint32_t fillType = ReadU32Le(bytes, indexOffset + 4);
        const std::uint32_t indexCount = ReadU32Le(bytes, indexOffset + 8);
        indexOffset += 12;

        if (indexCount == 0 || indexOffset + static_cast<std::size_t>(indexCount) * 2 > chunkEnd) {
            throw std::runtime_error("MD2 index data is truncated.");
        }
        if (model.triangles.size() + indexCount > kMaxPreviewTriangles) {
            throw std::runtime_error("MD2 model has too many triangles for the preview renderer.");
        }

        std::vector<std::uint32_t> indices(indexCount);
        for (std::uint32_t index = 0; index < indexCount; ++index) {
            indices[indexCount - 1U - index] = ReadU16Le(bytes, indexOffset + static_cast<std::size_t>(index) * 2);
        }

        const std::uint32_t material = textureIndex;
        if (fillType == 0) {
            for (std::uint32_t index = 0; index + 2 < indexCount; index += 3) {
                const std::uint32_t a = indices[index + 0];
                const std::uint32_t b = indices[index + 1];
                const std::uint32_t c = indices[index + 2];
                if (a < vertexCount && b < vertexCount && c < vertexCount) {
                    model.triangles.push_back({baseVertex + a, baseVertex + b, baseVertex + c, material, sectionIndex});
                }
            }
        } else {
            for (std::uint32_t index = 0; index + 2 < indexCount; ++index) {
                const std::uint32_t a = (index % 2U) == 0U ? indices[index + 0] : indices[index + 1];
                const std::uint32_t b = (index % 2U) == 0U ? indices[index + 1] : indices[index + 0];
                const std::uint32_t c = indices[index + 2];
                if (a == b || b == c || a == c) {
                    continue;
                }
                if (a < vertexCount && b < vertexCount && c < vertexCount) {
                    model.triangles.push_back({baseVertex + a, baseVertex + b, baseVertex + c, material, sectionIndex});
                }
            }
        }
        indexOffset += static_cast<std::size_t>(indexCount) * 2;

        ModelSection section;
        section.textureIndex = textureIndex;
        section.materialIndex = materialIndex;
        section.textureTiling = textureTiling;
        section.textureCoordinateIndex = textureCoordinateIndex;
        if (materialIndex < materialAlphaTypes.size()) {
            section.alphaType = materialAlphaTypes[materialIndex];
        }
        if (materialIndex < materialAlphas.size()) {
            const float alpha = 1.0f - materialAlphas[materialIndex];
            if (std::isfinite(alpha)) {
                section.alpha = std::clamp(alpha, 0.0f, 1.0f);
            }
        }
        section.vertexStart = baseVertex;
        section.vertexCount = vertexCount;
        section.triangleStart = triangleStart;
        section.triangleCount = model.triangles.size() - triangleStart;
        section.name = "Mesh " + std::to_string(model.sections.size() + 1);
        if (textureIndex < model.materials.size()) {
            const std::filesystem::path textureName(model.materials[textureIndex].path);
            if (!textureName.stem().string().empty()) {
                section.name += " - " + textureName.stem().string();
            }
        }
        model.sections.push_back(std::move(section));
        blockOffset = indexOffset;
    }
}

void NormalizeSkinBlendRecord(SkinBlendRecord& record) {
    float totalWeight = 0.0f;
    for (int index = 0; index < record.influenceCount; ++index) {
        if (record.influences[index].bone < 0 || record.influences[index].weight <= 0.0f) {
            record.influences[index].weight = 0.0f;
            continue;
        }
        totalWeight += record.influences[index].weight;
    }

    if (totalWeight <= 0.0001f) {
        record = {};
        return;
    }

    for (int index = 0; index < record.influenceCount; ++index) {
        record.influences[index].weight /= totalWeight;
    }
}

void AssignSkinRecordToVertex(ModelVertex& vertex, const SkinBlendRecord& record, int recordIndex) {
    vertex.skinRecord = recordIndex;
    vertex.skinInfluences = {};
    vertex.skinInfluenceCount = 0;
    for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
        const SkinBlendInfluence& influence = record.influences[influenceIndex];
        if (influence.bone < 0 || influence.weight <= 0.0f) {
            continue;
        }
        vertex.skinInfluences[vertex.skinInfluenceCount++] = {influence.bone, influence.weight};
        if (vertex.skinInfluenceCount >= static_cast<int>(vertex.skinInfluences.size())) {
            break;
        }
    }
}

void DecodeMd2SkinChunk(const std::vector<char>& bytes, std::size_t skinOffset, ModelPreview& model) {
    constexpr std::size_t kBlendRecordSize = 48;
    RequireModelRange(skinOffset, 8, bytes.size(), "MD2 SKN0 chunk header is truncated.");
    const std::size_t payloadSize = ReadU32Le(bytes, skinOffset + 4);
    const std::size_t chunkEnd = skinOffset + 8 + payloadSize;
    RequireModelRange(skinOffset, 8 + payloadSize, bytes.size(), "MD2 SKN0 chunk is truncated.");

    std::size_t cursor = skinOffset + 8;
    RequireModelRange(cursor, 8, chunkEnd, "MD2 SKN0 blend header is truncated.");
    const std::uint32_t recordCount = ReadU32Le(bytes, cursor);
    const std::uint32_t recordBytes = ReadU32Le(bytes, cursor + 4);
    cursor += 8;

    if (recordCount == 0 || recordCount > 200000 ||
        recordBytes != static_cast<std::uint64_t>(recordCount) * kBlendRecordSize) {
        throw std::runtime_error("MD2 SKN0 blend records have an unsupported layout.");
    }
    RequireModelRange(cursor, recordBytes, chunkEnd, "MD2 SKN0 blend records are truncated.");

    model.skinRecords.clear();
    model.skinRecords.reserve(recordCount);
    for (std::uint32_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::size_t recordOffset = cursor + static_cast<std::size_t>(recordIndex) * kBlendRecordSize;
        const std::uint16_t influenceCount = ReadU16Le(bytes, recordOffset + 44);
        if (influenceCount < 1 || influenceCount > 2) {
            throw std::runtime_error("MD2 SKN0 blend record influence count is unsupported.");
        }

        SkinBlendRecord record;
        record.influenceCount = static_cast<int>(influenceCount);
        record.influences[0] = {
            {
                ReadF32Le(bytes, recordOffset + 0),
                ReadF32Le(bytes, recordOffset + 4),
                ReadF32Le(bytes, recordOffset + 8),
            },
            static_cast<int>(ReadS16Le(bytes, recordOffset + 14)),
            influenceCount == 1 ? 1.0f : static_cast<float>(ReadS16Le(bytes, recordOffset + 12)) / 256.0f,
        };
        if (influenceCount > 1) {
            record.influences[1] = {
                {
                    ReadF32Le(bytes, recordOffset + 16),
                    ReadF32Le(bytes, recordOffset + 20),
                    ReadF32Le(bytes, recordOffset + 24),
                },
                static_cast<int>(ReadS16Le(bytes, recordOffset + 30)),
                static_cast<float>(ReadS16Le(bytes, recordOffset + 28)) / 256.0f,
            };
        }
        for (int influenceIndex = 0; influenceIndex < record.influenceCount; ++influenceIndex) {
            Sanitize(record.influences[influenceIndex].localPosition);
        }
        NormalizeSkinBlendRecord(record);
        model.skinRecords.push_back(record);
    }
    cursor += recordBytes;

    RequireModelRange(cursor, 4, chunkEnd, "MD2 SKN0 lookup header is truncated.");
    const std::uint32_t groupCount = ReadU32Le(bytes, cursor);
    cursor += 4;
    if (groupCount == 0 || groupCount > 4096) {
        throw std::runtime_error("MD2 SKN0 lookup group count is unsupported.");
    }

    model.skinLookupGroups.clear();
    model.skinLookupGroups.reserve(groupCount);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        RequireModelRange(cursor, 8, chunkEnd, "MD2 SKN0 lookup group is truncated.");
        const std::uint32_t declaredVertexCount = ReadU32Le(bytes, cursor);
        const std::uint32_t lookupBytes = ReadU32Le(bytes, cursor + 4);
        cursor += 8;

        if (lookupBytes % 2 != 0 || lookupBytes / 2 < declaredVertexCount) {
            throw std::runtime_error("MD2 SKN0 lookup table has an unsupported layout.");
        }
        RequireModelRange(cursor, lookupBytes, chunkEnd, "MD2 SKN0 lookup table is truncated.");

        SkinLookupGroup group;
        if (groupIndex < model.sections.size()) {
            const ModelSection& section = model.sections[groupIndex];
            group.vertexStart = section.vertexStart;
            group.vertexCount = std::min<std::size_t>(declaredVertexCount, section.vertexCount);
        }
        const std::size_t lookupCount = lookupBytes / 2;
        group.recordIndices.reserve(lookupCount);
        for (std::size_t lookupIndex = 0; lookupIndex < lookupCount; ++lookupIndex) {
            group.recordIndices.push_back(ReadU16Le(bytes, cursor + lookupIndex * 2));
        }

        if (group.vertexCount > group.recordIndices.size()) {
            group.vertexCount = group.recordIndices.size();
        }
        for (std::size_t vertexIndex = 0; vertexIndex < group.vertexCount; ++vertexIndex) {
            const std::size_t modelVertexIndex = group.vertexStart + vertexIndex;
            const std::uint16_t recordIndex = group.recordIndices[vertexIndex];
            if (modelVertexIndex < model.vertices.size() && recordIndex < model.skinRecords.size()) {
                AssignSkinRecordToVertex(model.vertices[modelVertexIndex], model.skinRecords[recordIndex], recordIndex);
            }
        }

        model.skinLookupGroups.push_back(std::move(group));
        cursor += lookupBytes;
    }
}

std::size_t FindChunkAfter(const std::vector<char>& bytes, std::size_t offset, const char* fourCc) {
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t payloadSize = ReadU32Le(bytes, offset + 4);
        const std::size_t nextOffset = offset + 8 + static_cast<std::size_t>(payloadSize);
        if (nextOffset > bytes.size() || nextOffset <= offset) {
            break;
        }
        if (std::memcmp(bytes.data() + offset, fourCc, 4) == 0) {
            return offset;
        }
        offset = nextOffset;
    }

    return static_cast<std::size_t>(-1);
}

void DecodeMdlHeaderTables(const std::vector<char>& bytes,
                           std::size_t geoOffset,
                           ModelPreview& model,
                           std::vector<std::uint32_t>& materialAlphaTypes,
                           std::vector<float>& materialAlphas) {
    if (bytes.size() < 8) {
        throw std::runtime_error("MD2 model header is truncated.");
    }

    const bool mdl1 = std::memcmp(bytes.data(), "MDL1", 4) == 0;
    const bool mdl2 = std::memcmp(bytes.data(), "MDL2", 4) == 0;
    if (!mdl1 && !mdl2) {
        throw std::runtime_error("Not a supported LR2 MD2 model.");
    }

    const std::size_t payloadSize = ReadU32Le(bytes, 4);
    std::size_t chunkEnd = 8 + payloadSize;
    if (chunkEnd > bytes.size()) {
        throw std::runtime_error("MD2 model header chunk is truncated.");
    }
    if (geoOffset != static_cast<std::size_t>(-1) && geoOffset > 8) {
        chunkEnd = std::min(chunkEnd, geoOffset);
    }

    std::size_t cursor = 8 + 12 + 8;
    RequireModelRange(cursor, 4, chunkEnd, "MD2 model header is truncated.");
    const std::uint32_t hasBoundingBox = ReadU32Le(bytes, cursor);
    cursor += 4;

    if (hasBoundingBox != 0) {
        RequireModelRange(cursor, 12 + 12 + 12 + 4, chunkEnd, "MD2 bounding box header is truncated.");
        cursor += 12 + 12 + 12 + 4;
    }

    RequireModelRange(cursor, 16 + 48, chunkEnd, "MD2 material header is truncated.");
    cursor += 16 + 48;

    RequireModelRange(cursor, 4, chunkEnd, "MD2 texture table count is truncated.");
    const std::uint32_t textureCount = ReadU32Le(bytes, cursor);
    cursor += 4;

    constexpr std::size_t kTextureRecordSize = 256 + 8;
    if (textureCount > 2048) {
        throw std::runtime_error("MD2 texture table count is unsupported.");
    }
    RequireModelRange(
        cursor,
        static_cast<std::size_t>(textureCount) * kTextureRecordSize,
        chunkEnd,
        "MD2 texture table is truncated.");

    model.materials.reserve(textureCount);
    for (std::uint32_t textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
        const std::size_t recordOffset = cursor + static_cast<std::size_t>(textureIndex) * kTextureRecordSize;
        std::string path = ReadFixedString(bytes, recordOffset, 256);
        std::replace(path.begin(), path.end(), '/', '\\');
        model.materials.push_back({Trim(path), {}, false});
    }
    cursor += static_cast<std::size_t>(textureCount) * kTextureRecordSize;

    if (cursor + 4 > chunkEnd) {
        return;
    }

    const std::uint32_t materialCount = ReadU32Le(bytes, cursor);
    cursor += 4;
    if (materialCount > 4096) {
        throw std::runtime_error("MD2 material table count is unsupported.");
    }

    const std::size_t materialRecordSize = mdl2 ? 88 : 28;
    RequireModelRange(
        cursor,
        static_cast<std::size_t>(materialCount) * materialRecordSize,
        chunkEnd,
        "MD2 material table is truncated.");

    materialAlphaTypes.reserve(materialCount);
    materialAlphas.reserve(materialCount);
    for (std::uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        const std::size_t materialOffset = cursor + static_cast<std::size_t>(materialIndex) * materialRecordSize;
        if (mdl2) {
            materialAlphas.push_back(ReadF32Le(bytes, materialOffset + 68));
            materialAlphaTypes.push_back(ReadU32Le(bytes, materialOffset + 72));
        } else {
            materialAlphas.push_back(0.0f);
            materialAlphaTypes.push_back(ReadU32Le(bytes, materialOffset));
        }
    }
}

ModelPreview DecodeMd2Model(const std::vector<char>& bytes) {
    if (bytes.size() >= 12 && std::memcmp(bytes.data(), "MDL0", 4) == 0) {
        throw std::runtime_error("MDL0/GEO0 models are not supported yet.");
    }
    if (bytes.size() < 12 ||
        (std::memcmp(bytes.data(), "MDL1", 4) != 0 &&
         std::memcmp(bytes.data(), "MDL2", 4) != 0)) {
        throw std::runtime_error("Not a supported LR2 MD2 model.");
    }

    std::size_t geoOffset = FindChunkAfter(bytes, 0, "GEO1");
    if (geoOffset == static_cast<std::size_t>(-1)) {
        geoOffset = FindFourCc(bytes, "GEO1");
    }
    if (geoOffset == static_cast<std::size_t>(-1)) {
        if (FindFourCc(bytes, "GEO0") != static_cast<std::size_t>(-1)) {
            throw std::runtime_error("MD2 model uses GEO0, which is not supported yet.");
        }
        throw std::runtime_error("MD2 model does not contain a GEO1 render mesh.");
    }
    RequireModelRange(geoOffset, 0x20, bytes.size(), "MD2 GEO1 header is truncated.");

    const std::size_t payloadSize = ReadU32Le(bytes, geoOffset + 4);
    const std::size_t chunkEnd = geoOffset + 8 + payloadSize;
    RequireModelRange(geoOffset, 8 + payloadSize, bytes.size(), "MD2 GEO1 chunk is truncated.");

    const std::uint32_t primaryBlockCount = ReadU32Le(bytes, geoOffset + 0x14);
    if (primaryBlockCount == 0 || primaryBlockCount > 1024) {
        throw std::runtime_error("MD2 GEO1 block count is unsupported.");
    }

    ModelPreview model;
    const float maxFloat = std::numeric_limits<float>::max();
    model.boundsMin = {maxFloat, maxFloat, maxFloat};
    model.boundsMax = {-maxFloat, -maxFloat, -maxFloat};
    std::vector<std::uint32_t> materialAlphaTypes;
    std::vector<float> materialAlphas;

    DecodeMdlHeaderTables(bytes, geoOffset, model, materialAlphaTypes, materialAlphas);

    std::size_t blockOffset = geoOffset + 0x20;
    AppendMd2GeometryBlocks(bytes, chunkEnd, blockOffset, primaryBlockCount, model, materialAlphaTypes, materialAlphas);

    const std::size_t skinOffset = FindChunkAfter(bytes, chunkEnd, "SKN0");
    if (skinOffset != static_cast<std::size_t>(-1)) {
        try {
            DecodeMd2SkinChunk(bytes, skinOffset, model);
        } catch (const std::exception& error) {
            model.skinRecords.clear();
            model.skinLookupGroups.clear();
            for (ModelVertex& vertex : model.vertices) {
                vertex.skinRecord = -1;
                vertex.skinInfluenceCount = 0;
                vertex.skinInfluences = {};
            }
        }
    }

    if (model.vertices.empty() || model.triangles.empty()) {
        throw std::runtime_error("MD2 model did not contain drawable triangles.");
    }

    model.center = {
        (model.boundsMin.x + model.boundsMax.x) * 0.5f,
        (model.boundsMin.y + model.boundsMax.y) * 0.5f,
        (model.boundsMin.z + model.boundsMax.z) * 0.5f,
    };

    model.radius = 0.0f;
    for (const ModelVertex& vertex : model.vertices) {
        model.radius = std::max(model.radius, Length(Subtract(vertex.position, model.center)));
    }
    if (model.radius <= 0.0001f || !std::isfinite(model.radius)) {
        model.radius = 1.0f;
    }

    return model;
}
