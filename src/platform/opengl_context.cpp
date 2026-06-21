void SetupOpenGl(HWND hwnd) {
    gDeviceContext = GetDC(hwnd);
    if (gDeviceContext == nullptr) {
        throw std::runtime_error("Could not get a window device context.");
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat(gDeviceContext, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(gDeviceContext, pixelFormat, &pfd)) {
        throw std::runtime_error("Could not set an OpenGL pixel format.");
    }

    gOpenGlContext = wglCreateContext(gDeviceContext);
    if (gOpenGlContext == nullptr || !wglMakeCurrent(gDeviceContext, gOpenGlContext)) {
        throw std::runtime_error("Could not create an OpenGL context.");
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);
    gClientWidth = std::max(1L, rect.right - rect.left);
    gClientHeight = std::max(1L, rect.bottom - rect.top);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    gGlUseProgram = reinterpret_cast<GlUseProgramFn>(wglGetProcAddress("glUseProgram"));
    gGlCreateShader = reinterpret_cast<GlCreateShaderFn>(wglGetProcAddress("glCreateShader"));
    gGlShaderSource = reinterpret_cast<GlShaderSourceFn>(wglGetProcAddress("glShaderSource"));
    gGlCompileShader = reinterpret_cast<GlCompileShaderFn>(wglGetProcAddress("glCompileShader"));
    gGlGetShaderiv = reinterpret_cast<GlGetShaderivFn>(wglGetProcAddress("glGetShaderiv"));
    gGlGetShaderInfoLog = reinterpret_cast<GlGetShaderInfoLogFn>(wglGetProcAddress("glGetShaderInfoLog"));
    gGlDeleteShader = reinterpret_cast<GlDeleteShaderFn>(wglGetProcAddress("glDeleteShader"));
    gGlCreateProgram = reinterpret_cast<GlCreateProgramFn>(wglGetProcAddress("glCreateProgram"));
    gGlAttachShader = reinterpret_cast<GlAttachShaderFn>(wglGetProcAddress("glAttachShader"));
    gGlLinkProgram = reinterpret_cast<GlLinkProgramFn>(wglGetProcAddress("glLinkProgram"));
    gGlGetProgramiv = reinterpret_cast<GlGetProgramivFn>(wglGetProcAddress("glGetProgramiv"));
    gGlGetProgramInfoLog = reinterpret_cast<GlGetProgramInfoLogFn>(wglGetProcAddress("glGetProgramInfoLog"));
    gGlDeleteProgram = reinterpret_cast<GlDeleteProgramFn>(wglGetProcAddress("glDeleteProgram"));
    gGlGetUniformLocation = reinterpret_cast<GlGetUniformLocationFn>(wglGetProcAddress("glGetUniformLocation"));
    gGlUniform1i = reinterpret_cast<GlUniform1iFn>(wglGetProcAddress("glUniform1i"));
    gGlUniform3f = reinterpret_cast<GlUniform3fFn>(wglGetProcAddress("glUniform3f"));
    gGlUniform1f = reinterpret_cast<GlUniform1fFn>(wglGetProcAddress("glUniform1f"));
    gGlActiveTexture = reinterpret_cast<GlActiveTextureFn>(wglGetProcAddress("glActiveTexture"));
    const bool shaderApiLoaded =
        gGlUseProgram != nullptr &&
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

void CleanupOpenGl(HWND hwnd) {
    if (gTerrainShaderProgram != 0 && gGlDeleteProgram != nullptr) {
        gGlDeleteProgram(gTerrainShaderProgram);
        gTerrainShaderProgram = 0;
    }
    if (gBlackTextureId != 0) {
        glDeleteTextures(1, &gBlackTextureId);
        gBlackTextureId = 0;
    }
    if (gOpenGlContext != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gOpenGlContext);
        gOpenGlContext = nullptr;
    }
    if (gDeviceContext != nullptr) {
        ReleaseDC(hwnd, gDeviceContext);
        gDeviceContext = nullptr;
    }
}

void BeginOpenGlFrame() {
    glViewport(0, 0, gClientWidth, gClientHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.085f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PresentOpenGlFrame() {
    SwapBuffers(gDeviceContext);
}
