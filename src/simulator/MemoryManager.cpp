#include "MemoryManager.h"

MemoryManager::MemoryManager(int frameCount) {
    frames.resize(frameCount);
}

void MemoryManager::access(int pid, int page) {

    if(processes.find(pid) == processes.end())
        processes.emplace(pid, Process(pid));

    Process& P = processes[pid];

    // HIT
    if(P.pageTable[page].present) {
        hits++;
        return;
    }

    // MISS → PAGE FAULT
    faults++;

    // Find free frame
    for(int i = 0; i < frames.size(); i++) {
        if(frames[i].free) {
            frames[i].free = false;
            frames[i].pid = pid;
            frames[i].page = page;

            P.pageTable[page].present = true;
            P.pageTable[page].frame = i;

            fifo.push(i);
            return;
        }
    }

    // NO FREE FRAME → EVICT
    int victim = fifo.evict();
    int oldPid = frames[victim].pid;
    int oldPage = frames[victim].page;

    processes[oldPid].pageTable[oldPage].present = false;

    // Load new page
    frames[victim].pid = pid;
    frames[victim].page = page;

    P.pageTable[page].present = true;
    P.pageTable[page].frame = victim;

    fifo.push(victim);
}

void MemoryManager::printFrames() const {
    std::cout << "Frames: ";
    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].free)
            std::cout << "[" << i << ": free] ";
        else
            std::cout << "[" << i << ": P" << frames[i].pid 
                      << ",p" << frames[i].page << "] ";
    }
    std::cout << "\n";
}

void MemoryManager::printPageTables() const {
    for(auto &p : processes) {
        std::cout << "Process " << p.first << ": ";
        for(int i = 0; i < 8; i++) {
            std::cout << (p.second.pageTable[i].present ? "1" : "0");
        }
        std::cout << "\n";
    }
}

void MemoryManager::printSummary() const {
    std::cout << "\n==== SUMMARY ====\n";
    std::cout << "Page Hits: " << hits << "\n";
    std::cout << "Page Faults: " << faults << "\n";
}
