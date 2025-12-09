
// main.cpp
// Simple Memory Management Visualizer (single-file, human-friendly)
// Supports three modes:
//   PAGING     : input lines "PID PAGE"   (page numbers)
//   SEGMENTATION: commands "ALLOC pid seg size" and "ACCESS pid seg offset"
//   VIRTUAL    : input lines "PID VA"     (virtual addresses, VM uses page size)
//
// Replacement algorithm for paging/virtual: "FIFO" or "LRU" passed as second arg.
// Example runs:
//   ./memory_visualizer traces_paging.txt FIFO PAGING
//   ./memory_visualizer traces_segment.txt - SEGMENTATION
//   ./memory_visualizer traces_vm.txt LRU VIRTUAL

#include <bits/stdc++.h>
using namespace std;

/* -----------------------
   Simple trace loader
   ----------------------- */
struct TraceEntry {
    // type: 0 = paging (pid,page), 1 = alloc (seg), 2 = access_seg, 3 = virtual (pid,va)
    int type = 0;
    int pid = 0;
    int page = 0;

    // segmentation fields
    int segId = 0;
    int size = 0;
    int offset = 0;

    // virtual
    int virtualAddress = 0;
};

vector<TraceEntry> loadTrace(const string &filename) {
    vector<TraceEntry> out;
    ifstream fin(filename);
    if (!fin) {
        cerr << "Could not open trace file: " << filename << "\n";
        return out;
    }

    string line;
    while (getline(fin, line)) {
        // ignore empty or comment lines
        if (line.empty()) continue;
        // trim front/back
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) continue;
        auto s = line.substr(start);
        if (s.empty()) continue;
        if (s[0]=='#') continue;

        // tokenize
        istringstream ss(s);
        vector<string> toks;
        string tok;
        while (ss >> tok) toks.push_back(tok);
        if (toks.empty()) continue;

        if (toks[0] == "ALLOC" && toks.size() >= 4) {
            TraceEntry e; e.type = 1;
            e.pid = stoi(toks[1]);
            e.segId = stoi(toks[2]);
            e.size = stoi(toks[3]);
            out.push_back(e);
        } else if (toks[0] == "ACCESS" && toks.size() >= 4) {
            TraceEntry e; e.type = 2;
            e.pid = stoi(toks[1]);
            e.segId = stoi(toks[2]);
            e.offset = stoi(toks[3]);
            out.push_back(e);
        } else if (toks.size() >= 2) {
            // numeric style: pid number
            // we store as both paging and virtual (main decides)
            TraceEntry e;
            e.type = 0; // default paging
            e.pid = stoi(toks[0]);
            int num = stoi(toks[1]);
            e.page = num;
            e.virtualAddress = num;
            out.push_back(e);
        }
    }
    return out;
}

/* -----------------------
   Replacement policies
   ----------------------- */
// FIFO Replacer
struct FIFOReplacer {
    queue<int> q;
    vector<bool> inQueue;
    FIFOReplacer() {}
    FIFOReplacer(int n) { inQueue.assign(n, false); }
    void ensure(int n) { if ((int)inQueue.size()<n) inQueue.assign(n,false); }
    void touch(int frame) {
        if (frame<0) return;
        if (frame >= (int)inQueue.size()) ensure(frame+1);
        if (!inQueue[frame]) {
            q.push(frame);
            inQueue[frame] = true;
        }
    }
    int evict() {
        while (!q.empty() && !inQueue[q.front()]) q.pop();
        if (q.empty()) return -1;
        int f = q.front(); q.pop();
        inQueue[f] = false;
        return f;
    }
    void remove(int frame) { if (frame>=0 && frame<(int)inQueue.size()) inQueue[frame]=false; }
};

// LRU Replacer
struct LRUReplacer {
    list<int> order; // front = most recent, back = least recent
    unordered_map<int, list<int>::iterator> pos;
    void touch(int frame) {
        if (frame<0) return;
        if (pos.count(frame)) {
            order.erase(pos[frame]);
            pos.erase(frame);
        }
        order.push_front(frame);
        pos[frame] = order.begin();
    }
    int evict() {
        if (order.empty()) return -1;
        int v = order.back();
        order.pop_back();
        pos.erase(v);
        return v;
    }
    void remove(int frame) {
        if (pos.count(frame)) {
            order.erase(pos[frame]);
            pos.erase(frame);
        }
    }
};

/* -----------------------
   Paging memory manager
   ----------------------- */
struct PTE {
    bool present = false;
    int frame = -1;
    bool dirty = false;
};

struct Frame {
    bool free = true;
    int pid = -1;
    int page = -1;
};

struct Process {
    int pid = -1;
    vector<PTE> pageTable;
    Process() : pid(-1), pageTable(32) {}
    Process(int id, int pageCount=32) : pid(id), pageTable(pageCount) {}
};

struct MemoryManager {
    vector<Frame> frames;
    unordered_map<int, Process> procs;
    string algorithm = "FIFO";
    FIFOReplacer fifo;
    LRUReplacer lru;
    int hits = 0;
    int faults = 0;

