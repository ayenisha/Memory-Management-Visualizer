#pragma once
#include <vector>
#include <unordered_map>
#include <iostream>
#include "Process.h"
#include "FIFOReplacer.h"

struct Frame {
    bool free = true;
    int pid = -1;
    int page = -1;
};

class MemoryManager {
private:
    std::vector<Frame> frames;
    std::unordered_map<int, Process> processes;
    FIFOReplacer fifo;
    int faults = 0;
    int hits = 0;

public:
    MemoryManager(int frameCount);

    void access(int pid, int page);
    void printFrames() const;
    void printPageTables() const;
    void printSummary() const;
};
