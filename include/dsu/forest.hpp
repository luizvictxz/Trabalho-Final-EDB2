// Conjuntos disjuntos representados por florestas (Galler e Fischer, 1964).
//
// Cada conjunto é uma árvore enraizada em que cada nó aponta para o pai; a
// raiz é o representante e aponta para si mesma. A estrutura é
// parametrizada por duas políticas independentes:
//
//   Política de ligação (Link)       Política de busca (Find)
//   ------------------------------   ----------------------------------------
//   LinkNaive : raiz de x vira       FindNaive     : sobe até a raiz, sem
//               filha da raiz de y                   alterar a árvore
//   LinkByRank: união pelo posto     FindCompress  : compressão de caminho
//   LinkBySize: união pelo tamanho                   (duas passadas)
//                                    FindHalving   : path halving
//                                    FindSplitting : path splitting
//
// LinkByRank + FindCompress é a versão otimizada clássica, com custo
// amortizado O(α(n)) por operação (Tarjan, 1975). Halving e splitting
// mantêm o mesmo limite com uma única passada (Tarjan e van Leeuwen, 1984).
//
// A busca é iterativa, para não estourar a pilha em árvores degeneradas
// (a ligação ingênua pode produzir caminhos de comprimento n-1).
#pragma once

#include <cstdint>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "counters.hpp"

namespace dsu {

struct LinkNaive {};
struct LinkByRank {};
struct LinkBySize {};

struct FindNaive {};
struct FindCompress {};
struct FindHalving {};
struct FindSplitting {};

template <class Link, class Find, bool Count = false>
class ForestDSU {
    static_assert(std::is_same_v<Link, LinkNaive> ||
                      std::is_same_v<Link, LinkByRank> ||
                      std::is_same_v<Link, LinkBySize>,
                  "politica de ligacao invalida");
    static_assert(std::is_same_v<Find, FindNaive> ||
                      std::is_same_v<Find, FindCompress> ||
                      std::is_same_v<Find, FindHalving> ||
                      std::is_same_v<Find, FindSplitting>,
                  "politica de busca invalida");

    static constexpr bool kRank = std::is_same_v<Link, LinkByRank>;
    static constexpr bool kSize = std::is_same_v<Link, LinkBySize>;

public:
    using index_t = std::uint32_t;
    // O posto de qualquer nó é no máximo floor(lg n) <= 31, então 1 byte basta.
    using rank_t = std::uint8_t;

    // Make-Set para os elementos 0..n-1.
    explicit ForestDSU(index_t n) : parent_(n), sets_(n) {
        std::iota(parent_.begin(), parent_.end(), index_t{0});
        if constexpr (kRank) rank_.assign(n, 0);
        if constexpr (kSize) size_.assign(n, 1);
    }

    // Raiz da árvore que contém x.
    index_t find(index_t x) {
        if constexpr (Count) ++c_.finds;

        if constexpr (std::is_same_v<Find, FindNaive>) {
            while (parent_[x] != x) {
                x = parent_[x];
                if constexpr (Count) ++c_.hops;
            }
            return x;

        } else if constexpr (std::is_same_v<Find, FindCompress>) {
            // 1ª passada: encontra a raiz.
            index_t r = x;
            while (parent_[r] != r) {
                r = parent_[r];
                if constexpr (Count) ++c_.hops;
            }
            // 2ª passada: faz todo nó do caminho apontar direto para a raiz.
            while (x != r) {
                index_t p = parent_[x];
                parent_[x] = r;
                if constexpr (Count) {
                    if (p != r) ++c_.writes;
                }
                x = p;
            }
            return r;

        } else if constexpr (std::is_same_v<Find, FindHalving>) {
            // Cada nó visitado passa a apontar para o avô; o percurso
            // salta de dois em dois, então só metade do caminho é visitada.
            while (parent_[x] != x) {
                index_t p = parent_[x];
                index_t g = parent_[p];
                parent_[x] = g;
                if constexpr (Count) {
                    c_.hops += (p == g) ? 1 : 2;
                    if (p != g) ++c_.writes;
                }
                x = g;
            }
            return x;

        } else {  // FindSplitting
            // Todo nó do caminho passa a apontar para o avô.
            while (parent_[x] != x) {
                index_t p = parent_[x];
                index_t g = parent_[p];
                parent_[x] = g;
                if constexpr (Count) {
                    ++c_.hops;
                    if (p != g) ++c_.writes;
                }
                x = p;
            }
            return x;
        }
    }

    // Une os conjuntos de x e y. Retorna false se já estavam juntos.
    bool unite(index_t x, index_t y) {
        if constexpr (Count) ++c_.unions;
        index_t a = find(x);
        index_t b = find(y);
        if (a == b) return false;
        link(a, b);
        --sets_;
        if constexpr (Count) ++c_.links;
        return true;
    }

    bool same(index_t x, index_t y) { return find(x) == find(y); }

    index_t num_elements() const { return static_cast<index_t>(parent_.size()); }
    index_t num_sets() const { return sets_; }

    // Maior profundidade (em arestas) entre todos os nós, sem modificar a
    // floresta. O(n) com memoização.
    index_t max_depth() const {
        const index_t n = num_elements();
        std::vector<index_t> depth(n, NIL);
        std::vector<index_t> stack;
        index_t best = 0;
        for (index_t v = 0; v < n; ++v) {
            index_t u = v;
            while (depth[u] == NIL && parent_[u] != u) {
                stack.push_back(u);
                u = parent_[u];
            }
            if (depth[u] == NIL) depth[u] = 0;  // u é raiz
            index_t d = depth[u];
            while (!stack.empty()) {
                depth[stack.back()] = ++d;
                stack.pop_back();
            }
            if (depth[v] > best) best = depth[v];
        }
        return best;
    }

    std::size_t memory_bytes() const {
        return sizeof(index_t) * (parent_.capacity() + size_.capacity()) +
               sizeof(rank_t) * rank_.capacity();
    }

    // Acesso somente leitura para testes de invariantes.
    const std::vector<index_t>& parents() const { return parent_; }
    const std::vector<rank_t>& ranks() const { return rank_; }
    const std::vector<index_t>& sizes() const { return size_; }

    const Counters& counters() const { return c_; }
    void reset_counters() { c_.reset(); }

private:
    static constexpr index_t NIL = static_cast<index_t>(-1);

    // Liga duas raízes distintas a e b.
    void link(index_t a, index_t b) {
        if constexpr (std::is_same_v<Link, LinkNaive>) {
            parent_[a] = b;
        } else if constexpr (kRank) {
            if (rank_[a] < rank_[b]) {
                parent_[a] = b;
            } else if (rank_[a] > rank_[b]) {
                parent_[b] = a;
            } else {
                parent_[b] = a;
                ++rank_[a];
            }
        } else {  // LinkBySize
            if (size_[a] < size_[b]) std::swap(a, b);
            parent_[b] = a;
            size_[a] += size_[b];
        }
    }

    std::vector<index_t> parent_;
    std::vector<rank_t> rank_;    // vazio se Link != LinkByRank
    std::vector<index_t> size_;   // vazio se Link != LinkBySize
    index_t sets_;
    Counters c_{};
};

}  // namespace dsu
