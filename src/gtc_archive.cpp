#include "gtc_archive.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace gtc {
namespace {

constexpr std::uint32_t kFileListRecordSize = 136;
constexpr std::uint32_t kFileNameBytes = 128;
constexpr std::uint32_t kUncompressedBlockSize = 131072;

std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw ArchiveError("Could not open " + path.string());
    }

    const auto size = file.tellg();
    if (size < 0) {
        throw ArchiveError("Could not measure " + path.string());
    }

    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty() && !file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw ArchiveError("Could not read " + path.string());
    }

    return bytes;
}

std::uint32_t ReadU32Le(const char* data) {
    const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]));
    const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]));
    const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]));
    const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

std::string ReadFixedString(const char* data, std::size_t size) {
    const auto end = std::find(data, data + size, '\0');
    return std::string(data, end);
}

void DetectCompressionFormat(const std::vector<char>& bytes,
                             std::uint32_t& formatVersion,
                             std::uint32_t& recordSize) {
    if (bytes.empty()) {
        throw ArchiveError("COMPRESS.INF is empty.");
    }

    if ((bytes.size() >= 16 && ReadU32Le(bytes.data() + 8) == ReadU32Le(bytes.data() + 12)) ||
        bytes.size() == 12) {
        formatVersion = 2;
        recordSize = 12;
        return;
    }

    if ((bytes.size() >= 24 && ReadU32Le(bytes.data() + 8) == ReadU32Le(bytes.data() + 20)) ||
        bytes.size() == 20) {
        formatVersion = 1;
        recordSize = 20;
        return;
    }

    throw ArchiveError("Could not detect the COMPRESS.INF record format.");
}

std::vector<FileEntry> ParseFileList(const std::vector<char>& bytes) {
    if (bytes.empty()) {
        throw ArchiveError("FILELIST.INF is empty.");
    }

    if (bytes.size() % kFileListRecordSize != 0) {
        throw ArchiveError("FILELIST.INF size is not a multiple of 136 bytes.");
    }

    std::vector<FileEntry> entries;
    entries.reserve(bytes.size() / kFileListRecordSize);

    for (std::size_t offset = 0; offset < bytes.size(); offset += kFileListRecordSize) {
        FileEntry entry;
        entry.path = ReadFixedString(bytes.data() + offset, kFileNameBytes);
        entry.offset = ReadU32Le(bytes.data() + offset + 128);
        entry.size = ReadU32Le(bytes.data() + offset + 132);

        if (!entry.path.empty()) {
            entries.push_back(std::move(entry));
        }
    }

    if (entries.empty()) {
        throw ArchiveError("FILELIST.INF did not contain any named entries.");
    }

    return entries;
}

std::vector<CompressionBlock> ParseCompressionBlocks(const std::vector<char>& bytes,
                                                     std::uint32_t recordSize) {
    if (bytes.size() % recordSize != 0) {
        throw ArchiveError("COMPRESS.INF size is not a multiple of the detected record size.");
    }

    std::vector<CompressionBlock> blocks;
    blocks.reserve(bytes.size() / recordSize);

    for (std::size_t offset = 0; offset < bytes.size(); offset += recordSize) {
        CompressionBlock block;
        block.offset = ReadU32Le(bytes.data() + offset);
        block.block = ReadU32Le(bytes.data() + offset + 4);
        block.size = ReadU32Le(bytes.data() + offset + 8);
        blocks.push_back(block);
    }

    return blocks;
}

void RequireRange(std::size_t offset,
                  std::size_t size,
                  std::size_t containerSize,
                  const std::string& message) {
    if (offset > containerSize || size > containerSize - offset) {
        throw ArchiveError(message);
    }
}

void CopyLiteralByte(const std::vector<char>& compressed,
                     std::size_t& inputPos,
                     std::size_t inputLimit,
                     std::vector<char>& decompressed,
                     std::size_t& outputPos,
                     std::size_t outputLimit) {
    RequireRange(inputPos, 1, inputLimit, "Compressed block read exceeded its block range.");
    if (outputPos >= outputLimit) {
        throw ArchiveError("Decompressed block exceeded its expected output size.");
    }

    decompressed[outputPos++] = compressed[inputPos++];
}

