void RequireTextureRange(std::size_t offset,
                         std::size_t size,
                         std::size_t containerSize,
                         const char* message) {
    if (offset > containerSize || size > containerSize - offset) {
        throw std::runtime_error(message);
    }
}

void FlipTextureVertically(DecodedTexture& texture) {
    if (texture.width <= 0 || texture.height <= 1 || texture.rgba.empty()) {
        return;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(texture.width) * 4;
    std::vector<unsigned char> scratch(rowBytes);
    for (int y = 0; y < texture.height / 2; ++y) {
        const std::size_t top = static_cast<std::size_t>(y) * rowBytes;
        const std::size_t bottom = static_cast<std::size_t>(texture.height - 1 - y) * rowBytes;
        std::memcpy(scratch.data(), texture.rgba.data() + top, rowBytes);
        std::memcpy(texture.rgba.data() + top, texture.rgba.data() + bottom, rowBytes);
        std::memcpy(texture.rgba.data() + bottom, scratch.data(), rowBytes);
    }
}

DecodedTexture DecodeMipTexture(const std::vector<char>& bytes) {
    constexpr std::size_t kHeaderSize = 38;
    constexpr std::size_t kPaletteBytes = 256 * 4;
    if (bytes.size() < kHeaderSize || std::memcmp(bytes.data() + 18, "AIF1", 4) != 0) {
        throw std::runtime_error("Not a supported LR2 MIP texture.");
    }

    DecodedTexture texture;
    texture.width = ReadU16Le(bytes, 12);
    texture.height = ReadU16Le(bytes, 14);
    texture.bitsPerPixel = ReadU16Le(bytes, 16);
    texture.mipLevels = static_cast<int>(ReadU32Le(bytes, 22));

    if (texture.width <= 0 || texture.height <= 0 ||
        texture.width > 8192 || texture.height > 8192) {
        throw std::runtime_error("Texture dimensions are invalid.");
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    texture.rgba.resize(pixelCount * 4);

    if (texture.bitsPerPixel == 8) {
        RequireTextureRange(kHeaderSize, kPaletteBytes + pixelCount, bytes.size(),
                            "Paletted MIP texture is truncated.");
        const std::size_t paletteOffset = kHeaderSize;
        const std::size_t indexOffset = kHeaderSize + kPaletteBytes;

        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const auto paletteIndex = static_cast<unsigned char>(bytes[indexOffset + pixel]);
            const std::size_t paletteEntry = paletteOffset + static_cast<std::size_t>(paletteIndex) * 4;
            texture.rgba[pixel * 4 + 0] = static_cast<unsigned char>(bytes[paletteEntry + 2]);
            texture.rgba[pixel * 4 + 1] = static_cast<unsigned char>(bytes[paletteEntry + 1]);
            texture.rgba[pixel * 4 + 2] = static_cast<unsigned char>(bytes[paletteEntry + 0]);
            texture.rgba[pixel * 4 + 3] = static_cast<unsigned char>(bytes[paletteEntry + 3]);
        }
        FlipTextureVertically(texture);
        return texture;
    }

    if (texture.bitsPerPixel == 24 || texture.bitsPerPixel == 32) {
        const std::size_t sourceStride = static_cast<std::size_t>(texture.bitsPerPixel / 8);
        RequireTextureRange(kHeaderSize, pixelCount * sourceStride, bytes.size(),
                            "MIP texture pixel data is truncated.");

        for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
            const std::size_t source = kHeaderSize + pixel * sourceStride;
            texture.rgba[pixel * 4 + 0] = static_cast<unsigned char>(bytes[source + 2]);
            texture.rgba[pixel * 4 + 1] = static_cast<unsigned char>(bytes[source + 1]);
            texture.rgba[pixel * 4 + 2] = static_cast<unsigned char>(bytes[source + 0]);
            texture.rgba[pixel * 4 + 3] =
                texture.bitsPerPixel == 32 ? static_cast<unsigned char>(bytes[source + 3]) : 255;
        }
        FlipTextureVertically(texture);
        return texture;
    }

    throw std::runtime_error("Unsupported MIP bit depth: " + std::to_string(texture.bitsPerPixel));
}

DecodedTexture DecodeTgaTexture(const std::vector<char>& bytes) {
    constexpr std::size_t kTgaHeaderSize = 18;
    if (bytes.size() < kTgaHeaderSize) {
        throw std::runtime_error("TGA file is too small.");
    }

    const auto idLength = static_cast<unsigned char>(bytes[0]);
    const auto colorMapType = static_cast<unsigned char>(bytes[1]);
    const auto imageType = static_cast<unsigned char>(bytes[2]);
    if (colorMapType != 0 || imageType != 2) {
        throw std::runtime_error("Only uncompressed true-color TGA textures are supported.");
    }

    DecodedTexture texture;
    texture.width = ReadU16Le(bytes, 12);
    texture.height = ReadU16Le(bytes, 14);
    texture.bitsPerPixel = static_cast<unsigned char>(bytes[16]);
    texture.mipLevels = 1;

    if ((texture.bitsPerPixel != 24 && texture.bitsPerPixel != 32) ||
        texture.width <= 0 || texture.height <= 0) {
        throw std::runtime_error("Unsupported TGA texture format.");
    }

    const std::size_t sourceStride = static_cast<std::size_t>(texture.bitsPerPixel / 8);
    const std::size_t pixelCount =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height);
    const std::size_t dataOffset = kTgaHeaderSize + idLength;
    RequireTextureRange(dataOffset, pixelCount * sourceStride, bytes.size(),
                        "TGA texture pixel data is truncated.");

    const bool topOrigin = (static_cast<unsigned char>(bytes[17]) & 0x20U) != 0;
    texture.rgba.resize(pixelCount * 4);
    for (int y = 0; y < texture.height; ++y) {
        const int sourceY = topOrigin ? y : texture.height - 1 - y;
        for (int x = 0; x < texture.width; ++x) {
            const std::size_t source = dataOffset +
                (static_cast<std::size_t>(sourceY) * texture.width + x) * sourceStride;
            const std::size_t destination =
                (static_cast<std::size_t>(y) * texture.width + x) * 4;
            texture.rgba[destination + 0] = static_cast<unsigned char>(bytes[source + 2]);
            texture.rgba[destination + 1] = static_cast<unsigned char>(bytes[source + 1]);
            texture.rgba[destination + 2] = static_cast<unsigned char>(bytes[source + 0]);
            texture.rgba[destination + 3] =
                texture.bitsPerPixel == 32 ? static_cast<unsigned char>(bytes[source + 3]) : 255;
        }
    }

    return texture;
}
