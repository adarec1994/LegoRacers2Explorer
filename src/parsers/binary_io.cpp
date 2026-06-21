std::uint16_t ReadU16Le(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Texture read exceeded the file size.");
    }

    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(bytes[offset]) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 8));
}

std::int16_t ReadS16Le(const std::vector<char>& bytes, std::size_t offset) {
    return static_cast<std::int16_t>(ReadU16Le(bytes, offset));
}

std::uint32_t ReadU32Le(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Texture read exceeded the file size.");
    }

    return static_cast<std::uint32_t>(
        static_cast<unsigned char>(bytes[offset]) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 8) |
        (static_cast<unsigned char>(bytes[offset + 2]) << 16) |
        (static_cast<unsigned char>(bytes[offset + 3]) << 24));
}

float ReadF32Le(const std::vector<char>& bytes, std::size_t offset) {
    const std::uint32_t packed = ReadU32Le(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &packed, sizeof(value));
    return value;
}

std::uint16_t ReadU16Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Audio read exceeded the file size.");
    }

    return static_cast<std::uint16_t>(
        (static_cast<unsigned char>(bytes[offset]) << 8) |
        static_cast<unsigned char>(bytes[offset + 1]));
}

std::uint32_t ReadU32Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Audio read exceeded the file size.");
    }

    return static_cast<std::uint32_t>(
        (static_cast<unsigned char>(bytes[offset]) << 24) |
        (static_cast<unsigned char>(bytes[offset + 1]) << 16) |
        (static_cast<unsigned char>(bytes[offset + 2]) << 8) |
        static_cast<unsigned char>(bytes[offset + 3]));
}
