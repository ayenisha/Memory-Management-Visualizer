#pragma once
#include <vector>

struct PTE {
    bool present = false;
    int frame = -1;
};

class Process {
public:
    int pid;
    std::vector<PTE> pageTable;

    Process(int pid, int pageCount = 32)
        : pid(pid), pageTable(pageCount) {}
};
