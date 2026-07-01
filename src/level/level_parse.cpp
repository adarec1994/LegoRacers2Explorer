void IncludeInLevelBounds(LevelPreview& preview, Vec3 position, bool& initialized) {
    if (!initialized) {
        preview.boundsMin = position;
        preview.boundsMax = position;
        initialized = true;
        return;
    }

    preview.boundsMin.x = std::min(preview.boundsMin.x, position.x);
    preview.boundsMin.y = std::min(preview.boundsMin.y, position.y);
    preview.boundsMin.z = std::min(preview.boundsMin.z, position.z);
    preview.boundsMax.x = std::max(preview.boundsMax.x, position.x);
    preview.boundsMax.y = std::max(preview.boundsMax.y, position.y);
    preview.boundsMax.z = std::max(preview.boundsMax.z, position.z);
}

void RecomputeLevelBounds(LevelPreview& preview) {
    bool initialized = false;
    for (const LevelVertex& vertex : preview.vertices) {
        IncludeInLevelBounds(preview, vertex.position, initialized);
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition) {
            IncludeInLevelBounds(preview, object.position, initialized);
        }
    }

    if (!initialized) {
        preview.boundsMin = {-1.0f, -1.0f, -1.0f};
        preview.boundsMax = {1.0f, 1.0f, 1.0f};
    }

    preview.center = {
        (preview.boundsMin.x + preview.boundsMax.x) * 0.5f,
        (preview.boundsMin.y + preview.boundsMax.y) * 0.5f,
        (preview.boundsMin.z + preview.boundsMax.z) * 0.5f,
    };

    preview.radius = 0.0f;
    for (const LevelVertex& vertex : preview.vertices) {
        preview.radius = std::max(preview.radius, Length(Subtract(vertex.position, preview.center)));
    }
    for (const WorldObject& object : preview.objects) {
        if (object.hasPosition) {
            preview.radius = std::max(preview.radius, Length(Subtract(object.position, preview.center)));
        }
    }
    if (preview.radius <= 0.0001f || !std::isfinite(preview.radius)) {
        preview.radius = 1.0f;
    }
}

