bool NodeNameLess(const ArchiveBrowser& browser, int left, int right) {
    return ToLower(browser.nodes[left].name) < ToLower(browser.nodes[right].name);
}

void SortBrowserNode(ArchiveBrowser& browser, int nodeIndex) {
    auto& node = browser.nodes[nodeIndex];
    std::sort(node.folders.begin(), node.folders.end(), [&](int left, int right) {
        return NodeNameLess(browser, left, right);
    });
    std::sort(node.files.begin(), node.files.end(), [&](int left, int right) {
        return NodeNameLess(browser, left, right);
    });
    const auto childFolders = node.folders;
    for (const int folderIndex : childFolders) {
        SortBrowserNode(browser, folderIndex);
    }
}

int EnsureBrowserFolder(ArchiveBrowser& browser, int parentIndex, const std::string& name) {
    ArchiveNode& parent = browser.nodes[parentIndex];
    const std::string key = ToLower(name);
    const auto found = parent.folderLookup.find(key);
    if (found != parent.folderLookup.end()) {
        return found->second;
    }

    ArchiveNode folder;
    folder.name = name;
    folder.path = JoinArchivePath(parent.path, folder.name);
    folder.directory = true;
    folder.parent = parentIndex;

    const int folderIndex = static_cast<int>(browser.nodes.size());
    browser.nodes.push_back(std::move(folder));
    browser.nodes[parentIndex].folders.push_back(folderIndex);
    browser.nodes[parentIndex].folderLookup[key] = folderIndex;
    return folderIndex;
}

bool IsMusicTrackExtension(const std::string& extension) {
    return extension == ".1" ||
           extension == ".2" ||
           extension == ".3" ||
           extension == ".4" ||
           extension == ".5";
}

std::filesystem::path MusicDirectoryForArchive(const gtc::ArchiveInfo& archive) {
    return archive.gtcPath.parent_path() / "game data" / "music";
}

void AddExternalMusicFiles(AppState& state, ArchiveBrowser& browser) {
    const std::filesystem::path musicDirectory = MusicDirectoryForArchive(state.archive);
    if (!std::filesystem::exists(musicDirectory) || !std::filesystem::is_directory(musicDirectory)) {
        return;
    }

    std::vector<std::filesystem::path> musicFiles;
    for (const auto& entry : std::filesystem::directory_iterator(musicDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsMusicTrackExtension(ToLower(entry.path().extension().string()))) {
            musicFiles.push_back(entry.path());
        }
    }

    std::sort(musicFiles.begin(), musicFiles.end(), [](const auto& left, const auto& right) {
        return ToLower(left.filename().string()) < ToLower(right.filename().string());
    });

    if (musicFiles.empty()) {
        return;
    }

    const int gameDataFolder = EnsureBrowserFolder(browser, 0, "GAME DATA");
    const int musicFolder = EnsureBrowserFolder(browser, gameDataFolder, "MUSIC");
    for (const std::filesystem::path& path : musicFiles) {
        ArchiveNode file;
        file.name = path.filename().string();
        file.path = JoinArchivePath(browser.nodes[musicFolder].path, file.name);
        file.directory = false;
        file.parent = musicFolder;
        file.externalFile = true;
        file.externalPath = path;

        const int fileIndex = static_cast<int>(browser.nodes.size());
        browser.nodes.push_back(std::move(file));
        browser.nodes[musicFolder].files.push_back(fileIndex);
    }

}

void BuildBrowser(AppState& state) {
    ArchiveBrowser browser;
    ArchiveNode root;
    root.name = state.archive.gtcPath.filename().string();
    if (root.name.empty()) {
        root.name = "GAMEDATA.GTC";
    }
    root.path.clear();
    root.directory = true;
    root.parent = -1;
    browser.nodes.push_back(std::move(root));

    for (std::size_t entryIndex = 0; entryIndex < state.archive.entries.size(); ++entryIndex) {
        const auto parts = SplitArchivePath(state.archive.entries[entryIndex].path);
        if (parts.empty()) {
            continue;
        }

        int current = 0;
        for (std::size_t partIndex = 0; partIndex + 1 < parts.size(); ++partIndex) {
            const std::string key = ToLower(parts[partIndex]);
            auto found = browser.nodes[current].folderLookup.find(key);
            if (found != browser.nodes[current].folderLookup.end()) {
                current = found->second;
                continue;
            }

            ArchiveNode folder;
            folder.name = parts[partIndex];
            folder.path = JoinArchivePath(browser.nodes[current].path, folder.name);
            folder.directory = true;
            folder.parent = current;

            const int folderIndex = static_cast<int>(browser.nodes.size());
            browser.nodes.push_back(std::move(folder));
            browser.nodes[current].folders.push_back(folderIndex);
            browser.nodes[current].folderLookup[key] = folderIndex;
            current = folderIndex;
        }

        ArchiveNode file;
        file.name = parts.back();
        file.path = JoinArchivePath(browser.nodes[current].path, file.name);
        file.directory = false;
        file.parent = current;
        file.entryIndex = entryIndex;

        const int fileIndex = static_cast<int>(browser.nodes.size());
        browser.nodes.push_back(std::move(file));
        browser.nodes[current].files.push_back(fileIndex);
    }

    AddExternalMusicFiles(state, browser);

    SortBrowserNode(browser, 0);
    browser.selectedFolder = 0;
    browser.selectedItem = 0;
    state.browser = std::move(browser);
}

