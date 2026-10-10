#pragma once
#include<string>
#include "ccl.hpp"

struct InputFlags {
    std::string video_path = "";
    bool headless = false;
    double scale = 1.0;
    Neighbors kneighbors = Neighbors::Eight;
};

InputFlags argParse(int argc, char* argv[]);
void printUsage(const char* prog); 