LevelPreview DecodeTdfTerrain(const std::vector<char>& bytes, std::string name, std::string path) {
    constexpr int kGridLine = 32;
    constexpr int kGridCount = kGridLine * kGridLine;
    constexpr int kChunkWidth = 16;
    constexpr int kVertexChunk = kChunkWidth + 1;
    constexpr std::size_t kHeaderSize = 0x20;
    constexpr std::size_t kHeightPointSize = 8;
    constexpr std::size_t kTerrainGridInfoSize = 0x120;
    constexpr std::size_t kGridStartOffset = 0x0C;
    constexpr std::size_t kGridMapInfoOffset = 0x90;

    if (bytes.size() < kHeaderSize || std::memcmp(bytes.data(), "TDF1", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 TDF terrain file.");
    }

    const int terrainWidth = static_cast<int>(ReadU32Le(bytes, 0x08));
    const int terrainDepth = static_cast<int>(ReadU32Le(bytes, 0x0C));
    float filterScale = ReadF32Le(bytes, 0x10);
    int allocatedMipLevels = static_cast<int>(ReadU32Le(bytes, 0x14));
    int mipLevels = static_cast<int>(ReadU32Le(bytes, 0x18));

    if (terrainWidth <= 0 || terrainDepth <= 0 || allocatedMipLevels <= 0 || mipLevels <= 0) {
        throw std::runtime_error("TDF terrain header dimensions are unsupported.");
    }
    if (!std::isfinite(filterScale) || std::abs(filterScale) < 0.000001f) {
        filterScale = 1.0f / 64.0f;
    }

    allocatedMipLevels = std::clamp(allocatedMipLevels, 1, 4);
    mipLevels = std::clamp(mipLevels, 1, 4);

    const int stepX = terrainWidth / kGridLine;
    const int stepY = terrainDepth / kGridLine;
    if (stepX <= 0 || stepY <= 0) {
        throw std::runtime_error("TDF terrain grid dimensions are unsupported.");
    }

    std::array<std::size_t, 4> pointOffsets = {};
    std::size_t cursor = kHeaderSize;
    int mipWidth = stepX + 1;
    int mipHeight = stepY + 1;
    for (int mipLevel = 0; mipLevel < allocatedMipLevels; ++mipLevel) {
        pointOffsets[static_cast<std::size_t>(mipLevel)] = cursor;
        const std::size_t pointCount =
            static_cast<std::size_t>(mipWidth) *
            static_cast<std::size_t>(mipHeight) *
            static_cast<std::size_t>(kGridCount);
        const std::size_t pointBytes = pointCount * kHeightPointSize;
        if (cursor > bytes.size() || pointBytes > bytes.size() - cursor) {
            throw std::runtime_error("TDF terrain height points are truncated.");
        }
        cursor += pointBytes;
        mipWidth = ((mipWidth - 1) / 2) + 1;
        mipHeight = ((mipHeight - 1) / 2) + 1;
    }

    mipWidth = stepX + 1;
    mipHeight = stepY + 1;
    for (int mipLevel = 0; mipLevel < 4; ++mipLevel) {
        if (mipLevel + 1 >= mipLevels) {
            break;
        }
        const std::size_t edgeCount =
            static_cast<std::size_t>(mipWidth + mipHeight) *
            2U *
            static_cast<std::size_t>(kGridCount);
        const std::size_t edgeBytes = edgeCount * sizeof(std::uint16_t);
        if (cursor > bytes.size() || edgeBytes > bytes.size() - cursor) {
            throw std::runtime_error("TDF terrain edge data is truncated.");
        }
        cursor += edgeBytes;
        mipWidth = ((mipWidth - 1) / 2) + 1;
        mipHeight = ((mipHeight - 1) / 2) + 1;
    }

    const std::size_t gridInfoOffset = cursor;
    const std::size_t gridInfoBytes = static_cast<std::size_t>(kGridCount) * kTerrainGridInfoSize;
    if (gridInfoOffset > bytes.size() || gridInfoBytes > bytes.size() - gridInfoOffset) {
        throw std::runtime_error("TDF terrain grid info is truncated.");
    }
    const std::size_t gridOffsetTableOffset = gridInfoOffset + gridInfoBytes;
    const std::size_t gridOffsetTableBytes = static_cast<std::size_t>(kGridCount) * sizeof(std::uint32_t);
    if (gridOffsetTableOffset > bytes.size() || gridOffsetTableBytes > bytes.size() - gridOffsetTableOffset) {
        throw std::runtime_error("TDF terrain grid offset table is truncated.");
    }

    LevelPreview preview;
    preview.open = true;
    preview.world = false;
    preview.name = std::move(name);
    preview.path = std::move(path);

    const std::size_t estimatedVertices =
        static_cast<std::size_t>(kGridCount) *
        static_cast<std::size_t>(kVertexChunk) *
        static_cast<std::size_t>(kVertexChunk);
    preview.vertices.reserve(estimatedVertices);
    preview.triangles.reserve(static_cast<std::size_t>(kGridCount) * kChunkWidth * kChunkWidth * 2U);

    int skippedTriangles = 0;
    const float terrainCenterX = static_cast<float>(kChunkWidth * kGridLine) * 0.5f;
    const float terrainCenterZ = static_cast<float>(kChunkWidth * kGridLine) * 0.5f;
    const float uDenominator = static_cast<float>(kChunkWidth);
    const float vDenominator = static_cast<float>(kChunkWidth);
    for (int chunkIndex = 0; chunkIndex < kGridCount; ++chunkIndex) {
            const std::uint32_t gridRelativeOffset =
                ReadU32Le(bytes, gridOffsetTableOffset + static_cast<std::size_t>(chunkIndex) * sizeof(std::uint32_t));
            const std::size_t gridOffset = gridInfoOffset + static_cast<std::size_t>(gridRelativeOffset);
            if (gridOffset < gridInfoOffset || gridOffset + kTerrainGridInfoSize > gridOffsetTableOffset) {
                continue;
            }
            const int startX = static_cast<int>(ReadS16Le(bytes, gridOffset + kGridStartOffset + 0));
            const int startY = static_cast<int>(ReadS16Le(bytes, gridOffset + kGridStartOffset + 2));
            const std::uint32_t materialKey =
                static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x118])) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x119])) << 8) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x11A])) << 16) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[gridOffset + 0x11B])) << 24);

            const std::size_t mapOffset = gridOffset + kGridMapInfoOffset;
            const std::uint32_t pointByteOffset = ReadU32Le(bytes, mapOffset + 0);
            if (pointByteOffset % kHeightPointSize != 0) {
                continue;
            }

            const std::size_t pointBaseOffset = pointOffsets[0] + static_cast<std::size_t>(pointByteOffset);
            const std::size_t pointBytes =
                static_cast<std::size_t>(kVertexChunk) *
                static_cast<std::size_t>(kVertexChunk) *
                kHeightPointSize;
            if (pointBaseOffset > bytes.size() || pointBytes > bytes.size() - pointBaseOffset) {
                continue;
            }

            const std::uint32_t baseVertex = static_cast<std::uint32_t>(preview.vertices.size());
            for (int y = 0; y < kVertexChunk; ++y) {
                for (int x = 0; x < kVertexChunk; ++x) {
                    const std::size_t pointOffset =
                        pointBaseOffset +
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(kVertexChunk) + static_cast<std::size_t>(x)) *
                            kHeightPointSize;

                    const std::uint16_t rawHeight = ReadU16Le(bytes, pointOffset + 0);
                    const auto normalX = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 2])));
                    const auto normalY = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 3])));
                    const auto normalZ = static_cast<float>(
                        static_cast<std::int8_t>(static_cast<unsigned char>(bytes[pointOffset + 4])));
                    const unsigned char flags = static_cast<unsigned char>(bytes[pointOffset + 5]);
                    const std::uint16_t mixRatios = ReadU16Le(bytes, pointOffset + 6);

                    const float sourceX = static_cast<float>(startX + x);
                    const float sourceZ = static_cast<float>(startY + y);
                    const float worldX = sourceX - terrainCenterX;
                    const float worldZ = sourceZ - terrainCenterZ;
                    const float worldY = static_cast<float>(rawHeight) * filterScale;

                    LevelVertex vertex;
                    vertex.position = {worldX, worldY, worldZ};
                    vertex.normal = Normalize(Vec3{normalX, normalY, normalZ});
                    if (Length(vertex.normal) <= 0.0001f) {
                        vertex.normal = {0.0f, 1.0f, 0.0f};
                    }
                    vertex.u = sourceX / uDenominator;
                    vertex.v = sourceZ / vDenominator;
                    vertex.mix = {
                        static_cast<unsigned char>(((mixRatios >> 0x0) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0x4) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0x8) & 0xfU) * 0x11U),
                        static_cast<unsigned char>(((mixRatios >> 0xc) & 0xfU) * 0x11U),
                    };
                    vertex.visible = (flags & 0x80U) == 0;
                    preview.vertices.push_back(vertex);
                }
            }

            for (int y = 0; y < kChunkWidth; ++y) {
                for (int x = 0; x < kChunkWidth; ++x) {
                    const std::uint32_t a = baseVertex + static_cast<std::uint32_t>(y * kVertexChunk + x);
                    const std::uint32_t b = a + 1;
                    const std::uint32_t c = baseVertex + static_cast<std::uint32_t>((y + 1) * kVertexChunk + x);
                    const std::uint32_t d = c + 1;
                    if (!preview.vertices[d].visible) {
                        skippedTriangles += 2;
                        continue;
                    }
                    if (y % 2 == 0) {
                        preview.triangles.push_back({a, b, c, materialKey});
                        preview.triangles.push_back({c, b, d, materialKey});
                    } else {
                        preview.triangles.push_back({a, b, d, materialKey});
                        preview.triangles.push_back({a, d, c, materialKey});
                    }
                }
            }
    }

    RecomputeLevelBounds(preview);
    preview.status =
        std::to_string(kGridLine) + "x" + std::to_string(kGridLine) +
        " tiled LOD0 terrain, " + std::to_string(preview.vertices.size()) + " vertices, " +
        std::to_string(preview.triangles.size()) + " triangles";
    if (skippedTriangles > 0) {
        preview.status += ", " + std::to_string(skippedTriangles) + " cutout triangles skipped";
    }
    return preview;
}