    MemoryManager(int frameCount=4, const string &algo="FIFO") {
        frames.assign(frameCount, Frame());
        algorithm = algo;
        fifo.ensure(frameCount);
    }

    // get process object (creates if missing)
    Process &getProcess(int pid) {
        if (!procs.count(pid)) procs.emplace(pid, Process(pid));
        return procs[pid];
    }

    // access by page number (for paging mode)
    void accessPage(int pid, int page) {
        auto &P = getProcess(pid);
        if (page < 0 || page >= (int)P.pageTable.size()) {
            cout << "Invalid page " << page << " for P" << pid << "\n";
            return;
        }
        if (P.pageTable[page].present) {
            hits++;
            int f = P.pageTable[page].frame;
            if (algorithm == "LRU") lru.touch(f);
            // FIFO: no touch on hit required
            cout << "HIT: P" << pid << ":p" << page << " in frame " << f << "\n";
            return;
        }

        // page fault
        faults++;
        cout << "PAGE FAULT: P" << pid << ":p" << page << "\n";

        // first try free frame
        int freeIdx = -1;
        for (int i = 0; i < (int)frames.size(); ++i) if (frames[i].free) { freeIdx = i; break; }

        if (freeIdx != -1) {
            loadIntoFrame(freeIdx, pid, page);
            return;
        }

        // no free frame -> evict using algorithm
        int victim = -1;
        if (algorithm == "LRU") {
            victim = lru.evict();
        } else {
            victim = fifo.evict();
        }
        if (victim == -1) {
            cout << "ERROR: no victim found\n";
            return;
        }
        int oldPid = frames[victim].pid;
        int oldPage = frames[victim].page;
        cout << "EVICT: frame " << victim << " (P" << oldPid << ", p" << oldPage << ")\n";
        // mark old not present
        if (procs.count(oldPid)) procs[oldPid].pageTable[oldPage].present = false;
        // load new
        loadIntoFrame(victim, pid, page);
    }

    void loadIntoFrame(int idx, int pid, int page) {
        frames[idx].free = false;
        frames[idx].pid = pid;
        frames[idx].page = page;
        Process &P = getProcess(pid);
        P.pageTable[page].present = true;
        P.pageTable[page].frame = idx;
        // update replacers
        fifo.touch(idx);
        lru.touch(idx);
        cout << "Loaded P" << pid << ":p" << page << " -> frame " << idx << "\n";
    }

    vector<string> getFrameState() const {
        vector<string> out;
        for (int i = 0; i < (int)frames.size(); ++i) {
            if (frames[i].free) out.push_back("free");
            else {
                ostringstream ss; ss << "P" << frames[i].pid << ",p" << frames[i].page;
                out.push_back(ss.str());
            }
        }
        return out;
    }

    void printFrames() const {
        cout << "Frames: ";
        for (int i = 0; i < (int)frames.size(); ++i) {
            if (frames[i].free) cout << "["<<i<<": free] ";
            else cout << "["<<i<<": P"<<frames[i].pid<<",p"<<frames[i].page<<"] ";
        }
        cout << "\n";
    }

    void printPageTables() const {
        for (auto &pr : procs) {
            cout << "P" << pr.first << " page present bits: ";
            int limit = min((int)pr.second.pageTable.size(), 16);
            for (int i = 0; i < limit; ++i) cout << (pr.second.pageTable[i].present ? '1' : '0');
            cout << "\n";
        }
    }

    void printSummary() const {
        cout << "\n=== SUMMARY ===\n";
        cout << "Frames: " << frames.size() << "\n";
        cout << "Page Hits: " << hits << "\n";
        cout << "Page Faults: " << faults << "\n";
    }
};

/* -----------------------
   Segmentation manager
   ----------------------- */
struct Segment {
    int base = -1;
    int limit = 0;
};

struct SegmentationManager {
    int memorySize = 1024;      // total physical units
    int nextFree = 0;           // first free physical address
    unordered_map<int, vector<Segment>> tables; // pid -> segments

    SegmentationManager(int mem=1024) : memorySize(mem), nextFree(0) {}

    bool allocateSegment(int pid, int segId, int size) {
        if (size <= 0) { cout << "Invalid segment size\n"; return false; }
        if (nextFree + size > memorySize) {
            cout << "NOT_ENOUGH_MEMORY for P" << pid << " seg " << segId << "\n";
            return false;
        }
        auto &tab = tables[pid];
        if ((int)tab.size() <= segId) tab.resize(segId+1);
        tab[segId].base = nextFree;
        tab[segId].limit = size;
        nextFree += size;
        cout << "ALLOC: P" << pid << " S" << segId << " base=" << tab[segId].base << " limit=" << size << "\n";
        return true;
    }

    void access(int pid, int segId, int offset) {
        if (!tables.count(pid)) {
            cout << "P" << pid << " has no segments\n"; return;
        }
        auto &tab = tables[pid];
        if (segId < 0 || segId >= (int)tab.size()) { cout << "Invalid segment\n"; return; }
        auto &s = tab[segId];
        if (s.base < 0) { cout << "Segment not allocated\n"; return; }
        if (offset < 0 || offset >= s.limit) {
            cout << "SEGMENTATION FAULT: P" << pid << " S" << segId << " offset=" << offset << " limit=" << s.limit << "\n";
            return;
        }
        int phys = s.base + offset;
        cout << "ACCESS: P" << pid << " S" << segId << "("<<offset<<") -> PA=" << phys << "\n";
    }

