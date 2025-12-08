#include "ConsoleView.h"
#include <iostream>

void ConsoleView::render(int step, const MemoryManager& m, const TraceEntry& r) {
    std::cout << "\nStep " << step << ": P" << r.pid
              << " accesses page " << r.page << "\n";
    m.printFrames();
    m.printPageTables();
}
