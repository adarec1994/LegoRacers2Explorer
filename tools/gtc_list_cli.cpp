#include "gtc_archive.h"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage:\n"
                  << "  gtc_list_cli <path-to-gamedata.gtc>\n"
                  << "  gtc_list_cli --dump <path-to-gamedata.gtc> <output-folder>\n";
        return 2;
    }

    try {
        if (argc == 4 && std::string(argv[1]) == "--dump") {
            const auto archive = gtc::LoadArchive(std::filesystem::path(argv[2]));
            gtc::DumpArchive(archive, std::filesystem::path(argv[3]), [](const gtc::DumpProgress& progress) {
                if (progress.filesWritten == 0 ||
                    progress.filesWritten == progress.totalFiles ||
                    progress.filesWritten % 250 == 0) {
                    std::cout << "Dumped " << progress.filesWritten
                              << " / " << progress.totalFiles << '\n';
                }
            });
            return 0;
        }

        if (argc != 2) {
            std::cerr << "Unknown command.\n";
            return 2;
        }

        const auto archive = gtc::LoadArchive(std::filesystem::path(argv[1]));

        std::cout << "Archive: " << archive.gtcPath.string() << '\n';
        std::cout << "Size: " << gtc::FormatByteSize(archive.gtcSize) << '\n';
        std::cout << "Format version: " << archive.formatVersion << '\n';
        std::cout << "Compression blocks: " << archive.compressionBlocks.size() << '\n';
        std::cout << "Files: " << archive.entries.size() << "\n\n";

        for (const auto& entry : archive.entries) {
            std::cout << entry.offset << '\t'
                      << entry.size << '\t'
                      << entry.path << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
