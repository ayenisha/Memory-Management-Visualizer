#pragma once
#include <queue>

class FIFOReplacer {
    std::queue<int> q;
public:
    void push(int frame) {
        q.push(frame);
    }
    int evict() {
        int f = q.front();
        q.pop();
        return f;
    }
};
