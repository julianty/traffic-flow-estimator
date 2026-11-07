#include "unionfind.hpp"
#include <vector>

UnionFind::UnionFind(int n) {
    for (int i=0; i<n; i++) parent.push_back(i);
    rank = std::vector<int>(n, 0);
}

int UnionFind::find(int x) {
    if (parent[x] == x) {
        return x;
    }
    int p = find(parent[x]);
    parent[x] = p;
    return p;
}

bool UnionFind::unite(int a, int b) {
    int ra = find(a);
    int rb = find(b);
    // They're already pointing to the same root
    if (ra == rb) return false;
    if (rank[ra] < rank[rb]) parent[ra] = rb;
    else if (rank[ra] > rank[rb]) parent[rb] = ra;
    else {
        parent[rb] = ra;
        rank[ra] += 1;
    }
    return true;
}

int UnionFind::size() const {
    return rank.size();
}

bool UnionFind::connected(int a, int b) {
    return find(a) == find(b);
}