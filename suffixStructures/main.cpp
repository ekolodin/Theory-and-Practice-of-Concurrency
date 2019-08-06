#include <vector>
#include <iostream>
#include <deque>
#include <map>

typedef unsigned long long ull;
typedef unsigned int ui;

struct Node {

    ui link{};
    ull length{};
    std::vector<ui> to;

    explicit Node(ui alphabetSize) {
        to.resize(alphabetSize);
    }
};

class SuffixAutomaton {

private:
    const ui maxN = 300005;
    std::vector<Node> states;
    ui last;
    ui size;
    ui alphabetSize;

public:
    explicit SuffixAutomaton(ui alphabetSize) : last(0), size(1), alphabetSize(alphabetSize) {
        states.resize(maxN, Node(alphabetSize));
    }

    ui getSize() { return size; }

    std::vector<ull> getLen() {
        std::vector<ull> length;
        for (auto &state : states) {
            length.push_back(state.length);
        }
        return length;
    }

    std::vector<ui> getLink() {
        std::vector<ui> links;
        for (auto &state : states) {
            links.push_back(state.link);
        }
        return links;
    }

    void addLetter(int letter, std::vector<ull> &cnt);

    bool dfs(int max, std::deque<ui> &res, int state = 0, int length = 0);
};


void SuffixAutomaton::addLetter(int letter, std::vector<ull> &cnt) {
    ui previous = last;
    last = size++;
    cnt[last] = 1;

    states[last].length = states[previous].length + 1;

    for (; states[previous].to[letter] == 0; previous = states[previous].link) {
        states[previous].to[letter] = last;
    }
    if (states[previous].to[letter] == last) {
        states[last].link = 0;
        return;
    }
    ui q = states[previous].to[letter];
    if (states[q].length == states[previous].length + 1) {
        states[last].link = q;
        return;
    }
    ui clone = size++;
    states[clone].to = states[q].to;
    states[clone].link = states[q].link;
    states[clone].length = states[previous].length + 1;
    states[last].link = states[q].link = clone;
    for (; states[previous].to[letter] == q; previous = states[previous].link) {
        states[previous].to[letter] = clone;
    }

}

bool SuffixAutomaton::dfs(int max, std::deque<ui> &res, int state, int length) {
    if (state == max && states[max].length == length) {
        return true;
    }

    for (size_t i = 0; i < alphabetSize; ++i) {
        if (states[state].to[i] != 0 && states[states[state].to[i]].length == states[state].length + 1 &&
            dfs(max, res, states[state].to[i], length + 1)) {
            res.push_front(static_cast<ui &&>(i));
            return true;
        }
    }

    return false;
}

struct Refren {

    ull value, length, position;

    Refren() : value(0), length(0), position(0) {}
};

struct Link {

    int start{};
    int end{};
    int to;

    Link() : to(-1) {}

    Link(int start, int end, int to) : start(start), end(end), to(to) {}
};

struct Vertex {

    std::vector<Link> links;
    int suffix;

    explicit Vertex(ui alphabetSize) {
        links.resize(alphabetSize);
        suffix = -1;
    }
};

class SuffixTree {

private:
    const ui inf = static_cast<const ui>(1e9);

    std::vector<Vertex> tree;
    ui root;
    ui dummy;
    ui alphabetSize;
    std::vector<int> sample;

    int getTheCharacter(int i) {
        return (i < 0) ? (-i - 1) : sample[i];
    }

    ui newVertex() {
        auto i = static_cast<ui>(tree.size());
        tree.emplace_back(alphabetSize);
        return i;
    }

    void link(int from, int start, int end, int to) {
        tree[from].links[getTheCharacter(start)] = Link(start, end, to);
    }

    int &getTheSufLink(int vertex) {
        return tree[vertex].suffix;
    }

    std::pair<int, int> canonize(int vertex, int start, int end);

    std::pair<int, int> update(int vertex, int start, int end);

    std::pair<bool, int> testAndSplit(int vertex, int start, int end, int letter);

public:

    explicit SuffixTree(ui alphabetSize, const std::vector<int> &sample) : alphabetSize(alphabetSize), sample(sample) {
        tree.clear();
        dummy = newVertex();
        root = newVertex();

        getTheSufLink(root) = dummy;
        for (int i = 0; i < alphabetSize; ++i) {
            link(dummy, -i - 1, -i, root);
        }

        std::pair<int, int> activePoint = std::make_pair(root, 0);
        for (int i = 0; i < sample.size(); ++i) {
            activePoint = update(activePoint.first, activePoint.second, i);
            activePoint = canonize(activePoint.first, activePoint.second, i + 1);
        }
    }

    ui dfs(int vertex, ui length, Refren &refren);
};

