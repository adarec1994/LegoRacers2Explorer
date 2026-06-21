std::string ProjectSourceUrl() {
    return std::string("https:") + std::string(2, '/') + "github.com/adarec1994/LegoRacers2Explorer";
}

std::string JrMasterModelBuilderUrl() {
    return std::string("https:") + std::string(2, '/') + "github.com/JrMasterModelBuilder";
}

std::filesystem::path AboutImagePath() {
    const std::filesystem::path sourcePath = std::filesystem::path(PROJECT_SOURCE_DIR_PATH) / "images" / "sparky.png";
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
    const std::filesystem::path localPath = std::filesystem::current_path() / "images" / "sparky.png";
    if (std::filesystem::exists(localPath)) {
        return localPath;
    }
    return sourcePath;
}

DecodedTexture DecodeWicImageFile(const std::filesystem::path& path) {
    ComInitScope com;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    try {
        ThrowIfFailed(
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)),
            "Could not create WIC imaging factory.");
        ThrowIfFailed(
            factory->CreateDecoderFromFilename(
                path.wstring().c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                &decoder),
            "Could not open about image.");
        ThrowIfFailed(decoder->GetFrame(0, &frame), "Could not read about image frame.");
        ThrowIfFailed(factory->CreateFormatConverter(&converter), "Could not create about image converter.");
        ThrowIfFailed(
            converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom),
            "Could not convert about image.");

        UINT width = 0;
        UINT height = 0;
        ThrowIfFailed(converter->GetSize(&width, &height), "Could not read about image size.");
        if (width == 0 || height == 0) {
            throw std::runtime_error("About image is empty.");
        }

        DecodedTexture texture;
        texture.width = static_cast<int>(width);
        texture.height = static_cast<int>(height);
        texture.bitsPerPixel = 32;
        texture.mipLevels = 1;
        texture.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        const UINT stride = width * 4U;
        ThrowIfFailed(
            converter->CopyPixels(
                nullptr,
                stride,
                static_cast<UINT>(texture.rgba.size()),
                texture.rgba.data()),
            "Could not copy about image pixels.");

        ReleaseCom(converter);
        ReleaseCom(frame);
        ReleaseCom(decoder);
        ReleaseCom(factory);
        return texture;
    } catch (...) {
        ReleaseCom(converter);
        ReleaseCom(frame);
        ReleaseCom(decoder);
        ReleaseCom(factory);
        throw;
    }
}

void EnsureAboutImage(AppState& state) {
    if (state.aboutImageLoadAttempted || state.aboutTextureId != 0) {
        return;
    }
    state.aboutImageLoadAttempted = true;
    try {
        DecodedTexture image = DecodeWicImageFile(AboutImagePath());
        state.aboutTextureId = CreateGlTextureRgba(
            image.width,
            image.height,
            image.rgba.data(),
            false,
            true,
            false);
        state.aboutImageWidth = image.width;
        state.aboutImageHeight = image.height;
    } catch (...) {
        state.aboutTextureId = 0;
        state.aboutImageWidth = 0;
        state.aboutImageHeight = 0;
    }
}

void OpenExternalUrl(const std::string& url) {
    if (url.empty()) {
        return;
    }
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void DrawAboutLink(const char* label, const std::string& url) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.68f, 1.0f, 1.0f));
    const bool clicked = ImGui::Selectable(label, false, ImGuiSelectableFlags_DontClosePopups);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Open in browser");
    }
    if (clicked) {
        OpenExternalUrl(url);
    }
}

ImVec2 AboutImageFitSize(int imageWidth, int imageHeight, ImVec2 maximumSize) {
    if (imageWidth <= 0 || imageHeight <= 0) {
        return {};
    }
    const float imageAspect = static_cast<float>(imageWidth) / static_cast<float>(imageHeight);
    const float boxAspect = maximumSize.x / maximumSize.y;
    if (imageAspect > boxAspect) {
        return {maximumSize.x, maximumSize.x / imageAspect};
    }
    return {maximumSize.y * imageAspect, maximumSize.y};
}

void DrawAboutRowText(const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value);
}

void DrawAboutWindow(AppState& state) {
    if (!state.aboutOpen) {
        return;
    }

    EnsureAboutImage(state);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(
            viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
            viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("About LEGO Racers 2 Explorer", &state.aboutOpen, flags)) {
        ImGui::End();
        return;
    }

    constexpr float imageMaxWidth = 240.0f;
    constexpr float imageMaxHeight = 360.0f;
    constexpr float rightWidth = 430.0f;

    ImGui::BeginGroup();
    if (state.aboutTextureId != 0) {
        const ImVec2 imageSize = AboutImageFitSize(
            state.aboutImageWidth,
            state.aboutImageHeight,
            ImVec2(imageMaxWidth, imageMaxHeight));
        const ImTextureID textureId = static_cast<ImTextureID>(static_cast<std::uintptr_t>(state.aboutTextureId));
        ImGui::Image(ImTextureRef(textureId), imageSize);
    } else {
        ImGui::Dummy(ImVec2(imageMaxWidth, imageMaxHeight));
        ImGui::TextDisabled("Image unavailable");
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + rightWidth);
    ImGui::TextUnformatted(
        "LEGO Racers 2 Explorer is a tool for inspecting the game's GTC archive, previewing textures, models, audio, FX, and worlds, and exporting assets for modding and research.");
    ImGui::PopTextWrapPos();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    if (ImGui::BeginTable(
            "##AboutEntries",
            2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody,
            ImVec2(rightWidth, 0.0f))) {
        ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 118.0f);
        ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);

        DrawAboutRowText("Version:", "In development");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Source Code:");
        ImGui::TableSetColumnIndex(1);
        DrawAboutLink("adarec1994/LegoRacers2Explorer", ProjectSourceUrl());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Special Thanks:");
        ImGui::TableSetColumnIndex(1);
        DrawAboutLink("JrMasterModelBuilder", JrMasterModelBuilderUrl());
        ImGui::TextUnformatted("GiantBlargg");

        ImGui::EndTable();
    }

    ImGui::EndGroup();
    ImGui::End();
}
