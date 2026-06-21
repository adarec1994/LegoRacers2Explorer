double ReadExtended80Be(const std::vector<char>& bytes, std::size_t offset) {
    if (offset + 10 > bytes.size()) {
        throw std::runtime_error("AIFF sample rate is truncated.");
    }

    const std::uint16_t exponentBits = ReadU16Be(bytes, offset);
    const bool negative = (exponentBits & 0x8000U) != 0;
    const int exponent = static_cast<int>(exponentBits & 0x7fffU);
    const std::uint64_t high = ReadU32Be(bytes, offset + 2);
    const std::uint64_t low = ReadU32Be(bytes, offset + 6);
    const std::uint64_t mantissa = (high << 32) | low;
    if (exponent == 0 && mantissa == 0) {
        return 0.0;
    }

    const long double value = std::ldexp(static_cast<long double>(mantissa), exponent - 16383 - 63);
    return static_cast<double>(negative ? -value : value);
}

float DecodeSignedPcmSampleBe(const std::vector<char>& bytes, std::size_t offset, int bitsPerSample) {
    switch (bitsPerSample) {
    case 8: {
        const auto value = static_cast<std::int8_t>(bytes[offset]);
        return static_cast<float>(value) / 128.0f;
    }
    case 16: {
        const auto value = static_cast<std::int16_t>(ReadU16Be(bytes, offset));
        return static_cast<float>(value) / 32768.0f;
    }
    case 24: {
        std::int32_t value =
            (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 0])) << 16) |
            (static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
            static_cast<std::int32_t>(static_cast<unsigned char>(bytes[offset + 2]));
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xff000000);
        }
        return static_cast<float>(value) / 8388608.0f;
    }
    case 32: {
        const auto value = static_cast<std::int32_t>(ReadU32Be(bytes, offset));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    default:
        throw std::runtime_error("Unsupported AIFF bit depth: " + std::to_string(bitsPerSample));
    }
}

DecodedAudio DecodeAiffAudio(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "FORM", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "AIFF", 4) != 0) {
        throw std::runtime_error("Not a supported AIFF audio file.");
    }

    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::uint64_t declaredFrames = 0;
    std::size_t soundOffset = static_cast<std::size_t>(-1);
    std::size_t soundBytes = 0;

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const char* chunk = bytes.data() + offset;
        const std::uint32_t chunkSize = ReadU32Be(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            throw std::runtime_error("AIFF chunk is truncated.");
        }

        if (std::memcmp(chunk, "COMM", 4) == 0) {
            if (chunkSize < 18) {
                throw std::runtime_error("AIFF COMM chunk is truncated.");
            }
            channels = ReadU16Be(bytes, dataOffset);
            declaredFrames = ReadU32Be(bytes, dataOffset + 2);
            bitsPerSample = ReadU16Be(bytes, dataOffset + 6);
            sampleRate = static_cast<int>(std::lround(ReadExtended80Be(bytes, dataOffset + 8)));
        } else if (std::memcmp(chunk, "SSND", 4) == 0) {
            if (chunkSize < 8) {
                throw std::runtime_error("AIFF SSND chunk is truncated.");
            }
            const std::uint32_t dataSkip = ReadU32Be(bytes, dataOffset);
            const std::size_t pcmOffset = dataOffset + 8 + dataSkip;
            const std::size_t pcmStartLimit = dataOffset + chunkSize;
            if (pcmOffset > pcmStartLimit) {
                throw std::runtime_error("AIFF SSND data offset is invalid.");
            }
            soundOffset = pcmOffset;
            soundBytes = pcmStartLimit - pcmOffset;
        }

        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }

    if (channels <= 0 || channels > 8 || sampleRate <= 0 || bitsPerSample <= 0) {
        throw std::runtime_error("AIFF COMM metadata is invalid.");
    }
    if (soundOffset == static_cast<std::size_t>(-1) || soundBytes == 0) {
        throw std::runtime_error("AIFF SSND audio data was not found.");
    }
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        throw std::runtime_error("Unsupported AIFF bit depth: " + std::to_string(bitsPerSample));
    }

    const int bytesPerSample = bitsPerSample / 8;
    const std::size_t bytesPerFrame = static_cast<std::size_t>(channels) * bytesPerSample;
    std::uint64_t frameCount = soundBytes / bytesPerFrame;
    if (declaredFrames > 0) {
        frameCount = std::min<std::uint64_t>(frameCount, declaredFrames);
    }
    if (frameCount == 0 || frameCount > 10ULL * 60ULL * 60ULL * static_cast<std::uint64_t>(sampleRate)) {
        throw std::runtime_error("AIFF audio length is invalid.");
    }

    DecodedAudio audio;
    audio.channels = channels;
    audio.sampleRate = sampleRate;
    audio.bitsPerSample = bitsPerSample;
    audio.frameCount = frameCount;
    audio.format = "AIFF " + std::to_string(bitsPerSample) + "-bit";
    audio.samples.resize(static_cast<std::size_t>(frameCount) * channels);

    for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t sampleOffset =
                soundOffset +
                (static_cast<std::size_t>(frame) * channels + static_cast<std::size_t>(channel)) * bytesPerSample;
            audio.samples[static_cast<std::size_t>(frame) * channels + static_cast<std::size_t>(channel)] =
                DecodeSignedPcmSampleBe(bytes, sampleOffset, bitsPerSample);
        }
    }

    return audio;
}

