void ThrowIfFailed(HRESULT result, const char* message) {
    if (FAILED(result)) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path EnsureExtension(std::filesystem::path path, const char* extension) {
    if (ToLower(path.extension().string()) != extension) {
        path.replace_extension(extension);
    }
    return path;
}

std::string ExportBaseName(const ArchiveNode& node) {
    std::string stem = std::filesystem::path(node.name).stem().string();
    if (stem.empty()) {
        stem = "export";
    }
    for (char& character : stem) {
        if (character == '/' || character == '\\' || character == ':' || character == '*' ||
            character == '?' || character == '"' || character == '<' || character == '>' || character == '|') {
            character = '_';
        }
    }
    return stem;
}

class ComInitScope {
public:
    ComInitScope() {
        result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initialized_ = result_ == S_OK || result_ == S_FALSE;
        if (FAILED(result_) && result_ != RPC_E_CHANGED_MODE) {
            ThrowIfFailed(result_, "Could not initialize COM for image export.");
        }
    }

    ~ComInitScope() {
        if (initialized_) {
            CoUninitialize();
        }
    }

private:
    HRESULT result_ = S_OK;
    bool initialized_ = false;
};

template <typename T>
void ReleaseCom(T*& value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

std::vector<unsigned char> EncodeWicImageToMemory(const DecodedTexture& texture, REFGUID containerFormat) {
    if (texture.width <= 0 || texture.height <= 0 || texture.rgba.empty()) {
        throw std::runtime_error("Texture has no pixels to export.");
    }

    ComInitScope com;
    IWICImagingFactory* factory = nullptr;
    IStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;

    try {
        ThrowIfFailed(
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)),
            "Could not create WIC imaging factory.");
        ThrowIfFailed(CreateStreamOnHGlobal(nullptr, TRUE, &stream), "Could not create image memory stream.");
        ThrowIfFailed(factory->CreateEncoder(containerFormat, nullptr, &encoder), "Could not create image encoder.");
        ThrowIfFailed(encoder->Initialize(stream, WICBitmapEncoderNoCache), "Could not initialize image encoder.");
        ThrowIfFailed(encoder->CreateNewFrame(&frame, nullptr), "Could not create image frame.");
        ThrowIfFailed(frame->Initialize(nullptr), "Could not initialize image frame.");
        ThrowIfFailed(frame->SetSize(texture.width, texture.height), "Could not set image size.");

        std::vector<unsigned char> bgra(texture.rgba.size());
        for (std::size_t pixel = 0; pixel + 3 < texture.rgba.size(); pixel += 4) {
            bgra[pixel + 0] = texture.rgba[pixel + 2];
            bgra[pixel + 1] = texture.rgba[pixel + 1];
            bgra[pixel + 2] = texture.rgba[pixel + 0];
            bgra[pixel + 3] = texture.rgba[pixel + 3];
        }

        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
        ThrowIfFailed(frame->SetPixelFormat(&pixelFormat), "Could not set image pixel format.");
        if (!IsEqualGUID(pixelFormat, GUID_WICPixelFormat32bppBGRA)) {
            throw std::runtime_error("WIC encoder does not support BGRA export for this image.");
        }

        const UINT stride = static_cast<UINT>(texture.width) * 4U;
        const UINT byteCount = stride * static_cast<UINT>(texture.height);
        ThrowIfFailed(
            frame->WritePixels(
                static_cast<UINT>(texture.height),
                stride,
                byteCount,
                bgra.data()),
            "Could not write image pixels.");
        ThrowIfFailed(frame->Commit(), "Could not commit image frame.");
        ThrowIfFailed(encoder->Commit(), "Could not commit image encoder.");

        HGLOBAL memory = nullptr;
        ThrowIfFailed(GetHGlobalFromStream(stream, &memory), "Could not access encoded image memory.");
        const SIZE_T size = GlobalSize(memory);
        void* locked = GlobalLock(memory);
        if (locked == nullptr || size == 0) {
            throw std::runtime_error("Encoded image memory is empty.");
        }
        std::vector<unsigned char> encoded(
            static_cast<unsigned char*>(locked),
            static_cast<unsigned char*>(locked) + size);
        GlobalUnlock(memory);

        ReleaseCom(frame);
        ReleaseCom(encoder);
        ReleaseCom(stream);
        ReleaseCom(factory);
        return encoded;
    } catch (...) {
        ReleaseCom(frame);
        ReleaseCom(encoder);
        ReleaseCom(stream);
        ReleaseCom(factory);
        throw;
    }
}

void WriteWicImageFile(const DecodedTexture& texture, const std::filesystem::path& path, REFGUID containerFormat) {
    const std::vector<unsigned char> encoded = EncodeWicImageToMemory(texture, containerFormat);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Could not open export file: " + path.string());
    }
    file.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    if (!file) {
        throw std::runtime_error("Could not write export file: " + path.string());
    }
}