bool IsPlausibleWorldPosition(Vec3 position) {
    return std::isfinite(position.x) &&
           std::isfinite(position.y) &&
           std::isfinite(position.z) &&
           std::abs(position.x) < 5000.0f &&
           std::abs(position.y) < 2500.0f &&
           std::abs(position.z) < 5000.0f &&
           (std::abs(position.x) + std::abs(position.y) + std::abs(position.z)) > 0.001f;
}

std::string NormalizeWorldAssetPath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    path = Trim(std::move(path));
    while (!path.empty() && (path.back() == '\\' || path.back() == '/')) {
        path.pop_back();
    }
    return path;
}

bool IsWorldAssetPath(const std::string& path) {
    const std::string lower = ToLower(path);
    return lower == "game data" || lower.rfind("game data\\", 0) == 0;
}

bool IsWorldMd2Path(const std::string& path) {
    if (!IsWorldAssetPath(path)) {
        return false;
    }
    return ToLower(std::filesystem::path(path).extension().string()) == ".md2";
}

void AddUniqueWorldAssetPath(std::vector<std::string>& paths, std::string path) {
    path = NormalizeWorldAssetPath(std::move(path));
    if (!IsWorldAssetPath(path)) {
        return;
    }

    const std::string wanted = ToLower(path);
    const auto found = std::find_if(paths.begin(), paths.end(), [&](const std::string& existing) {
        return ToLower(existing) == wanted;
    });
    if (found == paths.end()) {
        paths.push_back(std::move(path));
    }
}

std::vector<std::string> ExtractWorldRecordPaths(const std::vector<char>& bytes,
                                                 std::size_t start,
                                                 std::size_t end) {
    std::vector<std::string> paths;
    if (start >= end || start >= bytes.size()) {
        return paths;
    }

    end = std::min(end, bytes.size());
    constexpr char kNeedle[] = "game data";
    for (std::size_t offset = start; offset + 9 <= end; ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < 9; ++index) {
            const char source = static_cast<char>(std::tolower(static_cast<unsigned char>(bytes[offset + index])));
            if (source != kNeedle[index]) {
                matched = false;
                break;
            }
        }
        if (!matched || offset + 9 >= end || (bytes[offset + 9] != '\\' && bytes[offset + 9] != '/')) {
            continue;
        }

        std::string path;
        for (std::size_t pathOffset = offset; pathOffset < end && path.size() < 260; ++pathOffset) {
            const auto character = static_cast<unsigned char>(bytes[pathOffset]);
            if (character == '\0') {
                break;
            }
            if (character < 32 || character > 126) {
                break;
            }
            path.push_back(character == '/' ? '\\' : static_cast<char>(character));
        }
        const std::size_t consumed = path.size();
        AddUniqueWorldAssetPath(paths, std::move(path));
        if (consumed > 0) {
            offset += consumed - 1;
        }
    }
    return paths;
}

std::string FirstWorldMd2Path(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        if (IsWorldMd2Path(path)) {
            return path;
        }
    }
    return {};
}

bool TryReadWorldVec3(const std::vector<char>& bytes,
                      std::size_t recordOffset,
                      std::size_t recordEnd,
                      std::size_t relativeOffset,
                      Vec3& position,
                      bool allowZero = false) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return false;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 12) {
        return false;
    }

    const std::size_t offset = recordOffset + relativeOffset;
    position = {
        ReadF32Le(bytes, offset + 0),
        ReadF32Le(bytes, offset + 4),
        ReadF32Le(bytes, offset + 8),
    };
    Sanitize(position);
    if (IsPlausibleWorldPosition(position)) {
        return true;
    }
    return allowZero &&
           std::isfinite(position.x) &&
           std::isfinite(position.y) &&
           std::isfinite(position.z) &&
           std::abs(position.x) < 5000.0f &&
           std::abs(position.y) < 2500.0f &&
           std::abs(position.z) < 5000.0f;
}

