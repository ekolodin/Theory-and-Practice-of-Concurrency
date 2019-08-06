#include <iostream>
#include <vector>
#include <cmath>
#include <thread>

using namespace std;

typedef unsigned int ui;

class PetersonLock {
public:
    explicit PetersonLock() {
        want[0].store(false);
        want[1].store(false);
    }

    void Lock(bool thread_index) {
        want[thread_index].store(true);
        victim.store(thread_index);
        while (want[1 - thread_index].load() && victim.load() == thread_index) {
            std::this_thread::yield();
        }
    }

    void Unlock(int thread_index) {
        want[thread_index].store(false);
    }

private:
    std::atomic<int> victim{};
    std::atomic<bool> want[2]{};
};

class TournamentTreeLock {
public:
    explicit TournamentTreeLock(int num_threads) : tree(std::vector<PetersonLock>(GetTheRightSize(num_threads))) {}

    void Lock(int thread_index) {
        int current = GetThreadLeaf(thread_index);
        while (GetParent(current) != -1) {
            tree[GetParent(current)].Lock((current + 1) % 2);
            current = GetParent(current);
        }
    }

    void Unlock(int thread_index) {
        RecursiveUnlock(GetThreadLeaf(thread_index));
    }

private:
    ui GetTheRightSize(int num_threads) {
        if (num_threads <= 2) {
            return 1;
        }
        ui power_of_two = 1, answer = 1;
        while (1 << power_of_two < num_threads) {
            answer += 1 << power_of_two;
            ++power_of_two;
        }
        return answer;
    }

    int GetParent(int index) {
        return (index == 0 ? -1 : (index - 1) / 2);
    }

    void RecursiveUnlock(int petersonIndex) {
        if (GetParent(petersonIndex) == -1) {
            return;
        }
        RecursiveUnlock(GetParent(petersonIndex));
        tree[GetParent(petersonIndex)].Unlock((petersonIndex + 1) % 2);
    }

    ui GetThreadLeaf(int thread_index) {
        return tree.size() + thread_index;
    }

private:
    std::vector<PetersonLock> tree;
};

int main() {
    TournamentTreeLock tree(2);
    tree.Lock(0);
    tree.Unlock(0);
    tree.Lock(1);
    tree.Unlock(1);
    std::cout << "finished\n";

    return 0;
}