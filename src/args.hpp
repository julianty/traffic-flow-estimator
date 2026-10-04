#pragma once
#include<string>

struct InputFlags {
    std::string video_path = "";
    bool headless = false;
    double scale = 1.0;
};

InputFlags argParse(int argc, char* argv[]);
void printUsage(const char* prog); 