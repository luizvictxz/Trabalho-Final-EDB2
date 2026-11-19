// Exemplo: componentes conexas de um grafo lido de uma lista de arestas.
//
// Uso:  ./build/componentes grafo.txt        (ou lendo da entrada padrão)
// Formato: uma aresta "u v" por linha; linhas iniciadas por '#' são ignoradas.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "dsu/apps.hpp"
#include "dsu/dsu.hpp"

int main(int argc, char** argv) {
    std::ifstream file;
    if (argc > 1) {
        file.open(argv[1]);
        if (!file) {
            std::fprintf(stderr, "nao foi possivel abrir %s\n", argv[1]);
            return 1;
        }
    }
    std::istream& in = argc > 1 ? static_cast<std::istream&>(file) : std::cin;

    std::unordered_map<long long, dsu::Vertex> id;
    std::vector<long long> original;
    auto get = [&](long long x) {
        auto [it, inserted] = id.try_emplace(x, static_cast<dsu::Vertex>(id.size()));
        if (inserted) original.push_back(x);
        return it->second;
    };
    dsu::EdgeList edges;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        long long u, v;
        if (ss >> u >> v) edges.emplace_back(get(u), get(v));
    }

    const auto n = static_cast<dsu::Vertex>(id.size());
    dsu::Vertex k = 0;
    auto label = dsu::connected_components<dsu::ForestRankCompress<>>(n, edges, &k);

    std::map<dsu::Vertex, std::vector<long long>> groups;
    for (dsu::Vertex v = 0; v < n; ++v) groups[label[v]].push_back(original[v]);

    std::printf("%u vertices, %zu arestas, %u componentes\n", n, edges.size(), k);
    int shown = 0;
    for (auto& [root, members] : groups) {
        std::sort(members.begin(), members.end());
        std::printf("  {");
        for (std::size_t i = 0; i < members.size() && i < 10; ++i)
            std::printf("%s%lld", i ? ", " : "", members[i]);
        std::printf("%s}\n", members.size() > 10 ? ", ..." : "");
        if (++shown == 10) {
            std::printf("  ...\n");
            break;
        }
    }
    return 0;
}