void CopyBackReference(std::vector<char>& decompressed,
                       std::size_t& outputPos,
                       std::size_t outputLimit,
                       int backReferenceJump,
                       std::uint32_t backReferenceSize) {
    for (std::uint32_t index = 0; index < backReferenceSize; ++index) {
        const auto sourcePos = static_cast<std::int64_t>(outputPos) + backReferenceJump;
        if (sourcePos < 0 || static_cast<std::uint64_t>(sourcePos) >= outputPos) {
            throw ArchiveError("Compressed block contains an invalid back-reference.");
        }
        if (outputPos >= outputLimit) {
            throw ArchiveError("Decompressed block exceeded its expected output size.");
        }

        decompressed[outputPos] = decompressed[static_cast<std::size_t>(sourcePos)];
        ++outputPos;
    }
}

std::vector<char> DecompressArchiveData(const ArchiveInfo& archive,
                                        const DumpProgressCallback& progress) {
    if (archive.compressionBlocks.empty()) {
        throw ArchiveError("Archive does not contain compression block metadata.");
    }

    const auto compressed = ReadBinaryFile(archive.gtcPath);
    const std::size_t decompressedSize =
        archive.compressionBlocks.size() * static_cast<std::size_t>(kUncompressedBlockSize);
    std::vector<char> decompressed(decompressedSize);

    for (std::size_t blockIndex = 0; blockIndex < archive.compressionBlocks.size(); ++blockIndex) {
        if (progress) {
            DumpProgress dumpProgress;
            dumpProgress.filesWritten = blockIndex;
            dumpProgress.totalFiles = archive.compressionBlocks.size();
            dumpProgress.message = "Decompressing blocks";
            progress(dumpProgress);
        }

        const CompressionBlock& block = archive.compressionBlocks[blockIndex];
        const std::size_t blockEnd = static_cast<std::size_t>(block.offset) + block.size;
        if (block.block > blockEnd) {
            throw ArchiveError("Compression block start is before the beginning of GAMEDATA.GTC.");
        }
        const std::size_t inputStart = blockEnd - block.block;
        const std::size_t outputStart = blockIndex * static_cast<std::size_t>(kUncompressedBlockSize);
        const std::size_t outputLimit = outputStart + kUncompressedBlockSize;

        RequireRange(inputStart, block.block, compressed.size(),
                     "Compression block range exceeds GAMEDATA.GTC.");
        RequireRange(outputStart, kUncompressedBlockSize, decompressed.size(),
                     "Compression block output range is invalid.");

        std::size_t inputPos = inputStart;
        std::size_t outputPos = outputStart;

        if (block.size == kUncompressedBlockSize) {
            RequireRange(inputPos, block.size, compressed.size(),
                         "Uncompressed block range exceeds GAMEDATA.GTC.");
            std::memcpy(decompressed.data() + outputPos, compressed.data() + inputPos, block.size);
            continue;
        }

        bool readingBackReference = false;
        std::uint32_t singleBackReferenceBits = 0;
        std::uint32_t backReferenceSize = 0;
        int backReferenceJump = 0;

        while (inputPos < blockEnd) {
            if (blockEnd - inputPos < 4) {
                break;
            }

            RequireRange(inputPos, 4, blockEnd, "Compressed command read exceeded its block range.");
            const std::uint32_t command = ReadU32Le(compressed.data() + inputPos);
            inputPos += 4;

            for (std::uint32_t bitIndex = 0;
                 bitIndex < 32 && inputPos < blockEnd && outputPos < outputLimit;
                 ++bitIndex) {
                const bool bit = ((command >> bitIndex) & 1U) != 0;

                if (readingBackReference) {
                    if (singleBackReferenceBits != 0) {
                        if (bit) {
                            backReferenceSize += singleBackReferenceBits;
                        }

                        --singleBackReferenceBits;
                        if (singleBackReferenceBits == 0) {
                            CopyBackReference(decompressed, outputPos, outputLimit,
                                              backReferenceJump, backReferenceSize);
                            readingBackReference = false;
                        }
                    } else if (bit) {
                        RequireRange(inputPos, 2, blockEnd,
                                     "Compressed multi-byte back-reference exceeded its block range.");

                        backReferenceSize =
                            static_cast<unsigned char>(compressed[inputPos]) & 0x07U;
                        backReferenceJump =
                            -8192 +
                            ((((static_cast<unsigned char>(compressed[inputPos]) & 0xF8U) >> 3U) |
                              (static_cast<unsigned char>(compressed[inputPos + 1]) << 5U)));
                        inputPos += 2;

                        if (backReferenceSize == 0) {
                            RequireRange(inputPos, 1, blockEnd,
                                         "Compressed back-reference size exceeded its block range.");
                            backReferenceSize =
                                static_cast<unsigned char>(compressed[inputPos]) & 0x7FU;

                            if ((static_cast<unsigned char>(compressed[inputPos]) & 0x80U) != 0) {
                                backReferenceJump -= 8192;
                            }
                            ++inputPos;

                            if (backReferenceSize == 0) {
                                RequireRange(inputPos, 2, blockEnd,
                                             "Compressed extended back-reference size exceeded its block range.");
                                backReferenceSize =
                                    static_cast<unsigned char>(compressed[inputPos]) |
                                    (static_cast<unsigned char>(compressed[inputPos + 1]) << 8U);
                                inputPos += 2;
                            } else {
                                backReferenceSize += 2;
                            }
                        } else {
                            backReferenceSize += 2;
                        }

                        CopyBackReference(decompressed, outputPos, outputLimit,
                                          backReferenceJump, backReferenceSize);
                        readingBackReference = false;
                    } else {
                        RequireRange(inputPos, 1, blockEnd,
                                     "Compressed single-byte back-reference exceeded its block range.");
                        singleBackReferenceBits = 2;
                        backReferenceSize = 2;
                        backReferenceJump =
                            -256 + static_cast<unsigned char>(compressed[inputPos]);
                        ++inputPos;
                    }
                } else if (bit) {
                    readingBackReference = true;
                    singleBackReferenceBits = 0;
                } else {
                    CopyLiteralByte(compressed, inputPos, blockEnd,
                                    decompressed, outputPos, outputLimit);
                }
            }
        }
    }

    if (progress) {
        DumpProgress dumpProgress;
        dumpProgress.filesWritten = archive.compressionBlocks.size();
        dumpProgress.totalFiles = archive.compressionBlocks.size();
        dumpProgress.message = "Decompressing blocks";
        progress(dumpProgress);
    }

    return decompressed;
}

