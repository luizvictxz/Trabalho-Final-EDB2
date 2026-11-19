// Contadores de instrumentação das operações de conjuntos disjuntos.
//
// Só são incrementados quando a estrutura é instanciada com Count = true
// (via `if constexpr`). Com Count = false o compilador elimina todo o
// código de contagem, de modo que as medições de tempo não são afetadas.
#pragma once

#include <cstdint>

namespace dsu {

struct Counters {
    // Chamadas a find(), incluindo as duas feitas internamente por unite().
    std::uint64_t finds = 0;
    // Chamadas a unite().
    std::uint64_t unions = 0;
    // Uniões efetivas (os dois elementos estavam em conjuntos distintos).
    std::uint64_t links = 0;
    // Arestas percorridas nos caminhos de busca (soma das profundidades
    // dos elementos consultados). Nas listas ligadas, cada find custa 1.
    std::uint64_t hops = 0;
    // Ponteiros reescritos: na floresta, pela compressão/halving/splitting;
    // nas listas, pelo reapontamento dos elementos da lista anexada.
    std::uint64_t writes = 0;

    void reset() { *this = Counters{}; }
};

}  // namespace dsu