bool TryReadAiSectionPosition(const std::vector<char>& bytes,
                              std::size_t recordOffset,
                              std::size_t recordEnd,
                              Vec3& position) {
    Vec3 sum = {};
    int count = 0;
    for (int pointIndex = 0; pointIndex < 8; ++pointIndex) {
        Vec3 point = {};
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, 0xA4 + static_cast<std::size_t>(pointIndex) * 0x24, point)) {
            sum.x += point.x;
            sum.y += point.y;
            sum.z += point.z;
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }

    position = {
        sum.x / static_cast<float>(count),
        sum.y / static_cast<float>(count),
        sum.z / static_cast<float>(count),
    };
    return true;
}

bool TryReadSplinePosition(const std::vector<char>& bytes,
                           std::size_t recordOffset,
                           std::size_t recordEnd,
                           Vec3& position) {
    if (recordOffset + 0x60 > recordEnd || recordOffset + 0x60 > bytes.size()) {
        return false;
    }

    const std::uint32_t pointCount = std::min<std::uint32_t>(ReadU32Le(bytes, recordOffset + 0x5C), 64);
    if (pointCount == 0) {
        return false;
    }

    Vec3 sum = {};
    int count = 0;
    for (std::uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        Vec3 point = {};
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, 0x68 + static_cast<std::size_t>(pointIndex) * 0x20, point)) {
            sum.x += point.x;
            sum.y += point.y;
            sum.z += point.z;
            ++count;
        }
    }
    if (count == 0) {
        return false;
    }

    position = {
        sum.x / static_cast<float>(count),
        sum.y / static_cast<float>(count),
        sum.z / static_cast<float>(count),
    };
    return true;
}

float ReadWrlFloatOrDefault(const std::vector<char>& bytes,
                            std::size_t recordOffset,
                            std::size_t recordEnd,
                            std::size_t relativeOffset,
                            float fallback) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return fallback;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 4) {
        return fallback;
    }

    const float value = ReadF32Le(bytes, recordOffset + relativeOffset);
    if (!std::isfinite(value) || std::abs(value) < 0.0001f || std::abs(value) > 1024.0f) {
        return fallback;
    }
    return value;
}

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f) {
    return std::abs(lhs - rhs) <= epsilon;
}

bool NearlyEqual(Vec3 lhs, Vec3 rhs, float epsilon = 0.0001f) {
    return NearlyEqual(lhs.x, rhs.x, epsilon) &&
           NearlyEqual(lhs.y, rhs.y, epsilon) &&
           NearlyEqual(lhs.z, rhs.z, epsilon);
}

bool NearlyEqual(Quat lhs, Quat rhs, float epsilon = 0.0001f) {
    return NearlyEqual(lhs.x, rhs.x, epsilon) &&
           NearlyEqual(lhs.y, rhs.y, epsilon) &&
           NearlyEqual(lhs.z, rhs.z, epsilon) &&
           NearlyEqual(lhs.w, rhs.w, epsilon);
}

void AddUniqueTerrainLink(std::vector<WrlTerrainLink>& links, WrlTerrainLink link) {
    link.path = NormalizeWorldAssetPath(std::move(link.path));
    if (!IsWorldAssetPath(link.path)) {
        return;
    }

    const std::string wantedPath = ToLower(link.path);
    const auto found = std::find_if(links.begin(), links.end(), [&](const WrlTerrainLink& existing) {
        return ToLower(existing.path) == wantedPath &&
               existing.hasLayer == link.hasLayer &&
               (!existing.hasLayer || existing.layer == link.layer) &&
               NearlyEqual(existing.position, link.position) &&
               NearlyEqual(existing.rotation, link.rotation) &&
               NearlyEqual(existing.scale, link.scale) &&
               NearlyEqual(existing.textureScaleX, link.textureScaleX) &&
               NearlyEqual(existing.textureScaleY, link.textureScaleY);
    });
    if (found == links.end()) {
        links.push_back(std::move(link));
    }
}

bool ClassUsesStandardWrlPosition(const std::string& classLower) {
    return classLower == "cracestartpos" ||
           classLower == "cfoyerstartpos" ||
           classLower == "cgeneralstatic" ||
           classLower == "cgoldenbrick" ||
           classLower == "cgeneralmobile" ||
           classLower == "cbonuspickup" ||
           classLower == "cbonusvortex" ||
           classLower == "cawarddisplay" ||
           classLower == "cfirework" ||
           classLower == "cmapmarker" ||
           classLower == "ccheckpoint" ||
           classLower == "cthrustermobile" ||
           classLower == "cskeleton" ||
           classLower == "clegostompermech" ||
           classLower == "cwheel" ||
           classLower == "cthepits" ||
           classLower == "cweaponpickup" ||
           classLower == "ccinematiccamera" ||
           classLower == "cpatch" ||
           classLower == "clegoterrain" ||
           classLower == "cfoyerhotspot" ||
           classLower == "csteampoint" ||
           classLower == "ckillbox" ||
           classLower == "ckillsphere" ||
           classLower == "cscripttrigger";
}