std::filesystem::path BuildSafeOutputPath(const std::filesystem::path& outputDirectory,
                                          const std::string& archivePath) {
    std::filesystem::path relativePath(archivePath);
    if (relativePath.is_absolute()) {
        throw ArchiveError("Archive entry is absolute: " + archivePath);
    }

    std::filesystem::path outputPath = outputDirectory;
    for (const auto& component : relativePath) {
        const auto componentText = component.string();
        if (componentText.empty() || componentText == ".") {
            continue;
        }
        if (componentText == ".." || component.has_root_directory() || component.has_root_name()) {
            throw ArchiveError("Archive entry escapes the output folder: " + archivePath);
        }

        outputPath /= component;
    }

    return outputPath;
}

void WriteEntry(const std::vector<char>& decompressed,
                const FileEntry& entry,
                const std::filesystem::path& outputDirectory) {
    RequireRange(entry.offset, entry.size, decompressed.size(),
                 "File entry range exceeds the decompressed archive: " + entry.path);

    const auto outputPath = BuildSafeOutputPath(outputDirectory, entry.path);
    const auto parentPath = outputPath.parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw ArchiveError("Could not create " + outputPath.string());
    }

    if (entry.size > 0) {
        file.write(decompressed.data() + entry.offset, static_cast<std::streamsize>(entry.size));
        if (!file) {
            throw ArchiveError("Could not write " + outputPath.string());
        }
    }
}

} // namespace

ArchiveError::ArchiveError(const std::string& message)
    : std::runtime_error(message) {
}

