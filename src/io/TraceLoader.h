#pragma once
#include <vector>
#include <string>

struct TraceEntry {
    int pid;
    int page;
};

class TraceLoader {
public:
    std::vector<TraceEntry> load(const std::string& filename);
};
