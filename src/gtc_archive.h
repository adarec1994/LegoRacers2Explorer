#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace gtc {

struct FileEntry {
    std::string path;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct CompressionBlock {
    std::uint32_t offset = 0;
    std::uint32_t block = 0;
    std::uint32_t size = 0;
};

struct ArchiveInfo {
    std::filesystem::path gtcPath;
    std::filesystem::path fileListPath;
    std::filesystem::path compressPath;
    std::uintmax_t gtcSize = 0;
    std::uint32_t formatVersion = 0;
    std::uint32_t compressionRecordSize = 0;
    std::vector<FileEntry> entries;
    std::vector<CompressionBlock> compressionBlocks;
};

struct DumpProgress {
    std::size_t filesWritten = 0;
    std::size_t totalFiles = 0;
    std::filesystem::path currentPath;
    std::string message;
};

using DumpProgressCallback = std::function<void(const DumpProgress&)>;

class ArchiveError : public std::runtime_error {
public:
    explicit ArchiveError(const std::string& message);
};

ArchiveInfo LoadArchive(const std::filesystem::path& gtcPath);
std::vector<char> ReadArchiveData(const ArchiveInfo& archive,
                                  const DumpProgressCallback& progress = {});
std::vector<char> ReadEntryData(const ArchiveInfo& archive,
                                std::size_t entryIndex,
                                const std::vector<char>& decompressedArchive);
void DumpArchive(const ArchiveInfo& archive,
                 const std::filesystem::path& outputDirectory,
                 const DumpProgressCallback& progress = {});

std::string FormatByteSize(std::uint64_t bytes);

}