bool TryReadWorldObjectPosition(const std::vector<char>& bytes,
                                std::size_t recordOffset,
                                std::size_t recordEnd,
                                const std::string& className,
                                Vec3& position) {
    const std::string classLower = ToLower(className);
    if (classLower == "caisection") {
        return TryReadAiSectionPosition(bytes, recordOffset, recordEnd, position);
    }
    if (classLower == "cspline") {
        return TryReadSplinePosition(bytes, recordOffset, recordEnd, position);
    }

    std::vector<std::size_t> candidates;
    if (classLower == "cwatersheet" || classLower == "cwatercourse") {
        candidates.push_back(0x158);
    } else if (classLower == "cboulderspitter") {
        candidates.push_back(0x68);
    } else if (classLower == "cpointlight") {
        candidates.push_back(0x88);
    } else if (classLower == "cmodelandspotlight") {
        candidates.push_back(0x88);
        candidates.push_back(0x68);
    } else if (classLower == "caiquad") {
        candidates.push_back(0x88);
    } else if (classLower == "cminifig") {
        candidates.push_back(0x5C);
    } else if (classLower == "ccamera") {
        candidates.push_back(0x70);
    } else if (ClassUsesStandardWrlPosition(classLower)) {
        candidates.push_back(0x58);
    }

    for (std::size_t relativeOffset : candidates) {
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, relativeOffset, position, ClassUsesStandardWrlPosition(classLower))) {
            return true;
        }
    }
    return false;
}

bool CanUseFallbackModelPosition(const std::string& classLower) {
    return classLower != "cskybox" &&
           classLower != "clegoterrain" &&
           classLower != "cwatersheet" &&
           classLower != "cwatercourse";
}

bool TryReadFallbackModelPosition(const std::vector<char>& bytes,
                                  std::size_t recordOffset,
                                  std::size_t recordEnd,
                                  const std::string& classLower,
                                  Vec3& position) {
    if (!CanUseFallbackModelPosition(classLower)) {
        return false;
    }

    constexpr std::array<std::size_t, 12> kCandidates = {
        0x58, 0x5C, 0x68, 0x70, 0x78, 0x88, 0x90, 0x98, 0xA8, 0xB8, 0xC8, 0xD8,
    };
    for (std::size_t relativeOffset : kCandidates) {
        if (TryReadWorldVec3(bytes, recordOffset, recordEnd, relativeOffset, position)) {
            return true;
        }
    }
    return false;
}

bool IsWrlRecordAt(const std::vector<char>& bytes, std::size_t offset) {
    return offset + 4 <= bytes.size() &&
           bytes[offset + 0] == 'O' &&
           bytes[offset + 1] == 'B' &&
           bytes[offset + 2] == 'M' &&
           bytes[offset + 3] == 'G';
}

std::size_t FindNextWrlRecord(const std::vector<char>& bytes, std::size_t start) {
    for (std::size_t offset = start; offset + 4 <= bytes.size(); ++offset) {
        if (IsWrlRecordAt(bytes, offset)) {
            return offset;
        }
    }
    return static_cast<std::size_t>(-1);
}

struct WrlObjectRecord {
    std::size_t offset = 0;
    std::size_t end = 0;
    std::uint32_t payloadSize = 0;
    bool lengthTrusted = false;
};

std::vector<WrlObjectRecord> FindWrlObjectRecords(const std::vector<char>& bytes) {
    std::vector<WrlObjectRecord> records;
    std::size_t offset = 8;
    while (offset + 0x24 <= bytes.size()) {
        if (!IsWrlRecordAt(bytes, offset)) {
            offset = FindNextWrlRecord(bytes, offset + 1);
            if (offset == static_cast<std::size_t>(-1)) {
                break;
            }
        }

        const std::uint32_t payloadSize = ReadU32Le(bytes, offset + 0x20);
        std::size_t recordEnd = offset + 0x24 + static_cast<std::size_t>(payloadSize);
        bool lengthTrusted =
            payloadSize > 0 &&
            payloadSize < 0x20000 &&
            recordEnd <= bytes.size();

        if (!lengthTrusted) {
            const std::size_t next = FindNextWrlRecord(bytes, offset + 4);
            recordEnd = next == static_cast<std::size_t>(-1) ? bytes.size() : next;
        }
        if (recordEnd <= offset || recordEnd > bytes.size()) {
            break;
        }

        records.push_back({offset, recordEnd, payloadSize, lengthTrusted});
        offset = recordEnd;
    }
    return records;
}

std::string ReadWrlAssetPathAt(const std::vector<char>& bytes,
                               std::size_t recordOffset,
                               std::size_t recordEnd,
                               std::size_t relativeOffset,
                               std::size_t maxLength) {
    return NormalizeWorldAssetPath(ReadBoundedString(bytes, recordOffset + relativeOffset, recordEnd, maxLength));
}

bool TryReadWorldQuat(const std::vector<char>& bytes,
                      std::size_t recordOffset,
                      std::size_t recordEnd,
                      std::size_t relativeOffset,
                      Quat& rotation) {
    recordEnd = std::min(recordEnd, bytes.size());
    if (recordOffset >= recordEnd) {
        return false;
    }
    const std::size_t recordSize = recordEnd - recordOffset;
    if (relativeOffset > recordSize || recordSize - relativeOffset < 16) {
        return false;
    }

    const std::size_t offset = recordOffset + relativeOffset;
    rotation = Normalize({
        ReadF32Le(bytes, offset + 0),
        ReadF32Le(bytes, offset + 4),
        ReadF32Le(bytes, offset + 8),
        ReadF32Le(bytes, offset + 12),
    });
    return std::isfinite(rotation.x) &&
           std::isfinite(rotation.y) &&
           std::isfinite(rotation.z) &&
           std::isfinite(rotation.w);
}

