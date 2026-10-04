#include"args.hpp"
#include<iostream>
#include<stdexcept>
#include<string>

namespace {
std::string getNextArg(int argc, char* argv[], int& i, std::string flag) {
    // The next argument doesn't exist
    if (i+1 >= argc) {
        throw std::runtime_error("--" + flag + " requires an argument");
    }
    // Return next arg
    return argv[++i];
}
} // namespace

InputFlags argParse(int argc, char* argv[]) {
    InputFlags flags;
    for (int i=1; i<argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video_path") {
            std::string path =  getNextArg(argc, argv, i, "video_path");
            flags.video_path = path;
        } else if (arg == "--headless") {
            flags.headless = true;
        } else if (arg == "--scale") {
            std::size_t consumed = 0;
            std::string argText = getNextArg(argc, argv, i, "scale");
            try {
                flags.scale = std::stod(argText, &consumed);
            } catch (const std::exception&) {
                throw std::runtime_error("--scale expects a number, got '" + argText + "'");
            }
            if (consumed != argText.size()) {
                throw std::runtime_error("--scale expects a number, got '" + argText + "'");
            }
            if (flags.scale <= 0 || flags.scale > 1) {
                throw std::runtime_error("--scale must be between 0 and 1");
            }
        } else {
            // Unknown argument
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (flags.video_path.size() < 1) {
        throw std::runtime_error("--video_path is required");
    }
    return flags;
}

void printUsage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " --video_path <file> [--headless] [--scale <s>]\n"
        << "\n"
        << "  --video_path <file>  video to process (required)\n"
        << "  --headless           no window or key controls; for benchmarks\n"
        << "  --scale <s>          resize factor before MOG2, 0 < s <= 1 (default 1.0)\n";
}