void RebuildVisibleEntries(AppState&) {
}

void LoadArchive(AppState& state, const std::filesystem::path& path) {
    try {
        state.archive = gtc::LoadArchive(path);
        state.archiveLoaded = true;
        state.archiveDataLoaded = false;
        state.archiveData.clear();
        StopAudioPreview(state.audioPreview);
        StopFxPreview(state.fxPreview);
        DestroyPreviewTexture(state.texturePreview);
        state.texturePreview.open = false;
        state.textPreview = {};
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = {};
        StopLevelPreview(state.levelPreview);
        state.selectedPath = state.archive.gtcPath.string();
        state.status = "Loaded " + state.selectedPath;
        BuildBrowser(state);
        SaveSavedGtcPath(state.archive.gtcPath);
    } catch (const std::exception& error) {
        state.archiveLoaded = false;
        state.archiveDataLoaded = false;
        state.archiveData.clear();
        state.browser = {};
        StopAudioPreview(state.audioPreview);
        StopFxPreview(state.fxPreview);
        DestroyPreviewTexture(state.texturePreview);
        state.texturePreview.open = false;
        state.textPreview = {};
        DestroyModelRenderTexture(state.modelPreview);
        state.modelPreview = {};
        StopLevelPreview(state.levelPreview);
        state.status = std::string("Load failed: ") + error.what();
    }
}

DumpSnapshot GetDumpSnapshot(AppState& state) {
    std::lock_guard lock(state.dumpMutex);
    return {
        state.dumpActive,
        state.dumpFinished,
        state.dumpSucceeded,
        state.dumpFilesWritten,
        state.dumpTotalFiles,
        state.dumpCurrentPath,
        state.dumpMessage,
    };
}

bool IsDumpActive(AppState& state) {
    std::lock_guard lock(state.dumpMutex);
    return state.dumpActive;
}

void PollDumpTask(AppState& state) {
    if (state.dumpFuture.valid() &&
        state.dumpFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        state.dumpFuture.get();

        std::lock_guard lock(state.dumpMutex);
        state.dumpActive = false;
        state.status = state.dumpMessage;
    }
}

void StartDump(AppState& state, const std::filesystem::path& outputDirectory) {
    if (!state.archiveLoaded) {
        state.status = "Load a GTC archive before dumping.";
        return;
    }

    {
        std::lock_guard lock(state.dumpMutex);
        if (state.dumpActive) {
            return;
        }

        state.dumpActive = true;
        state.dumpFinished = false;
        state.dumpSucceeded = false;
        state.dumpFilesWritten = 0;
        state.dumpTotalFiles = state.archive.entries.size();
        state.dumpCurrentPath.clear();
        state.dumpMessage = "Starting dump to " + outputDirectory.string();
    }

    const gtc::ArchiveInfo archive = state.archive;
    state.dumpFuture = std::async(std::launch::async, [&state, archive, outputDirectory]() {
        try {
            gtc::DumpArchive(archive, outputDirectory, [&state](const gtc::DumpProgress& progress) {
                std::lock_guard lock(state.dumpMutex);
                state.dumpFilesWritten = progress.filesWritten;
                state.dumpTotalFiles = progress.totalFiles;
                state.dumpCurrentPath = progress.currentPath.string();
                state.dumpMessage = progress.message.empty() ? "Dumping" : progress.message;
            });

            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = true;
            state.dumpMessage = "Dumped " + std::to_string(archive.entries.size()) +
                                " files to " + outputDirectory.string();
        } catch (const std::exception& error) {
            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = false;
            state.dumpMessage = std::string("Dump failed: ") + error.what();
        } catch (...) {
            std::lock_guard lock(state.dumpMutex);
            state.dumpFinished = true;
            state.dumpSucceeded = false;
            state.dumpMessage = "Dump failed: unknown error.";
        }
    });
}

void OpenGtcDialog() {
    IGFD::FileDialogConfig config;
    config.path = PreferredInitialDirectory();
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal |
                   ImGuiFileDialogFlags_ReadOnlyFileNameField |
                   ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;

    ImGuiFileDialog::Instance()->OpenDialog(
        kChooseGtcDialogKey,
        "Open GAMEDATA.GTC",
        ".gtc",
        config);
}

void OpenDumpDirectoryDialog(const AppState& state) {
    IGFD::FileDialogConfig config;
    config.path = state.archiveLoaded && state.archive.gtcPath.has_parent_path()
                      ? state.archive.gtcPath.parent_path().string()
                      : PreferredInitialDirectory();
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_Modal;

    ImGuiFileDialog::Instance()->OpenDialog(
        kChooseDumpDirectoryDialogKey,
        "Dump All To Folder",
        nullptr,
        config);
}