    void printAll() const {
        cout << "\n=== SEGMENT TABLES ===\n";
        for (auto &pr : tables) {
            cout << "P" << pr.first << ":\n";
            for (int i = 0; i < (int)pr.second.size(); ++i) {
                auto &s = pr.second[i];
                if (s.base >= 0) cout << "  S"<<i<<" base="<<s.base<<" limit="<<s.limit<<"\n";
            }
        }
    }
};

/* -----------------------
   Virtual memory manager (uses MemoryManager)
   ----------------------- */
struct VirtualMemoryManager {
    int pageSize = 1024;
    MemoryManager mem; // uses paging internals
    unordered_map<int, unordered_set<int>> swap; // simulate swapped-out pages pid->set(page)
    int pageIns = 0;

    VirtualMemoryManager(int frames=4, string algo="FIFO") : mem(frames, algo) {}

    void accessVirtual(int pid, int va) {
        if (va < 0) { cout << "Invalid VA\n"; return; }
        int page = va / pageSize;
        int offset = va % pageSize;
        cout << "VACCESS: P" << pid << " VA=" << va << " -> page="<<page<<" off="<<offset<<"\n";
        // if page is in swap (simulated), we "page-in"
        if (swap[pid].count(page)) {
            cout << "PAGE-IN: bringing page " << page << " of P" << pid << " from swap\n";
            swap[pid].erase(page);
            pageIns++;
        }
        // now use memory manager to access page
        mem.accessPage(pid, page);
        // on eviction, we don't simulate writing to swap automatically here (simple)
    }

    vector<string> getFrameState() const { return mem.getFrameState(); }
    void printSummary() const {
        mem.printSummary();
        cout << "Simulated page-ins: " << pageIns << "\n";
    }
};

/* -----------------------
   Command-line / main
   ----------------------- */

void printUsage(const string &prog) {
    cout << "Usage:\n";
    cout << "  " << prog << " <tracefile> <FIFO|LRU|-> <PAGING|SEGMENTATION|VIRTUAL>\n";
    cout << "Examples:\n";
    cout << "  " << prog << " traces_paging.txt FIFO PAGING\n";
    cout << "  " << prog << " traces_segment.txt - SEGMENTATION\n";
    cout << "  " << prog << " traces_vm.txt LRU VIRTUAL\n";
    cout << "\nTrace formats:\n";
    cout << "  PAGING: lines 'pid page' (numbers)\n";
    cout << "  SEGMENTATION: lines 'ALLOC pid seg size' or 'ACCESS pid seg offset'\n";
    cout << "  VIRTUAL: lines 'pid virtualAddress' (numbers)\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    string tracefile = argv[1];
    string algo = argv[2];
    string mode = argv[3];

    auto trace = loadTrace(tracefile);
    if (trace.empty()) {
        cout << "No trace entries loaded (or file missing).\n";
        return 1;
    }

    if (mode == "PAGING") {
        cout << "Running PAGING mode, algorithm=" << algo << "\n";
        MemoryManager mm(4, algo);
        for (size_t i = 0; i < trace.size(); ++i) {
            auto &e = trace[i];
            // in paging mode we use e.page (loader stored numeric second token in page)
            cout << "\nStep " << i << ": P" << e.pid << " page " << e.page << "\n";
            mm.accessPage(e.pid, e.page);
            mm.printFrames();
        }
        mm.printSummary();
    }
    else if (mode == "SEGMENTATION") {
        cout << "Running SEGMENTATION mode\n";
        SegmentationManager sm(2048);
        for (size_t i = 0; i < trace.size(); ++i) {
            auto &e = trace[i];
            if (e.type == 1) { // ALLOC
                sm.allocateSegment(e.pid, e.segId, e.size);
            } else if (e.type == 2) { // ACCESS
                sm.access(e.pid, e.segId, e.offset);
            } else {
                // The trace loader treats "pid number" as page/va -- ignore in segmentation unless it's an explicit ACCESS
                cout << "Skipping entry (not a segmentation command): pid="<<e.pid<<"\n";
            }
        }
        sm.printAll();
    }
    else if (mode == "VIRTUAL") {
        cout << "Running VIRTUAL mode, algorithm=" << algo << "\n";
        VirtualMemoryManager vm(4, algo);
        for (size_t i = 0; i < trace.size(); ++i) {
            auto &e = trace[i];
            cout << "\nStep " << i << ": P" << e.pid << " VA " << e.virtualAddress << "\n";
            vm.accessVirtual(e.pid, e.virtualAddress);
            // print frames
            auto frames = vm.getFrameState();
            cout << "Frames: ";
            for (int j=0;j<(int)frames.size();++j) cout << "["<<j<<":"<<frames[j]<<"] ";
            cout << "\n";
        }
        vm.printSummary();
    }
    else {
        cout << "Unknown mode: " << mode << "\n";
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
