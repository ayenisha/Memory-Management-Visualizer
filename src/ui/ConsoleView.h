#pragma once
#include "../simulator/MemoryManager.h"
#include "../io/TraceLoader.h"

class ConsoleView {
public:
    void render(int step, const MemoryManager& m, const TraceEntry& r);
};

