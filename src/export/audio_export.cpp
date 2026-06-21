void WriteBytes(std::ofstream& file, const char* text, std::size_t size) {
    file.write(text, static_cast<std::streamsize>(size));
}

void WriteU16Le(std::ofstream& file, std::uint16_t value) {
    const unsigned char bytes[2] = {
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8) & 0xffU),
    };
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void WriteU32Le(std::ofstream& file, std::uint32_t value) {
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8) & 0xffU),
        static_cast<unsigned char>((value >> 16) & 0xffU),
        static_cast<unsigned char>((value >> 24) & 0xffU),
    };
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

std::int16_t FloatToPcm16(float sample) {
    if (!std::isfinite(sample)) {
        sample = 0.0f;
    }
    sample = std::clamp(sample, -1.0f, 1.0f);
    if (sample >= 0.0f) {
        return static_cast<std::int16_t>(std::lround(sample * 32767.0f));
    }
    return static_cast<std::int16_t>(std::lround(sample * 32768.0f));
}

void WriteWavFile(const DecodedAudio& audio, const std::filesystem::path& requestedPath) {
    if (audio.channels <= 0 || audio.channels > 65535 ||
        audio.sampleRate <= 0 ||
        audio.frameCount == 0 ||
        audio.samples.empty()) {
        throw std::runtime_error("Audio has no samples to export.");
    }

    const std::uint64_t sampleCount = audio.frameCount * static_cast<std::uint64_t>(audio.channels);
    if (sampleCount > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max() / 2U)) {
        throw std::runtime_error("Audio is too large for WAV export.");
    }
    if (audio.samples.size() < sampleCount) {
        throw std::runtime_error("Decoded audio sample data is incomplete.");
    }

    const std::filesystem::path outputPath = EnsureExtension(requestedPath, ".wav");
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Could not create " + outputPath.string());
    }

    const auto channels = static_cast<std::uint16_t>(audio.channels);
    const auto sampleRate = static_cast<std::uint32_t>(audio.sampleRate);
    constexpr std::uint16_t bitsPerSample = 16;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * sizeof(std::int16_t));
    const std::uint32_t byteRate = sampleRate * blockAlign;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(sampleCount * sizeof(std::int16_t));
    const std::uint32_t riffBytes = 36U + dataBytes;

    WriteBytes(file, "RIFF", 4);
    WriteU32Le(file, riffBytes);
    WriteBytes(file, "WAVE", 4);
    WriteBytes(file, "fmt ", 4);
    WriteU32Le(file, 16);
    WriteU16Le(file, 1);
    WriteU16Le(file, channels);
    WriteU32Le(file, sampleRate);
    WriteU32Le(file, byteRate);
    WriteU16Le(file, blockAlign);
    WriteU16Le(file, bitsPerSample);
    WriteBytes(file, "data", 4);
    WriteU32Le(file, dataBytes);

    for (std::uint64_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        WriteU16Le(file, static_cast<std::uint16_t>(FloatToPcm16(audio.samples[static_cast<std::size_t>(sampleIndex)])));
    }

    if (!file) {
        throw std::runtime_error("Could not write " + outputPath.string());
    }
}

void ExportAudioNode(AppState& state, const ArchiveNode& node, const std::filesystem::path& requestedPath) {
    if (!IsAudioNode(node)) {
        throw std::runtime_error("Selected file is not audio.");
    }

    const std::vector<char> bytes =
        node.externalFile ? ReadBinaryPreviewFile(node.externalPath) : ReadEntryBytesForPreview(state, node.entryIndex);
    const DecodedAudio audio = DecodeAudioBytes(bytes);
    WriteWavFile(audio, requestedPath);
}
