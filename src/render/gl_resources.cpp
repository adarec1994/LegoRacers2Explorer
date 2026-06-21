void DeleteGlTexture(GLuint& textureId) {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
}

void DeleteGlDisplayList(GLuint& displayList) {
    if (displayList != 0) {
        glDeleteLists(displayList, 1);
        displayList = 0;
    }
}

RetiredGlTexture TakePreviewTexture(TexturePreview& preview) {
    RetiredGlTexture texture;
    texture.textureId = preview.textureId;
    texture.framesUntilDestroy = 3;
    preview.textureId = 0;
    return texture;
}

RetiredGlTexture TakeFxPreviewTexture(FxPreview& preview) {
    RetiredGlTexture texture;
    texture.textureId = preview.textureId;
    texture.framesUntilDestroy = 3;
    preview.textureId = 0;
    return texture;
}

bool HasRetiredTexture(const RetiredGlTexture& texture) {
    return texture.textureId != 0;
}

void DestroyRetiredTextureNow(RetiredGlTexture& texture) {
    DeleteGlTexture(texture.textureId);
}

void DestroyPreviewTextureImmediate(TexturePreview& preview) {
    RetiredGlTexture texture = TakePreviewTexture(preview);
    DestroyRetiredTextureNow(texture);
}

void DestroyFxPreviewTextureImmediate(FxPreview& preview) {
    RetiredGlTexture texture = TakeFxPreviewTexture(preview);
    DestroyRetiredTextureNow(texture);
}

void RetirePreviewTexture(RetiredGlTexture texture) {
    if (!HasRetiredTexture(texture)) {
        return;
    }
    gRetiredPreviewTextures.push_back(texture);
}

void DestroyPreviewTexture(TexturePreview& preview) {
    RetirePreviewTexture(TakePreviewTexture(preview));
}

void DestroyFxPreviewTexture(FxPreview& preview) {
    RetirePreviewTexture(TakeFxPreviewTexture(preview));
}

void DestroyModelTextures(ModelPreview& preview) {
    for (ModelMaterial& material : preview.materials) {
        DeleteGlTexture(material.textureId);
        material.loaded = false;
    }
}

void DestroyModelRenderTextureImmediate(ModelPreview& preview) {
    DestroyModelTextures(preview);
}

void DestroyModelRenderTexture(ModelPreview& preview) {
    DestroyModelRenderTextureImmediate(preview);
}

void ProcessRetiredPreviewTextures() {
    for (auto& texture : gRetiredPreviewTextures) {
        --texture.framesUntilDestroy;
    }

    auto writeIt = gRetiredPreviewTextures.begin();
    for (auto readIt = gRetiredPreviewTextures.begin(); readIt != gRetiredPreviewTextures.end(); ++readIt) {
        if (readIt->framesUntilDestroy <= 0) {
            DestroyRetiredTextureNow(*readIt);
        } else {
            *writeIt++ = *readIt;
        }
    }
    gRetiredPreviewTextures.erase(writeIt, gRetiredPreviewTextures.end());
}

void DestroyAllPreviewTextures(TexturePreview& preview) {
    DestroyPreviewTextureImmediate(preview);
    for (auto& texture : gRetiredPreviewTextures) {
        DestroyRetiredTextureNow(texture);
    }
    gRetiredPreviewTextures.clear();
}

void StopFxPreview(FxPreview& preview) {
    DestroyFxPreviewTexture(preview);
    preview.open = false;
    preview.playing = false;
    preview.loop = true;
    preview.frameIndex = 0;
    preview.lastFrameTime = 0.0;
    preview.fps = 12.5f;
    preview.name.clear();
    preview.path.clear();
    preview.status.clear();
    preview.decoded = {};
    preview.frames.clear();
    preview.frameNames.clear();
    preview.frameTicks.clear();
}

void StopLevelPreview(LevelPreview& preview) {
    DeleteGlTexture(preview.terrainTextureId);
    for (LevelTerrainSection& section : preview.terrainSections) {
        DeleteGlTexture(section.textureId);
        for (GLuint& textureId : section.layerTextureIds) {
            DeleteGlTexture(textureId);
        }
        DeleteGlDisplayList(section.displayListTextured);
        DeleteGlDisplayList(section.displayListColored);
    }
    for (LevelWaterSheet& waterSheet : preview.waterSheets) {
        DeleteGlTexture(waterSheet.textureId);
        DeleteGlDisplayList(waterSheet.displayList);
    }
    for (LevelModelAsset& asset : preview.modelAssets) {
        DeleteGlDisplayList(asset.displayList);
        DeleteGlDisplayList(asset.skyboxDisplayList);
        DestroyModelRenderTexture(asset.model);
    }
    preview = {};
}