template <typename T>
void WritePod(std::ofstream& file, const T& value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void WriteDdsImageFile(const DecodedTexture& texture, const std::filesystem::path& path) {
    if (texture.width <= 0 || texture.height <= 0 || texture.rgba.empty()) {
        throw std::runtime_error("Texture has no pixels to export.");
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Could not open export file: " + path.string());
    }

    constexpr std::uint32_t ddsMagic = 0x20534444U;
    constexpr std::uint32_t ddsdCaps = 0x1U;
    constexpr std::uint32_t ddsdHeight = 0x2U;
    constexpr std::uint32_t ddsdWidth = 0x4U;
    constexpr std::uint32_t ddsdPitch = 0x8U;
    constexpr std::uint32_t ddsdPixelFormat = 0x1000U;
    constexpr std::uint32_t ddpfAlphaPixels = 0x1U;
    constexpr std::uint32_t ddpfRgb = 0x40U;
    constexpr std::uint32_t ddsCapsTexture = 0x1000U;

    WritePod(file, ddsMagic);
    WritePod(file, std::uint32_t{124});
    WritePod(file, ddsdCaps | ddsdHeight | ddsdWidth | ddsdPitch | ddsdPixelFormat);
    WritePod(file, static_cast<std::uint32_t>(texture.height));
    WritePod(file, static_cast<std::uint32_t>(texture.width));
    WritePod(file, static_cast<std::uint32_t>(texture.width * 4));
    WritePod(file, std::uint32_t{0});
    WritePod(file, std::uint32_t{0});
    for (int index = 0; index < 11; ++index) {
        WritePod(file, std::uint32_t{0});
    }
    WritePod(file, std::uint32_t{32});
    WritePod(file, ddpfAlphaPixels | ddpfRgb);
    WritePod(file, std::uint32_t{0});
    WritePod(file, std::uint32_t{32});
    WritePod(file, std::uint32_t{0x000000ff});
    WritePod(file, std::uint32_t{0x0000ff00});
    WritePod(file, std::uint32_t{0x00ff0000});
    WritePod(file, std::uint32_t{0xff000000});
    WritePod(file, ddsCapsTexture);
    WritePod(file, std::uint32_t{0});
    WritePod(file, std::uint32_t{0});
    WritePod(file, std::uint32_t{0});
    WritePod(file, std::uint32_t{0});
    file.write(reinterpret_cast<const char*>(texture.rgba.data()), static_cast<std::streamsize>(texture.rgba.size()));
    if (!file) {
        throw std::runtime_error("Could not write DDS export: " + path.string());
    }
}

bool IsImageExportKind(ExportKind kind) {
    return kind == ExportKind::TexturePng ||
           kind == ExportKind::TextureTiff ||
           kind == ExportKind::TextureDds ||
           kind == ExportKind::HeightmapPng ||
           kind == ExportKind::HeightmapTiff ||
           kind == ExportKind::HeightmapDds;
}

ExportKind NormalizeImageExportKind(ExportKind kind) {
    switch (kind) {
    case ExportKind::HeightmapPng:
        return ExportKind::TexturePng;
    case ExportKind::HeightmapTiff:
        return ExportKind::TextureTiff;
    case ExportKind::HeightmapDds:
        return ExportKind::TextureDds;
    default:
        return kind;
    }
}

void ExportDecodedTextureFile(const DecodedTexture& texture,
                              const std::filesystem::path& requestedPath,
                              ExportKind kind) {
    switch (NormalizeImageExportKind(kind)) {
    case ExportKind::TexturePng:
        WriteWicImageFile(texture, EnsureExtension(requestedPath, ".png"), GUID_ContainerFormatPng);
        break;
    case ExportKind::TextureTiff:
        WriteWicImageFile(texture, EnsureExtension(requestedPath, ".tiff"), GUID_ContainerFormatTiff);
        break;
    case ExportKind::TextureDds:
        WriteDdsImageFile(texture, EnsureExtension(requestedPath, ".dds"));
        break;
    default:
        throw std::runtime_error("Unsupported image export format.");
    }
}

void ExportTextureNode(AppState& state, const ArchiveNode& node, const std::filesystem::path& requestedPath, ExportKind kind) {
    if (!IsTextureNode(node)) {
        throw std::runtime_error("Selected file is not a texture.");
    }
    const std::vector<char> bytes = ReadEntryBytesForPreview(state, node.entryIndex);
    DecodedTexture texture;
    if (NodeExtensionLower(node) == ".ifl") {
        LoadedIflFrames frames = LoadIflFrames(state, node, bytes);
        if (frames.frames.empty()) {
            throw std::runtime_error("IFL did not contain an exportable frame.");
        }
        texture = std::move(frames.frames.front());
    } else {
        texture = DecodeTextureBytesByName(bytes, node.path);
    }

    ExportDecodedTextureFile(texture, requestedPath, kind);
}
