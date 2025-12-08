#include <iostream>
#include "simulator/MemoryManager.h"
#include "io/TraceLoader.h"
#include "ui/ConsoleView.h"

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "Usage: ./memory_visualizer <tracefile>\n";
        return 1;
    }

    std::string filename = argv[1];

    TraceLoader loader;
    auto trace = loader.load(filename);

    MemoryManager manager(4); // 4 physical frames
    ConsoleView view;

    for(size_t step = 0; step < trace.size(); step++) {
        auto& r = trace[step];

        manager.access(r.pid, r.page);
        view.render(step, manager, r);
    }

    manager.printSummary();
    return 0;
}