std::vector<unsigned char> BuildNextRgbaMipLevel(const std::vector<unsigned char>& previous, int width, int height) {
    const int nextWidth = std::max(1, width / 2);
    const int nextHeight = std::max(1, height / 2);
    std::vector<unsigned char> next(static_cast<std::size_t>(nextWidth) * static_cast<std::size_t>(nextHeight) * 4);

    for (int y = 0; y < nextHeight; ++y) {
        for (int x = 0; x < nextWidth; ++x) {
            std::array<unsigned int, 4> sum = {};
            int samples = 0;
            for (int dy = 0; dy < 2; ++dy) {
                const int sourceY = std::min(height - 1, y * 2 + dy);
                for (int dx = 0; dx < 2; ++dx) {
                    const int sourceX = std::min(width - 1, x * 2 + dx);
                    const std::size_t source =
                        (static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(sourceX)) * 4;
                    for (int channel = 0; channel < 4; ++channel) {
                        sum[static_cast<std::size_t>(channel)] += previous[source + channel];
                    }
                    ++samples;
                }
            }

            const std::size_t destination =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(nextWidth) + static_cast<std::size_t>(x)) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                next[destination + channel] =
                    static_cast<unsigned char>((sum[static_cast<std::size_t>(channel)] + samples / 2) / samples);
            }
        }
    }

    return next;
}

bool HasOpenGlExtension(const char* name) {
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions == nullptr || name == nullptr || name[0] == '\0') {
        return false;
    }

    const std::string allExtensions(extensions);
    std::size_t offset = 0;
    while ((offset = allExtensions.find(name, offset)) != std::string::npos) {
        const bool startOk = offset == 0 || allExtensions[offset - 1] == ' ';
        const std::size_t end = offset + std::strlen(name);
        const bool endOk = end == allExtensions.size() || allExtensions[end] == ' ';
        if (startOk && endOk) {
            return true;
        }
        offset = end;
    }
    return false;
}

float MaxSupportedAnisotropy() {
    static bool checked = false;
    static float maxAnisotropy = 1.0f;
    if (!checked) {
        checked = true;
        if (HasOpenGlExtension("GL_EXT_texture_filter_anisotropic")) {
            glGetFloatv(kGlMaxTextureMaxAnisotropyExt, &maxAnisotropy);
            if (!std::isfinite(maxAnisotropy) || maxAnisotropy < 1.0f) {
                maxAnisotropy = 1.0f;
            }
        }
    }
    return maxAnisotropy;
}

