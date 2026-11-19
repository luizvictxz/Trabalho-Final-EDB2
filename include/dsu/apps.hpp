// Aplicações clássicas de conjuntos disjuntos, genéricas na variante D.
#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace dsu {

using Vertex = std::uint32_t;
using EdgeList = std::vector<std::pair<Vertex, Vertex>>;

struct WEdge {
    Vertex u, v;
    double w;
};

// Componentes conexas de um grafo não direcionado com n vértices.
// Retorna, para cada vértice, o representante do seu componente.
template <class D>
std::vector<Vertex> connected_components(Vertex n, const EdgeList& edges,
                                         Vertex* num_components = nullptr) {
    D d(n);
    for (const auto& [u, v] : edges) d.unite(u, v);
    std::vector<Vertex> label(n);
    for (Vertex v = 0; v < n; ++v) label[v] = d.find(v);
    if (num_components) *num_components = d.num_sets();
    return label;
}

// Árvore (floresta) geradora mínima pelo algoritmo de Kruskal (1956).
// Retorna o peso total; se `tree` != nullptr, devolve as arestas escolhidas.
template <class D>
double kruskal(Vertex n, std::vector<WEdge> edges,
               std::vector<WEdge>* tree = nullptr) {
    std::sort(edges.begin(), edges.end(),
              [](const WEdge& a, const WEdge& b) { return a.w < b.w; });
    D d(n);
    double total = 0.0;
    Vertex used = 0;
    for (const auto& e : edges) {
        if (n > 0 && used == n - 1) break;  // árvore completa
        if (d.unite(e.u, e.v)) {
            total += e.w;
            ++used;
            if (tree) tree->push_back(e);
        }
    }
    return total;
}

}  // namespace dsu
