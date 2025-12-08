#include "TraceLoader.h"
#include <fstream>
#include <iostream>

std::vector<TraceEntry> TraceLoader::load(const std::string& filename) {
    std::vector<TraceEntry> trace;
    std::ifstream file(filename);
    if(!file) {
        std::cout << "Could not open trace file!\n";
        return trace;
    }

    int pid, page;
    while(file >> pid >> page)
        trace.push_back({pid, page});

    return trace;
}
