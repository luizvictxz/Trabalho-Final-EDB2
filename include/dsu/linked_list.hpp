// Conjuntos disjuntos representados por listas ligadas
// (Cormen et al., Cap. "Data Structures for Disjoint Sets", seção de
// representação por listas ligadas).
//
// Cada conjunto é uma lista simplesmente encadeada. O representante é o
// primeiro elemento (cabeça). Todo elemento guarda um ponteiro direto para
// o representante, então find() é O(1). A união anexa uma lista ao final
// da outra e reaponta o representante de todos os elementos anexados.
//
//   Weighted = false  -> anexa sempre a lista de y à de x (versão ingênua);
//                        m operações com n make-set custam O(m + n²), e
//                        há sequências de 2n-1 operações que custam Θ(n²).
//   Weighted = true   -> heurística da união ponderada: anexa a lista menor
//                        à maior; m operações com n make-set custam
//                        O(m + n lg n).
//
// Os "nós" são índices em vetores paralelos (rep_, next_), e não objetos
// alocados individualmente. Isso preserva a semântica de lista ligada
// (cada nó aponta para o próximo) e deixa a comparação com a floresta,
// que também usa vetores, justa em termos de localidade de memória.
#pragma once

#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include "counters.hpp"

namespace dsu {

template <bool Weighted, bool Count = false>
class LinkedListDSU {
public:
    using index_t = std::uint32_t;
    static constexpr index_t NIL = static_cast<index_t>(-1);

    // Make-Set para os elementos 0..n-1.
    explicit LinkedListDSU(index_t n)
        : rep_(n), next_(n, NIL), tail_(n), size_(n, 1), sets_(n) {
        std::iota(rep_.begin(), rep_.end(), index_t{0});
        std::iota(tail_.begin(), tail_.end(), index_t{0});
    }

    // Representante (cabeça da lista) do conjunto que contém x. O(1).
    index_t find(index_t x) {
        if constexpr (Count) {
            ++c_.finds;
            ++c_.hops;
        }
        return rep_[x];
    }

    // Une os conjuntos de x e y. Retorna false se já estavam juntos.
    bool unite(index_t x, index_t y) {
        if constexpr (Count) ++c_.unions;
        index_t a = find(x);
        index_t b = find(y);
        if (a == b) return false;
        if constexpr (Weighted) {
            if (size_[a] < size_[b]) std::swap(a, b);
        }
        // Anexa a lista de b ao final da lista de a, reapontando o
        // representante de cada elemento anexado (custo = |lista de b|).
        for (index_t e = b; e != NIL; e = next_[e]) {
            rep_[e] = a;
            if constexpr (Count) ++c_.writes;
        }
        next_[tail_[a]] = b;
        tail_[a] = tail_[b];
        size_[a] += size_[b];
        --sets_;
        if constexpr (Count) ++c_.links;
        return true;
    }

    bool same(index_t x, index_t y) { return find(x) == find(y); }

    index_t set_size(index_t x) { return size_[find(x)]; }

    // Percorre os elementos do conjunto de x em O(|conjunto|): vantagem
    // da representação por listas que a floresta não oferece diretamente.
    template <class F>
    void for_each_member(index_t x, F&& f) const {
        for (index_t e = rep_[x]; e != NIL; e = next_[e]) f(e);
    }

    index_t num_elements() const { return static_cast<index_t>(rep_.size()); }
    index_t num_sets() const { return sets_; }

    // Custo máximo de um find, em ponteiros seguidos (sempre 1 nas listas).
    index_t max_depth() const { return rep_.empty() ? 0 : 1; }

    std::size_t memory_bytes() const {
        return sizeof(index_t) *
               (rep_.capacity() + next_.capacity() + tail_.capacity() +
                size_.capacity());
    }

    const Counters& counters() const { return c_; }
    void reset_counters() { c_.reset(); }

private:
    std::vector<index_t> rep_;   // representante de cada elemento
    std::vector<index_t> next_;  // próximo elemento na lista (NIL no fim)
    std::vector<index_t> tail_;  // último elemento (válido só na cabeça)
    std::vector<index_t> size_;  // tamanho do conjunto (válido só na cabeça)
    index_t sets_;
    Counters c_{};
};

}  // namespace dsu
