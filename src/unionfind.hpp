#pragma once
#include <vector>

class UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> rank;
public:
    explicit UnionFind(int n);
    int find(int x);
    bool unite(int a, int b);
    bool connected(int a, int b);
    int size() const;
};