std::size_t WrlModelPathOffsetForClass(const std::string& classLower) {
    if (classLower == "cgeneralstatic" ||
        classLower == "cgoldenbrick" ||
        classLower == "cbonusvortex" ||
        classLower == "cawarddisplay" ||
        classLower == "ccheckpoint" ||
        classLower == "cpatch" ||
        classLower == "cthepits" ||
        classLower == "cweaponpickup") {
        return 0x80;
    }
    if (classLower == "cgeneralmobile" ||
        classLower == "cbonuspickup" ||
        classLower == "cthrustermobile" ||
        classLower == "cskeleton" ||
        classLower == "clegostompermech" ||
        classLower == "cwheel") {
        return 0x90;
    }
    if (classLower == "cmodelandspotlight") {
        return 0xBC;
    }
    if (classLower == "cskybox") {
        return 0x58;
    }
    return static_cast<std::size_t>(-1);
}

LevelPreview DecodeWrlWorld(const std::vector<char>& bytes, std::string name, std::string path) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RC2W", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 WRL saved world.");
    }

    LevelPreview preview;
    preview.open = true;
    preview.world = true;
    preview.name = std::move(name);
    preview.path = std::move(path);

    const std::vector<WrlObjectRecord> records = FindWrlObjectRecords(bytes);
    preview.objects.reserve(records.size());
    int untrustedLengths = 0;
    for (const WrlObjectRecord& record : records) {
        if (record.end <= record.offset + 0x24) {
            continue;
        }
        if (!record.lengthTrusted) {
            ++untrustedLengths;
        }

        WorldObject object;
        object.className = ReadBoundedString(bytes, record.offset + 4, record.end, 24);
        object.recordSize = static_cast<std::uint32_t>(std::min<std::size_t>(
            record.end - record.offset,
            std::numeric_limits<std::uint32_t>::max()));
        if (record.offset + 0x28 <= record.end) {
            object.layer = ReadU32Le(bytes, record.offset + 0x24);
            object.hasLayer = true;
        }
        object.name = ReadBoundedString(bytes, record.offset + 0x28, record.end, 24);
        object.binding = ReadBoundedString(bytes, record.offset + 0x40, record.end, 24);
        object.assetPaths = ExtractWorldRecordPaths(bytes, record.offset, record.end);

        if (object.className.empty()) {
            object.className = "Unknown";
        }
        if (object.name.empty()) {
            object.name = object.className;
        }
        if (!object.assetPaths.empty()) {
            object.assetPath = object.assetPaths.front();
        }

        const std::string classLower = ToLower(object.className);
        object.hasPosition =
            TryReadWorldObjectPosition(bytes, record.offset, record.end, object.className, object.position);
        object.hasRotation = TryReadWorldQuat(
            bytes,
            record.offset,
            record.end,
            classLower == "cwatersheet" ? 0x164 : 0x64,
            object.rotation);
        const std::size_t modelPathOffset = WrlModelPathOffsetForClass(classLower);
        if (modelPathOffset != static_cast<std::size_t>(-1)) {
            object.modelPath = ReadWrlAssetPathAt(bytes, record.offset, record.end, modelPathOffset, 0x80);
            if (IsWorldAssetPath(object.modelPath)) {
                AddUniqueWorldAssetPath(object.assetPaths, object.modelPath);
                object.assetPath = object.modelPath;
            } else {
                object.modelPath.clear();
            }
        }
        if (object.modelPath.empty()) {
            object.modelPath = FirstWorldMd2Path(object.assetPaths);
            if (!object.modelPath.empty()) {
                object.assetPath = object.modelPath;
            }
        }
        if (!object.hasPosition && !object.modelPath.empty()) {
            object.hasPosition =
                TryReadFallbackModelPosition(bytes, record.offset, record.end, classLower, object.position);
            if (object.hasPosition) {
            }
        }
        if (classLower == "clegoterrain") {
            const std::string terrainDirectory = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0x80, 128);
            WrlTerrainLink terrainLink;
            terrainLink.layer = object.layer;
            terrainLink.hasLayer = object.hasLayer;
            terrainLink.position = object.hasPosition ? object.position : Vec3{};
            terrainLink.rotation = object.hasRotation ? object.rotation : Quat{};
            if (TryReadWorldVec3(bytes, record.offset, record.end, 0x104, terrainLink.scale)) {
                terrainLink.scale.x = std::abs(terrainLink.scale.x) < 0.0001f ? 1.0f : terrainLink.scale.x;
                terrainLink.scale.y = std::abs(terrainLink.scale.y) < 0.0001f ? 1.0f : terrainLink.scale.y;
                terrainLink.scale.z = std::abs(terrainLink.scale.z) < 0.0001f ? 1.0f : terrainLink.scale.z;
            }
            terrainLink.textureScaleX = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x120, 1.0f);
            terrainLink.textureScaleY = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x124, 1.0f);
            if (IsWorldAssetPath(terrainDirectory)) {
                terrainLink.path = terrainDirectory;
                AddUniqueWorldAssetPath(object.assetPaths, terrainDirectory);
                object.assetPath = terrainDirectory;
            } else if (!object.assetPath.empty()) {
                terrainLink.path = object.assetPath;
            }
            AddUniqueTerrainLink(preview.terrainLinks, std::move(terrainLink));
            if (preview.terrainPath.empty() && !preview.terrainLinks.empty()) {
                preview.terrainPath = preview.terrainLinks.front().path;
            }
        } else if (classLower == "cwatersheet") {
            LevelWaterSheet waterSheet;
            waterSheet.objectIndex = static_cast<int>(preview.objects.size());
            waterSheet.position = object.hasPosition ? object.position : Vec3{};
            waterSheet.rotation = object.hasRotation ? object.rotation : Quat{};
            waterSheet.texturePath = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0x58, 0x80);
            waterSheet.reflectionTexturePath = ReadWrlAssetPathAt(bytes, record.offset, record.end, 0xD8, 0x80);

            const float width = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x17C, waterSheet.width);
            const float depth = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x180, waterSheet.depth);
            const float uScale = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x184, waterSheet.uScale);
            const float vScale = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x188, waterSheet.vScale);
            const float alpha = ReadWrlFloatOrDefault(bytes, record.offset, record.end, 0x194, waterSheet.alpha);
            if (std::isfinite(width) && width > 0.001f && width < 10000.0f) {
                waterSheet.width = width;
            }
            if (std::isfinite(depth) && depth > 0.001f && depth < 10000.0f) {
                waterSheet.depth = depth;
            }
            if (std::isfinite(uScale) && uScale > 0.001f && uScale < 1024.0f) {
                waterSheet.uScale = uScale;
            }
            if (std::isfinite(vScale) && vScale > 0.001f && vScale < 1024.0f) {
                waterSheet.vScale = vScale;
            }
            if (std::isfinite(alpha) && alpha >= 0.15f && alpha <= 0.90f) {
                waterSheet.alpha = alpha;
            }

            if (IsWorldAssetPath(waterSheet.texturePath)) {
                AddUniqueWorldAssetPath(object.assetPaths, waterSheet.texturePath);
                object.assetPath = waterSheet.texturePath;
            } else {
                waterSheet.texturePath.clear();
            }
            if (IsWorldAssetPath(waterSheet.reflectionTexturePath)) {
                AddUniqueWorldAssetPath(object.assetPaths, waterSheet.reflectionTexturePath);
            } else {
                waterSheet.reflectionTexturePath.clear();
            }
            preview.waterSheets.push_back(std::move(waterSheet));
        }

        ++preview.classCounts[object.className];
        preview.objects.push_back(std::move(object));
    }

    RecomputeLevelBounds(preview);
    preview.status = std::to_string(preview.objects.size()) + " world objects";
    if (!preview.terrainLinks.empty()) {
        preview.status += ", " + std::to_string(preview.terrainLinks.size()) + " terrain link";
        if (preview.terrainLinks.size() != 1) {
            preview.status += "s";
        }
    }
    if (!preview.waterSheets.empty()) {
        preview.status += ", " + std::to_string(preview.waterSheets.size()) + " water sheet";
        if (preview.waterSheets.size() != 1) {
            preview.status += "s";
        }
    }
    if (untrustedLengths > 0) {
        preview.status += ", " + std::to_string(untrustedLengths) + " guessed records";
    }
    return preview;
}

