// Checks UnionFind against a brute-force reference on random unite() sequences.
// Reference: group[i] = group label of item i; merging relabels every member of
// one group (O(n) per merge, obviously correct).
// After each unite():
//   - unite()'s return value == "the reference groups were different"
//   - number of groups == n minus the number of unite() calls that returned true
//   - connected(a, b) matches the reference for every pair (small n),
//     or for a random sample of pairs plus every pair at the end (larger n)
//   - find(x) is a member of x's group, and find(find(x)) == find(x)
// Plus the hand case where comparing parents instead of roots goes wrong.
// Build: make build/unionfind_check.exe   (no OpenCV needed)
#include "../src/unionfind.hpp"
#include <cstdio>
#include <random>
#include <vector>

namespace {
    int failures = 0;

    void fail(const char* what, int n, int trial) {
        if (failures < 20) std::printf("FAIL: %s (n=%d, trial=%d)\n", what, n, trial);
        failures++;
    }

    // Brute-force reference.
    struct RefGroups {
        std::vector<int> group;
        int groups;

        explicit RefGroups(int n) : group(n), groups(n) {
            for (int i = 0; i < n; i++) group[i] = i;
        }

        // Returns true if a and b were in different groups (a merge happened).
        bool unite(int a, int b) {
            int ga = group[a], gb = group[b];
            if (ga == gb) return false;
            for (int& g : group) if (g == gb) g = ga;
            groups--;
            return true;
        }
    };

    // Checks every pair against the reference, plus the find() properties.
    void checkAllPairs(UnionFind& uf, const RefGroups& ref, int n, int trial) {
        for (int x = 0; x < n; x++) {
            int r = uf.find(x);
            if (r < 0 || r >= n) { fail("find out of range", n, trial); return; }
            if (ref.group[r] != ref.group[x]) fail("find(x) not in x's group", n, trial);
            if (uf.find(r) != r) fail("find(find(x)) != find(x)", n, trial);
        }
        for (int a = 0; a < n; a++)
            for (int b = a; b < n; b++)
                if (uf.connected(a, b) != (ref.group[a] == ref.group[b]))
                    fail("connected vs reference (all pairs)", n, trial);
    }

    // The case from the review: 1 and 3 share a root but not a parent.
    void handCase() {
        UnionFind uf(4);
        uf.unite(0, 1);
        uf.unite(2, 3);
        uf.unite(0, 2);
        if (!uf.connected(1, 3)) fail("hand case: connected(1, 3) after deep merge", 4, -1);
        if (!uf.connected(3, 1)) fail("hand case: connected(3, 1) after deep merge", 4, -1);
        if (uf.unite(1, 3)) fail("hand case: unite on same group returned true", 4, -1);
        if (uf.size() != 4) fail("hand case: size", 4, -1);
    }
}

int main() {
    handCase();

    std::mt19937 rng(187);
    const int maxN = 200;
    const int trialsPerN = 20;
    const int allPairsEveryOpUpTo = 30;  // above this, sample pairs per op, all pairs at the end
    const int sampledPairsPerOp = 50;
    long long unites = 0;

    // n = 0: an empty structure must be valid.
    {
        UnionFind uf(0);
        if (uf.size() != 0) fail("size of empty", 0, 0);
    }

    for (int n = 1; n <= maxN; n++) {
        std::uniform_int_distribution<int> item(0, n - 1);
        for (int t = 0; t < trialsPerN; t++) {
            UnionFind uf(n);
            RefGroups ref(n);
            if (uf.size() != n) fail("size after construction", n, t);

            // Every item starts alone.
            for (int x = 0; x < n; x++)
                if (uf.find(x) != x) fail("find(x) != x before any unite", n, t);

            // 2n random unites (a == b allowed), so most trials end in one big
            // group and many calls hit an already-merged pair.
            int merges = 0;
            for (int op = 0; op < 2 * n; op++) {
                int a = item(rng), b = item(rng);
                bool expected = ref.unite(a, b);
                bool got = uf.unite(a, b);
                if (got != expected) fail("unite return value", n, t);
                if (got) merges++;
                if (n - merges != ref.groups) fail("group count", n, t);
                unites++;

                if (n <= allPairsEveryOpUpTo) {
                    checkAllPairs(uf, ref, n, t);
                } else {
                    for (int s = 0; s < sampledPairsPerOp; s++) {
                        int p = item(rng), q = item(rng);
                        if (uf.connected(p, q) != (ref.group[p] == ref.group[q]))
                            fail("connected vs reference (sampled)", n, t);
                    }
                }
            }
            checkAllPairs(uf, ref, n, t);
            if (uf.size() != n) fail("size at end", n, t);
        }
    }

    std::printf("%lld unites checked, %d failures\n", unites, failures);
    return failures == 0 ? 0 : 1;
}
