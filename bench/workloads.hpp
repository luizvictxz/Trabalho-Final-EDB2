// Geradores de cargas de trabalho (sequências de operações) para o benchmark.
//
// Toda carga é materializada antes da medição como um vetor de operações,
// de modo que a geração de números aleatórios e a leitura de arquivos não
// entram no tempo medido.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace bench {

using Vertex = std::uint32_t;

struct Op {
    enum Kind : std::uint8_t { Unite = 0, Same = 1 } kind;
    Vertex a, b;
};

struct Workload {
    std::string name;
    Vertex n = 0;
    std::vector<Op> ops;
    std::string note;  // parâmetros relevantes, gravados no CSV
};

struct Params {
    std::string workload = "random";
    Vertex n = 1'000'000;
    std::uint64_t m = 0;          // nº de operações/arestas (0 = padrão da carga)
    std::uint64_t queries = 0;    // consultas same() extras após as uniões
    double union_ratio = 0.5;     // carga random: fração de unite
    double grid_p = 0.5;          // carga grid: probabilidade de manter aresta
    std::string order = "random"; // grid/file: ordem das arestas (random|sequential)
    std::string file;             // carga file: caminho da lista de arestas
    std::uint64_t seed = 1;
};

inline void append_queries(Workload& w, std::uint64_t q, std::mt19937_64& rng) {
    if (w.n == 0) return;
    std::uniform_int_distribution<Vertex> pick(0, w.n - 1);
    for (std::uint64_t i = 0; i < q; ++i)
        w.ops.push_back({Op::Same, pick(rng), pick(rng)});
}

// random: m operações intercaladas; cada uma é unite com prob. union_ratio,
// senão same(); pares uniformes em [0, n).
inline Workload make_random(const Params& p, std::mt19937_64& rng) {
    Workload w{"random", p.n, {}, ""};
    const std::uint64_t m = p.m ? p.m : 2ull * p.n;
    std::uniform_int_distribution<Vertex> pick(0, p.n - 1);
    std::bernoulli_distribution is_union(p.union_ratio);
    w.ops.reserve(m + p.queries);
    for (std::uint64_t i = 0; i < m; ++i)
        w.ops.push_back({is_union(rng) ? Op::Unite : Op::Same, pick(rng), pick(rng)});
    append_queries(w, p.queries, rng);
    w.note = "union_ratio=" + std::to_string(p.union_ratio);
    return w;
}

// ll-adversarial: unite(i, 0), i = 1..n-1. Na lista ingênua, o conjunto
// grande é sempre anexado ao singleton: Θ(n²) reapontamentos.
inline Workload make_list_adversarial(const Params& p, std::mt19937_64& rng) {
    Workload w{"ll-adversarial", p.n, {}, ""};
    w.ops.reserve(p.n + p.queries);
    for (Vertex i = 1; i < p.n; ++i) w.ops.push_back({Op::Unite, i, 0});
    append_queries(w, p.queries, rng);
    return w;
}

// chain: unite(i, i+1), i = 0..n-2, seguido de consultas aleatórias.
// Com ligação ingênua forma um caminho de altura n-1: cada consulta na
// floresta ingênua custa Θ(n).
inline Workload make_chain(const Params& p, std::mt19937_64& rng) {
    Workload w{"chain", p.n, {}, ""};
    const std::uint64_t q = p.queries ? p.queries : p.n;
    w.ops.reserve(p.n + q);
    for (Vertex i = 0; i + 1 < p.n; ++i) w.ops.push_back({Op::Unite, i, i + 1});
    append_queries(w, q, rng);
    return w;
}

// er: grafo aleatório de Erdős–Rényi G(n, m) — componentes conexas.
inline Workload make_er(const Params& p, std::mt19937_64& rng) {
    Workload w{"er", p.n, {}, ""};
    const std::uint64_t m = p.m ? p.m : p.n;  // grau médio 2 por padrão
    std::uniform_int_distribution<Vertex> pick(0, p.n - 1);
    w.ops.reserve(m + p.queries);
    for (std::uint64_t i = 0; i < m; ++i) w.ops.push_back({Op::Unite, pick(rng), pick(rng)});
    append_queries(w, p.queries, rng);
    w.note = "avg_degree=" + std::to_string(2.0 * double(m) / double(p.n));
    return w;
}

// grid: grade k×k (k = floor(sqrt(n))), cada aresta mantida com prob.
// grid_p (percolação). Ordem "sequential" imita rotulação de imagens
// (varredura linha a linha); "random" embaralha as arestas.
inline Workload make_grid(const Params& p, std::mt19937_64& rng) {
    const Vertex k = static_cast<Vertex>(std::sqrt(static_cast<double>(p.n)));
    Workload w{"grid", k * k, {}, ""};
    std::bernoulli_distribution keep(p.grid_p);
    for (Vertex r = 0; r < k; ++r)
        for (Vertex c = 0; c < k; ++c) {
            Vertex v = r * k + c;
            if (c + 1 < k && keep(rng)) w.ops.push_back({Op::Unite, v, v + 1});
            if (r + 1 < k && keep(rng)) w.ops.push_back({Op::Unite, v, v + k});
        }
    if (p.order == "random") std::shuffle(w.ops.begin(), w.ops.end(), rng);
    append_queries(w, p.queries, rng);
    w.note = "k=" + std::to_string(k) + ";p=" + std::to_string(p.grid_p) +
             ";order=" + p.order;
    return w;
}

// file: lista de arestas "u v" por linha (formato SNAP; linhas iniciadas
// por '#' ou '%' são ignoradas). Os rótulos são remapeados para 0..n-1.
inline Workload make_file(const Params& p, std::mt19937_64& rng) {
    std::ifstream in(p.file);
    if (!in) throw std::runtime_error("nao foi possivel abrir " + p.file);
    Workload w{"file", 0, {}, ""};
    std::unordered_map<std::uint64_t, Vertex> id;
    auto get = [&](std::uint64_t x) {
        auto [it, inserted] = id.try_emplace(x, static_cast<Vertex>(id.size()));
        return it->second;
    };
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '%') continue;
        std::istringstream ss(line);
        std::uint64_t u, v;
        if (ss >> u >> v) {
            Vertex a = get(u), b = get(v);
            w.ops.push_back({Op::Unite, a, b});
        }
    }
    w.n = static_cast<Vertex>(id.size());
    if (p.order == "random") std::shuffle(w.ops.begin(), w.ops.end(), rng);
    append_queries(w, p.queries, rng);
    std::string base = p.file.substr(p.file.find_last_of("/\\") + 1);
    w.note = "file=" + base + ";order=" + p.order;
    return w;
}

inline Workload make_workload(const Params& p) {
    std::mt19937_64 rng(p.seed);
    if (p.workload == "random") return make_random(p, rng);
    if (p.workload == "ll-adversarial") return make_list_adversarial(p, rng);
    if (p.workload == "chain") return make_chain(p, rng);
    if (p.workload == "er") return make_er(p, rng);
    if (p.workload == "grid") return make_grid(p, rng);
    if (p.workload == "file") return make_file(p, rng);
    throw std::runtime_error("carga desconhecida: " + p.workload);
}

}  // namespace bench