void MergeTerrainIntoWorld(LevelPreview& world, LevelPreview terrain) {
    world.vertices = std::move(terrain.vertices);
    world.triangles = std::move(terrain.triangles);
    RecomputeLevelBounds(world);
}

void AddTerrainSectionToWorld(LevelPreview& world, LevelPreview terrain, const WrlTerrainLink& link) {
    LevelTerrainSection section;
    section.path = terrain.path;
    section.layer = link.layer;
    section.hasLayer = link.hasLayer;
    section.position = link.position;
    section.rotation = link.rotation;
    section.scale = link.scale;
    section.textureScaleX = link.textureScaleX;
    section.textureScaleY = link.textureScaleY;
    section.vertices = std::move(terrain.vertices);
    section.triangles = std::move(terrain.triangles);
    for (LevelVertex& vertex : section.vertices) {
        Vec3 scaledPosition = {
            vertex.position.x * section.scale.x,
            vertex.position.y * section.scale.y,
            vertex.position.z * section.scale.z,
        };
        vertex.position = Add(section.position, Rotate(section.rotation, scaledPosition));

        Vec3 scaledNormal = {
            vertex.normal.x / section.scale.x,
            vertex.normal.y / section.scale.y,
            vertex.normal.z / section.scale.z,
        };
        vertex.normal = Normalize(Rotate(section.rotation, scaledNormal));
        if (Length(vertex.normal) <= 0.0001f) {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
    }

    const std::uint32_t baseVertex = static_cast<std::uint32_t>(world.vertices.size());
    world.vertices.insert(world.vertices.end(), section.vertices.begin(), section.vertices.end());
    for (const LevelTriangle& triangle : section.triangles) {
        world.triangles.push_back({
            triangle.a + baseVertex,
            triangle.b + baseVertex,
            triangle.c + baseVertex,
            triangle.materialKey,
        });
    }
    world.terrainSections.push_back(std::move(section));
    RecomputeLevelBounds(world);
}

std::size_t ResolveTerrainTextureEntryIndex(const AppState& state, const std::string& terrainPath) {
    const std::string parentPath = ParentArchivePath(terrainPath);
    std::vector<std::string> candidates;
    candidates.push_back(JoinArchivePathString(parentPath, "TerrainMipped.TGA"));
    candidates.push_back(JoinArchivePathString(parentPath, "TERRAINMIPPED.TGA"));
    candidates.push_back(JoinArchivePathString(parentPath, "terrainmipped.tga"));
    candidates.push_back(JoinArchivePathString(parentPath, "TERRAINMIPPED.MIP"));
    candidates.push_back(JoinArchivePathString(parentPath, "TerrainMipped.MIP"));
    candidates.push_back(JoinArchivePathString(parentPath, "terrainmipped.mip"));

    for (std::string candidate : candidates) {
        std::replace(candidate.begin(), candidate.end(), '/', '\\');
        const std::size_t entryIndex = FindArchiveEntryIndex(state, candidate);
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::size_t ResolveTerrainLayerTextureEntryIndex(const AppState& state,
                                                 const std::string& terrainPath,
                                                 unsigned char textureIndex) {
    const std::string parentPath = ParentArchivePath(terrainPath);
    const int textureNumber = static_cast<int>(textureIndex) + 1;
    std::vector<std::string> candidates;
    candidates.push_back("TEXTURE" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("Texture" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".TGA");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".tga");
    candidates.push_back("TEXTURE" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("Texture" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".MIP");
    candidates.push_back("texture" + std::to_string(textureNumber) + ".mip");

    for (const std::string& candidateName : candidates) {
        const std::size_t entryIndex = FindArchiveEntryIndex(state, JoinArchivePathString(parentPath, candidateName));
        if (entryIndex != static_cast<std::size_t>(-1)) {
            return entryIndex;
        }
    }
    return static_cast<std::size_t>(-1);
}

std::vector<unsigned char> CollectTerrainTextureIndices(const LevelTerrainSection& section) {
    std::vector<unsigned char> indices;
    for (const LevelTriangle& triangle : section.triangles) {
        for (int layer = 0; layer < 4; ++layer) {
            const auto textureIndex = static_cast<unsigned char>((triangle.materialKey >> (layer * 8)) & 0xffU);
            if (textureIndex == 0xffU) {
                continue;
            }
            if (std::find(indices.begin(), indices.end(), textureIndex) == indices.end()) {
                indices.push_back(textureIndex);
            }
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void LoadTerrainPreviewTexture(AppState& state, LevelPreview& preview, const std::string& terrainPath) {
    const std::size_t textureEntryIndex = ResolveTerrainTextureEntryIndex(state, terrainPath);
    if (textureEntryIndex == static_cast<std::size_t>(-1)) {
        return;
    }

    try {
        const gtc::FileEntry& textureEntry = state.archive.entries[textureEntryIndex];
        preview.terrainTexture = DecodeArchiveTextureEntryForWhirledRender(state, textureEntryIndex);
        DeleteGlTexture(preview.terrainTextureId);
        preview.terrainTextureId = CreateGlTextureRgba(
            preview.terrainTexture.width,
            preview.terrainTexture.height,
            preview.terrainTexture.rgba.data(),
            true,
            true,
            true);
        preview.terrainTexturePath = textureEntry.path;
        preview.status +=
            ", texture " +
            std::to_string(preview.terrainTexture.width) + "x" +
            std::to_string(preview.terrainTexture.height);
    } catch (const std::exception& error) {
        preview.status += ", texture failed";
    }
}

void LoadTerrainSectionTexture(AppState& state, LevelTerrainSection& section) {
    int whirledTextureCount = 0;
    for (const unsigned char textureIndex : CollectTerrainTextureIndices(section)) {
        const std::size_t layerEntryIndex = ResolveTerrainLayerTextureEntryIndex(state, section.path, textureIndex);
        if (layerEntryIndex == static_cast<std::size_t>(-1)) {
            continue;
        }

        try {
            const gtc::FileEntry& textureEntry = state.archive.entries[layerEntryIndex];
            DecodedTexture texture = DecodeArchiveTextureEntryForWhirledRender(state, layerEntryIndex);
            DeleteGlTexture(section.layerTextureIds[textureIndex]);
            section.layerTextureIds[textureIndex] = CreateGlTextureRgba(
                texture.width,
                texture.height,
                texture.rgba.data(),
                true,
                true,
                true);
            if (section.layerTextureIds[textureIndex] != 0) {
                section.layerTexturePaths[textureIndex] = textureEntry.path;
                ++whirledTextureCount;
            }
        } catch (const std::exception& error) {
        }
    }

    if (whirledTextureCount > 0) {
        return;
    }

    const std::size_t textureEntryIndex = ResolveTerrainTextureEntryIndex(state, section.path);
    if (textureEntryIndex == static_cast<std::size_t>(-1)) {
        return;
    }

    try {
        const gtc::FileEntry& textureEntry = state.archive.entries[textureEntryIndex];
        section.texture = DecodeArchiveTextureEntryForWhirledRender(state, textureEntryIndex);
        DeleteGlTexture(section.textureId);
        section.textureId = CreateGlTextureRgba(
            section.texture.width,
            section.texture.height,
            section.texture.rgba.data(),
            true,
            true,
            true);
        section.texturePath = textureEntry.path;
    } catch (const std::exception& error) {
    }
}
