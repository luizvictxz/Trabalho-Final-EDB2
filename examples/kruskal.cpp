// Exemplo: árvore geradora mínima (Kruskal) com conjuntos disjuntos.
//
// Uso:  ./build/kruskal grafo.txt       (ou lendo da entrada padrão)
// Formato: "u v peso" por linha, vértices numerados de 0 a n-1.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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

    std::vector<dsu::WEdge> edges;
    dsu::Vertex n = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        dsu::WEdge e;
        if (ss >> e.u >> e.v >> e.w) {
            edges.push_back(e);
            n = std::max({n, e.u + 1, e.v + 1});
        }
    }

    std::vector<dsu::WEdge> tree;
    double total = dsu::kruskal<dsu::ForestRankCompress<>>(n, edges, &tree);
    std::printf("arvore geradora minima: %zu arestas, peso total %.4g\n",
                tree.size(), total);
    for (const auto& e : tree) std::printf("  %u - %u  (%.4g)\n", e.u, e.v, e.w);
    return 0;
}