int ReadWaveBitsPerSample(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return 0;
    }

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkSize = ReadU32Le(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            return 0;
        }
        if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
            return static_cast<int>(ReadU16Le(bytes, dataOffset + 14));
        }
        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }
    return 0;
}

std::string DescribeWaveAudio(const std::vector<char>& bytes) {
    if (bytes.size() < 12 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return "Decoded audio";
    }

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkSize = ReadU32Le(bytes, offset + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > bytes.size() || chunkSize > bytes.size() - dataOffset) {
            return "WAV";
        }
        if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
            const std::uint16_t formatTag = ReadU16Le(bytes, dataOffset);
            const std::uint16_t bitsPerSample = ReadU16Le(bytes, dataOffset + 14);
            const char* label = "WAV";
            switch (formatTag) {
            case 1:
                label = "WAV PCM";
                break;
            case 2:
                label = "WAV MS ADPCM";
                break;
            case 3:
                label = "WAV float";
                break;
            case 17:
                label = "WAV IMA ADPCM";
                break;
            default:
                break;
            }
            return std::string(label) + (bitsPerSample > 0 ? " " + std::to_string(bitsPerSample) + "-bit" : "");
        }
        offset = dataOffset + chunkSize + (chunkSize & 1U);
    }
    return "WAV";
}

DecodedAudio DecodeMiniaudioAudio(const std::vector<char>& bytes) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder = {};
    const ma_result initResult = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder);
    if (initResult != MA_SUCCESS) {
        throw std::runtime_error("Audio format is not supported by the decoder.");
    }

    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    if (ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0) != MA_SUCCESS ||
        format != ma_format_f32 ||
        channels == 0 ||
        sampleRate == 0) {
        ma_decoder_uninit(&decoder);
        throw std::runtime_error("Decoded audio metadata is invalid.");
    }

    ma_uint64 frameCount = 0;
    std::vector<float> samples;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount) == MA_SUCCESS && frameCount > 0) {
        ma_decoder_seek_to_pcm_frame(&decoder, 0);
        samples.resize(static_cast<std::size_t>(frameCount) * channels);
        ma_uint64 framesRead = 0;
        const ma_result readResult =
            ma_decoder_read_pcm_frames(&decoder, samples.data(), frameCount, &framesRead);
        if (readResult != MA_SUCCESS) {
            ma_decoder_uninit(&decoder);
            throw std::runtime_error("Could not decode audio samples.");
        }
        frameCount = framesRead;
        samples.resize(static_cast<std::size_t>(frameCount) * channels);
    } else {
        static constexpr ma_uint64 kChunkFrames = 4096;
        std::vector<float> chunk(static_cast<std::size_t>(kChunkFrames) * channels);
        for (;;) {
            ma_uint64 framesRead = 0;
            const ma_result readResult =
                ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead);
            if (readResult != MA_SUCCESS || framesRead == 0) {
                break;
            }
            samples.insert(
                samples.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(framesRead * channels));
            frameCount += framesRead;
        }
    }

    ma_decoder_uninit(&decoder);
    if (frameCount == 0 || samples.empty()) {
        throw std::runtime_error("Decoded audio contains no samples.");
    }

    DecodedAudio audio;
    audio.channels = static_cast<int>(channels);
    audio.sampleRate = static_cast<int>(sampleRate);
    audio.bitsPerSample = ReadWaveBitsPerSample(bytes);
    audio.frameCount = frameCount;
    audio.format = DescribeWaveAudio(bytes);
    audio.samples = std::move(samples);
    return audio;
}

DecodedAudio DecodeAudioBytes(const std::vector<char>& bytes) {
    if (bytes.size() >= 12 &&
        std::memcmp(bytes.data(), "FORM", 4) == 0 &&
        std::memcmp(bytes.data() + 8, "AIFF", 4) == 0) {
        return DecodeAiffAudio(bytes);
    }
    return DecodeMiniaudioAudio(bytes);
}
