#pragma once
#include<string>
#include "ccl.hpp"

struct InputFlags {
    std::string video_path = "";
    bool headless = false;
    double scale = 1.0;
    Neighbors kneighbors = Neighbors::Four;
    std::string event_out = "";
};

InputFlags argParse(int argc, char* argv[]);
void printUsage(const char* prog); 