std::pair<int, int> SuffixTree::canonize(int vertex, int start, int end) {
    if (end <= start) {
        return std::make_pair(vertex, start);
    } else {
        Link cur = tree[vertex].links[getTheCharacter(start)];
        while (end - start >= cur.end - cur.start) {
            start += cur.end - cur.start;
            vertex = cur.to;
            if (end > start) {
                cur = tree[vertex].links[getTheCharacter(start)];
            }
        }
        return std::make_pair(vertex, start);
    }
}

std::pair<bool, int> SuffixTree::testAndSplit(int vertex, int start, int end, int letter) {
    if (end <= start) {
        return std::make_pair(tree[vertex].links[letter].to != -1, vertex);
    } else {
        Link cur = tree[vertex].links[getTheCharacter(start)];
        if (letter == getTheCharacter(cur.start + end - start)) {
            return std::make_pair(true, vertex);
        }

        int middle = newVertex();
        link(vertex, cur.start, cur.start + end - start, middle);
        link(middle, cur.start + end - start, cur.end, cur.to);
        return std::make_pair(false, middle);
    }
}

std::pair<int, int> SuffixTree::update(int vertex, int start, int end) {
    std::pair<bool, int> splitRes;
    int oldR = root;

    splitRes = testAndSplit(vertex, start, end, getTheCharacter(end));
    while (!splitRes.first) {
        link(splitRes.second, end, inf, newVertex());

        if (oldR != root) {
            getTheSufLink(oldR) = splitRes.second;
        }
        oldR = splitRes.second;

        std::pair<int, int> newPoint = canonize(getTheSufLink(vertex), start, end);
        vertex = newPoint.first;
        start = newPoint.second;
        splitRes = testAndSplit(vertex, start, end, getTheCharacter(end));
    }
    if (oldR != root) {
        getTheSufLink(oldR) = splitRes.second;
    }
    return std::make_pair(vertex, start);
}

ui SuffixTree::dfs(int vertex, ui length, Refren &refren) {
    if (tree[vertex].suffix == -1) {
        return 1;
    }
    ui countLeaves = 0;
    for (auto &i : tree[vertex].links) {
        if (i.to != -1) {
            ui leaves = dfs(i.to, length + i.end - i.start, refren);
            countLeaves += leaves;

            ull refrenLength = length + (i.end == inf ? sample.size() : static_cast<ull>(i.end)) - i.start;
            ull refrenPosition = (i.end == inf ? sample.size() : static_cast<ull>(i.end)) - refrenLength;
            if (leaves == 1) {
                --refrenLength;
            }
            if (refrenLength * leaves > refren.value) {
                refren.value = refrenLength * leaves;
                refren.length = refrenLength;
                refren.position = refrenPosition;
            }
        }
    }
    return countLeaves;
}

void automatonSolve(SuffixAutomaton &suffixAutomaton, std::vector<ull> &cnt) {
    ui max = 0;
    std::vector<ui> id(suffixAutomaton.getSize());
    std::vector<ull> length = suffixAutomaton.getLen();
    std::vector<ui> link = suffixAutomaton.getLink();
    for (ui i = 0; i < id.size(); ++i) {
        id[i] = i;
    }

    sort(id.begin(), id.end(), [&length](int a, int b) -> bool {
        if (length[a] == length[b])
            return a > b;
        return length[a] > length[b];
    });
    for (size_t i = 0; id[i] != 0; ++i) {
        cnt[link[id[i]]] += cnt[id[i]];
    }
    for (size_t i = 0; id[i] != 0; ++i) {
        if (cnt[id[i]] * length[id[i]] > cnt[max] * length[max]) {
            max = id[i];
        }
    }

    std::deque<ui> answer;
    std::cout << length[max] * cnt[max] << '\n' << length[max] << '\n';
    suffixAutomaton.dfs(max, answer);
    for (auto i : answer) {
        std::cout << i << ' ';
    }
}

void treeSolve(SuffixTree &suffixTree, const std::vector<int> &input) {
    Refren refren;
    suffixTree.dfs(1, 0, refren);
    std::cout << refren.value << '\n' << refren.length << '\n';
    for (ull i = refren.position; i < (refren.position + refren.length); ++i) {
        std::cout << input[i] << ' ';
    }
}

int main() {
    ui n, m, temp;
    std::cin >> n >> m;

    SuffixAutomaton suffixAutomaton(m + 1);
    std::vector<ull> cnt(300005, 0);
    std::vector<int> input;
    for (int i = 0; i < n; ++i) {
        std::cin >> temp;
        suffixAutomaton.addLetter(temp, cnt);
        //input.push_back(temp);
    }
    automatonSolve(suffixAutomaton, cnt);

//    input.push_back(m + 1);
//    SuffixTree suffixTree(m + 2, input);
//    treeSolve(suffixTree, input);

    return 0;
}