GLuint CreateGlTextureRgba(int width,
                           int height,
                           const unsigned char* pixels,
                           bool repeat,
                           bool linear,
                           bool mipmaps) {
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        return 0;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const bool buildMipmaps = mipmaps && repeat && linear && (width > 1 || height > 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, buildMipmaps ? GL_LINEAR_MIPMAP_LINEAR : (linear ? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    if (buildMipmaps) {
        std::vector<unsigned char> previous(
            pixels,
            pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        int mipWidth = width;
        int mipHeight = height;
        int level = 1;
        while (mipWidth > 1 || mipHeight > 1) {
            std::vector<unsigned char> next = BuildNextRgbaMipLevel(previous, mipWidth, mipHeight);
            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);
            glTexImage2D(
                GL_TEXTURE_2D,
                level,
                GL_RGBA,
                mipWidth,
                mipHeight,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                next.data());
            previous = std::move(next);
            ++level;
        }

        const float maxAnisotropy = MaxSupportedAnisotropy();
        if (maxAnisotropy > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, kGlTextureMaxAnisotropyExt, std::min(8.0f, maxAnisotropy));
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureId;
}

GLuint BlackTextureId() {
    if (gBlackTextureId != 0) {
        return gBlackTextureId;
    }

    const unsigned char blackPixel[4] = {0, 0, 0, 255};
    gBlackTextureId = CreateGlTextureRgba(1, 1, blackPixel, true, true, false);
    return gBlackTextureId;
}

bool HasTerrainShaderApi() {
    return gGlUseProgram != nullptr &&
           gGlCreateShader != nullptr &&
           gGlShaderSource != nullptr &&
           gGlCompileShader != nullptr &&
           gGlGetShaderiv != nullptr &&
           gGlGetShaderInfoLog != nullptr &&
           gGlDeleteShader != nullptr &&
           gGlCreateProgram != nullptr &&
           gGlAttachShader != nullptr &&
           gGlLinkProgram != nullptr &&
           gGlGetProgramiv != nullptr &&
           gGlGetProgramInfoLog != nullptr &&
           gGlDeleteProgram != nullptr &&
           gGlGetUniformLocation != nullptr &&
           gGlUniform1i != nullptr &&
           gGlUniform3f != nullptr &&
           gGlUniform1f != nullptr &&
           gGlActiveTexture != nullptr;
}

std::string GetShaderInfoLog(GLuint shader) {
    GLint length = 0;
    gGlGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    gGlGetShaderInfoLog(shader, length, &written, log.data());
    if (written > 0 && written <= length) {
        log.resize(static_cast<std::size_t>(written));
    }
    return log;
}

std::string GetProgramInfoLog(GLuint program) {
    GLint length = 0;
    gGlGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    gGlGetProgramInfoLog(program, length, &written, log.data());
    if (written > 0 && written <= length) {
        log.resize(static_cast<std::size_t>(written));
    }
    return log;
}

GLuint CompileTerrainShader(GLenum shaderType, const char* source, const char* label) {
    const GLuint shader = gGlCreateShader(shaderType);
    if (shader == 0) {
        return 0;
    }
    const char* sources[] = {source};
    gGlShaderSource(shader, 1, sources, nullptr);
    gGlCompileShader(shader);

    GLint compiled = 0;
    gGlGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        gGlDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint EnsureTerrainShaderProgram() {
    if (gTerrainShaderProgram != 0) {
        return gTerrainShaderProgram;
    }
    if (gTerrainShaderAttempted || !HasTerrainShaderApi()) {
        return 0;
    }
    gTerrainShaderAttempted = true;

    static constexpr const char* kTerrainVertexShader = R"GLSL(
#version 120
varying vec2 vUv;
varying vec4 vMix;
varying vec3 vNormal;

void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    vUv = gl_MultiTexCoord0.xy;
    vMix = gl_Color;
    vNormal = normalize(gl_Normal);
}
)GLSL";

    static constexpr const char* kTerrainFragmentShader = R"GLSL(
#version 120
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform vec3 lightDir;
uniform float ambientLight;
varying vec2 vUv;
varying vec4 vMix;
varying vec3 vNormal;

void main() {
    vec4 weights = clamp(vMix, 0.0, 1.0);
    vec4 blended =
        texture2D(tex0, vUv) * weights.r +
        texture2D(tex1, vUv) * weights.g +
        texture2D(tex2, vUv) * weights.b +
        texture2D(tex3, vUv) * weights.a;
    float lambert = max(dot(normalize(vNormal), normalize(lightDir)), 0.0);
    float lighting = clamp(ambientLight + lambert * (1.0 - ambientLight), 0.0, 1.0);
    gl_FragColor = vec4(clamp(blended.rgb * lighting, 0.0, 1.0), 1.0);
}
)GLSL";

    const GLuint vertexShader = CompileTerrainShader(GL_VERTEX_SHADER, kTerrainVertexShader, "vertex");
    if (vertexShader == 0) {
        return 0;
    }
    const GLuint fragmentShader = CompileTerrainShader(GL_FRAGMENT_SHADER, kTerrainFragmentShader, "fragment");
    if (fragmentShader == 0) {
        gGlDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = gGlCreateProgram();
    gGlAttachShader(program, vertexShader);
    gGlAttachShader(program, fragmentShader);
    gGlLinkProgram(program);

    GLint linked = 0;
    gGlGetProgramiv(program, GL_LINK_STATUS, &linked);
    gGlDeleteShader(vertexShader);
    gGlDeleteShader(fragmentShader);
    if (linked == 0) {
        gGlDeleteProgram(program);
        return 0;
    }

    gTerrainShaderProgram = program;
    gTerrainTextureUniforms[0] = gGlGetUniformLocation(program, "tex0");
    gTerrainTextureUniforms[1] = gGlGetUniformLocation(program, "tex1");
    gTerrainTextureUniforms[2] = gGlGetUniformLocation(program, "tex2");
    gTerrainTextureUniforms[3] = gGlGetUniformLocation(program, "tex3");
    gTerrainLightDirUniform = gGlGetUniformLocation(program, "lightDir");
    gTerrainAmbientUniform = gGlGetUniformLocation(program, "ambientLight");
    return gTerrainShaderProgram;
}