ArchiveInfo LoadArchive(const std::filesystem::path& gtcPath) {
    if (gtcPath.empty()) {
        throw ArchiveError("No GTC file was selected.");
    }

    if (!std::filesystem::exists(gtcPath)) {
        throw ArchiveError("The selected GTC file does not exist: " + gtcPath.string());
    }

    ArchiveInfo info;
    info.gtcPath = gtcPath;
    info.gtcSize = std::filesystem::file_size(gtcPath);

    const auto folder = gtcPath.parent_path();
    info.fileListPath = folder / "FILELIST.INF";
    info.compressPath = folder / "COMPRESS.INF";

    if (!std::filesystem::exists(info.fileListPath)) {
        throw ArchiveError("Missing FILELIST.INF beside " + gtcPath.filename().string());
    }

    if (!std::filesystem::exists(info.compressPath)) {
        throw ArchiveError("Missing COMPRESS.INF beside " + gtcPath.filename().string());
    }

    const auto fileList = ReadBinaryFile(info.fileListPath);
    const auto compression = ReadBinaryFile(info.compressPath);

    DetectCompressionFormat(compression, info.formatVersion, info.compressionRecordSize);
    info.entries = ParseFileList(fileList);
    info.compressionBlocks = ParseCompressionBlocks(compression, info.compressionRecordSize);

    return info;
}

std::vector<char> ReadArchiveData(const ArchiveInfo& archive,
                                  const DumpProgressCallback& progress) {
    if (archive.gtcPath.empty() || archive.entries.empty() || archive.compressionBlocks.empty()) {
        throw ArchiveError("No loaded archive is available.");
    }

    return DecompressArchiveData(archive, progress);
}

std::vector<char> ReadEntryData(const ArchiveInfo& archive,
                                std::size_t entryIndex,
                                const std::vector<char>& decompressedArchive) {
    if (entryIndex >= archive.entries.size()) {
        throw ArchiveError("Archive entry index is out of range.");
    }

    const FileEntry& entry = archive.entries[entryIndex];
    RequireRange(entry.offset, entry.size, decompressedArchive.size(),
                 "File entry range exceeds the decompressed archive: " + entry.path);

    const char* start = decompressedArchive.data() + entry.offset;
    return std::vector<char>(start, start + entry.size);
}

void DumpArchive(const ArchiveInfo& archive,
                 const std::filesystem::path& outputDirectory,
                 const DumpProgressCallback& progress) {
    if (archive.gtcPath.empty() || archive.entries.empty() || archive.compressionBlocks.empty()) {
        throw ArchiveError("No loaded archive is available to dump.");
    }

    if (outputDirectory.empty()) {
        throw ArchiveError("No output folder was selected.");
    }

    std::filesystem::create_directories(outputDirectory);
    if (!std::filesystem::is_directory(outputDirectory)) {
        throw ArchiveError("Output path is not a folder: " + outputDirectory.string());
    }

    DumpProgress dumpProgress;
    dumpProgress.totalFiles = archive.entries.size();
    dumpProgress.message = "Starting dump";
    if (progress) {
        progress(dumpProgress);
    }

    const auto decompressed = DecompressArchiveData(archive, progress);

    dumpProgress.filesWritten = 0;
    dumpProgress.totalFiles = archive.entries.size();
    dumpProgress.currentPath.clear();
    dumpProgress.message = "Writing files";
    if (progress) {
        progress(dumpProgress);
    }

    for (const auto& entry : archive.entries) {
        dumpProgress.currentPath = BuildSafeOutputPath(outputDirectory, entry.path);
        WriteEntry(decompressed, entry, outputDirectory);
        ++dumpProgress.filesWritten;
        dumpProgress.message = "Writing files";

        if (progress) {
            progress(dumpProgress);
        }
    }
}

std::string FormatByteSize(std::uint64_t bytes) {
    constexpr std::array<const char*, 5> suffixes = {"B", "KB", "MB", "GB", "TB"};

    double value = static_cast<double>(bytes);
    std::size_t suffixIndex = 0;
    while (value >= 1024.0 && suffixIndex + 1 < suffixes.size()) {
        value /= 1024.0;
        ++suffixIndex;
    }

    std::ostringstream out;
    if (suffixIndex == 0) {
        out << bytes << ' ' << suffixes[suffixIndex];
    } else {
        out << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1)
            << value << ' ' << suffixes[suffixIndex];
    }
    return out.str();
}

} // namespace gtc
