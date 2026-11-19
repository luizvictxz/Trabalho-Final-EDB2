// Cabeçalho único: inclui as duas representações e define as variantes
// avaliadas no trabalho, com nomes curtos usados pelo benchmark e nos testes.
#pragma once

#include "counters.hpp"
#include "forest.hpp"
#include "linked_list.hpp"

namespace dsu {

// ---- Listas ligadas -------------------------------------------------------
template <bool C = false> using ListNaive    = LinkedListDSU<false, C>;
template <bool C = false> using ListWeighted = LinkedListDSU<true, C>;

// ---- Florestas ------------------------------------------------------------
template <bool C = false> using ForestNaive    = ForestDSU<LinkNaive,  FindNaive,    C>;
template <bool C = false> using ForestRank     = ForestDSU<LinkByRank, FindNaive,    C>;
template <bool C = false> using ForestSize     = ForestDSU<LinkBySize, FindNaive,    C>;
template <bool C = false> using ForestCompress = ForestDSU<LinkNaive,  FindCompress, C>;
// Versão otimizada principal: união pelo posto + compressão de caminho.
template <bool C = false> using ForestRankCompress = ForestDSU<LinkByRank, FindCompress,  C>;
template <bool C = false> using ForestSizeCompress = ForestDSU<LinkBySize, FindCompress,  C>;
template <bool C = false> using ForestRankHalving  = ForestDSU<LinkByRank, FindHalving,   C>;
template <bool C = false> using ForestRankSplitting= ForestDSU<LinkByRank, FindSplitting, C>;

template <class T> struct Tag { using type = T; };

// Chama f(nome, Tag<versão sem contadores>, Tag<versão com contadores>)
// para cada variante, na ordem em que aparecem nas tabelas do artigo.
template <class F>
void for_each_variant(F&& f) {
    f("ll-naive",         Tag<ListNaive<false>>{},           Tag<ListNaive<true>>{});
    f("ll-weighted",      Tag<ListWeighted<false>>{},        Tag<ListWeighted<true>>{});
    f("f-naive",          Tag<ForestNaive<false>>{},         Tag<ForestNaive<true>>{});
    f("f-rank",           Tag<ForestRank<false>>{},          Tag<ForestRank<true>>{});
    f("f-size",           Tag<ForestSize<false>>{},          Tag<ForestSize<true>>{});
    f("f-compress",       Tag<ForestCompress<false>>{},      Tag<ForestCompress<true>>{});
    f("f-rank-compress",  Tag<ForestRankCompress<false>>{},  Tag<ForestRankCompress<true>>{});
    f("f-size-compress",  Tag<ForestSizeCompress<false>>{},  Tag<ForestSizeCompress<true>>{});
    f("f-rank-halving",   Tag<ForestRankHalving<false>>{},   Tag<ForestRankHalving<true>>{});
    f("f-rank-splitting", Tag<ForestRankSplitting<false>>{}, Tag<ForestRankSplitting<true>>{});
}

}  // namespace dsu
