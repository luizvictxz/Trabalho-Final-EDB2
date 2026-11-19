// Testes de corretude de todas as variantes.
//
//  1. Sequências aleatórias de unite/same comparadas com uma implementação
//     de referência por força bruta (vetor de rótulos).
//  2. Invariantes estruturais (posto, tamanho, profundidade, listas).
//  3. Efeito esperado de compressão, halving e splitting sobre um caminho.
//  4. Contadores: hops == profundidade na busca ingênua.
//  5. Aplicações: componentes conexas (vs. BFS) e Kruskal (vs. Prim).
//
// Compilar e executar:  make test
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "dsu/apps.hpp"
#include "dsu/dsu.hpp"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::fprintf(stderr, "FALHA %s:%d: %s | ", __FILE__, __LINE__, \
                         #cond);                                          \
            std::fprintf(stderr, __VA_ARGS__);                            \
            std::fprintf(stderr, "\n");                                   \
        }                                                                 \
    } while (0)

using dsu::Vertex;

// Referência por força bruta: rótulo explícito por elemento.
struct Reference {
    std::vector<Vertex> label;
    Vertex sets;
    explicit Reference(Vertex n) : label(n), sets(n) {
        for (Vertex i = 0; i < n; ++i) label[i] = i;
    }
    bool unite(Vertex a, Vertex b) {
        Vertex la = label[a], lb = label[b];
        if (la == lb) return false;
        for (auto& l : label)
            if (l == lb) l = la;
        --sets;
        return true;
    }
    bool same(Vertex a, Vertex b) const { return label[a] == label[b]; }
    Vertex size_of(Vertex a) const {
        Vertex s = 0;
        for (auto l : label) s += (l == label[a]);
        return s;
    }
};

template <class T> struct is_list : std::false_type {};
template <bool W, bool C> struct is_list<dsu::LinkedListDSU<W, C>> : std::true_type {};

template <class T> struct forest_traits { static constexpr bool forest = false; };
template <class L, class F, bool C>
struct forest_traits<dsu::ForestDSU<L, F, C>> {
    static constexpr bool forest = true;
    static constexpr bool rank = std::is_same_v<L, dsu::LinkByRank>;
    static constexpr bool size = std::is_same_v<L, dsu::LinkBySize>;
};

// Verifica invariantes estruturais da variante D.
template <class D>
void check_invariants(const std::string& name, D& d, const Reference& ref) {
    const Vertex n = d.num_elements();
    if constexpr (forest_traits<D>::forest) {
        const auto& p = d.parents();
        for (Vertex x = 0; x < n; ++x) {
            CHECK(p[x] < n, "%s: pai fora do intervalo", name.c_str());
        }
        if constexpr (forest_traits<D>::rank) {
            const auto& r = d.ranks();
            const int max_rank = n ? static_cast<int>(std::floor(std::log2(n))) : 0;
            for (Vertex x = 0; x < n; ++x) {
                if (p[x] != x)
                    CHECK(r[p[x]] > r[x], "%s: posto do pai deve ser maior (x=%u)",
                          name.c_str(), x);
                CHECK(r[x] <= max_rank, "%s: posto %d > lg n", name.c_str(), r[x]);
            }
            // Uma árvore cuja raiz tem posto k tem pelo menos 2^k nós.
            for (Vertex x = 0; x < n; ++x)
                if (p[x] == x)
                    CHECK(ref.size_of(x) >= (1u << r[x]),
                          "%s: |arvore| < 2^posto", name.c_str());
            // Profundidade <= floor(lg n) com união pelo posto.
            CHECK(static_cast<int>(d.max_depth()) <= max_rank,
                  "%s: altura %u > lg n", name.c_str(), d.max_depth());
        }
        if constexpr (forest_traits<D>::size) {
            const auto& s = d.sizes();
            for (Vertex x = 0; x < n; ++x)
                if (p[x] == x)
                    CHECK(s[x] == ref.size_of(x), "%s: tamanho incorreto", name.c_str());
        }
    }
    if constexpr (is_list<D>::value) {
        // Cada conjunto, percorrido pela lista, tem o tamanho e os membros
        // certos, e todos apontam para a cabeça.
        for (Vertex x = 0; x < n; ++x) {
            Vertex head = d.find(x);
            Vertex count = 0;
            bool ok = true;
            d.for_each_member(x, [&](Vertex e) {
                ++count;
                ok = ok && ref.same(e, x) && d.find(e) == head;
            });
            CHECK(ok, "%s: membro errado na lista", name.c_str());
            CHECK(count == ref.size_of(x), "%s: tamanho da lista", name.c_str());
            CHECK(d.set_size(x) == count, "%s: set_size", name.c_str());
        }
    }
}

template <class D>
void random_test(const std::string& name, Vertex n, unsigned seed, int ops,
                 double p_union) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Vertex> pick(0, n - 1);
    std::bernoulli_distribution is_union(p_union);
    D d(n);
    Reference ref(n);
    for (int i = 0; i < ops; ++i) {
        Vertex a = pick(rng), b = pick(rng);
        if (is_union(rng)) {
            bool r1 = d.unite(a, b), r2 = ref.unite(a, b);
            CHECK(r1 == r2, "%s n=%u seed=%u op=%d: unite(%u,%u)", name.c_str(),
                  n, seed, i, a, b);
        } else {
            CHECK(d.same(a, b) == ref.same(a, b), "%s n=%u seed=%u op=%d: same",
                  name.c_str(), n, seed, i);
        }
        CHECK(d.num_sets() == ref.sets, "%s: num_sets", name.c_str());
    }
    // Ao final, a partição inteira deve coincidir.
    for (Vertex a = 0; a < n; ++a)
        for (Vertex b = a; b < std::min<Vertex>(n, a + 50); ++b)
            CHECK(d.same(a, b) == ref.same(a, b), "%s: particao final", name.c_str());
    check_invariants(name, d, ref);
}

// Constrói o caminho 0 -> 1 -> ... -> n-1 com ligação ingênua.
template <class D>
D make_path(Vertex n) {
    D d(n);
    for (Vertex i = 0; i + 1 < n; ++i) d.unite(i, i + 1);
    return d;
}

void path_heuristics_test() {
    const Vertex n = 1025;  // caminho com 1024 arestas
    {
        auto d = make_path<dsu::ForestNaive<true>>(n);
        CHECK(d.max_depth() == n - 1, "caminho ingenuo deve ter altura n-1");
        d.reset_counters();
        d.find(0);
        CHECK(d.counters().hops == n - 1, "hops da busca ingenua = profundidade");
        CHECK(d.counters().writes == 0, "busca ingenua nao reescreve ponteiros");
    }
    {
        auto d = make_path<dsu::ForestCompress<true>>(n);
        // Construir com unite(i,i+1) já faz finds em raízes: ainda é caminho.
        CHECK(d.max_depth() == n - 1, "compress: caminho antes da busca");
        d.reset_counters();
        Vertex r = d.find(0);
        CHECK(d.max_depth() == 1, "compressao deve deixar altura 1");
        for (Vertex x = 0; x < n; ++x)
            CHECK(d.parents()[x] == r, "compressao: todos apontam para a raiz");
        CHECK(d.counters().writes == n - 2, "compressao reescreve n-2 ponteiros");
    }
    {
        using H = dsu::ForestDSU<dsu::LinkNaive, dsu::FindHalving, true>;
        auto d = make_path<H>(n);
        d.reset_counters();
        d.find(0);
        CHECK(d.counters().hops == n - 1, "halving: hops = profundidade");
        CHECK(d.max_depth() <= (n - 1 + 1) / 2 + 1, "halving reduz a altura a ~metade");
    }
    {
        using S = dsu::ForestDSU<dsu::LinkNaive, dsu::FindSplitting, true>;
        auto d = make_path<S>(n);
        d.reset_counters();
        d.find(0);
        CHECK(d.counters().hops == n - 1, "splitting: hops = profundidade");
        CHECK(d.counters().writes == n - 2, "splitting reescreve n-2 ponteiros");
        CHECK(d.max_depth() <= (n - 1 + 1) / 2 + 1, "splitting reduz a altura a ~metade");
    }
}

void list_cost_test() {
    // Sequência adversarial para a lista ingênua: unite(i, 0) anexa sempre o
    // conjunto grande ao singleton -> 1 + 2 + ... + (n-1) reapontamentos.
    const Vertex n = 500;
    dsu::ListNaive<true> naive(n);
    dsu::ListWeighted<true> weighted(n);
    for (Vertex i = 1; i < n; ++i) {
        naive.unite(i, 0);
        weighted.unite(i, 0);
    }
    CHECK(naive.counters().writes == std::uint64_t(n) * (n - 1) / 2,
          "lista ingenua: Theta(n^2) reapontamentos (obtido %llu)",
          (unsigned long long)naive.counters().writes);
    CHECK(weighted.counters().writes == n - 1,
          "lista ponderada: n-1 reapontamentos (obtido %llu)",
          (unsigned long long)weighted.counters().writes);

    // Com união ponderada, cada elemento é reapontado no máximo lg n vezes.
    dsu::ListWeighted<true> w2(1024);
    for (Vertex step = 1; step < 1024; step *= 2)
        for (Vertex i = 0; i + step < 1024; i += 2 * step) w2.unite(i, i + step);
    CHECK(w2.counters().writes == 1024u * 10 / 2,
          "lista ponderada, unioes balanceadas: (n lg n)/2 reapontamentos");
}

// ---- Aplicações -----------------------------------------------------------

Vertex bfs_components(Vertex n, const dsu::EdgeList& edges) {
    std::vector<std::vector<Vertex>> adj(n);
    for (auto [u, v] : edges) {
        adj[u].push_back(v);
        adj[v].push_back(u);
    }
    std::vector<char> seen(n, 0);
    Vertex comps = 0;
    for (Vertex s = 0; s < n; ++s) {
        if (seen[s]) continue;
        ++comps;
        std::queue<Vertex> q;
        q.push(s);
        seen[s] = 1;
        while (!q.empty()) {
            Vertex u = q.front();
            q.pop();
            for (Vertex v : adj[u])
                if (!seen[v]) seen[v] = 1, q.push(v);
        }
    }
    return comps;
}

double prim_mst(Vertex n, const std::vector<dsu::WEdge>& edges) {
    const double INF = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> w(n, std::vector<double>(n, INF));
    for (auto& e : edges) {
        w[e.u][e.v] = std::min(w[e.u][e.v], e.w);
        w[e.v][e.u] = std::min(w[e.v][e.u], e.w);
    }
    std::vector<double> key(n, INF);
    std::vector<char> in(n, 0);
    double total = 0;
    for (Vertex start = 0; start < n; ++start) {  // floresta, se desconexo
        if (in[start]) continue;
        key[start] = 0;
        while (true) {
            Vertex u = n;
            for (Vertex v = 0; v < n; ++v)
                if (!in[v] && key[v] < INF && (u == n || key[v] < key[u])) u = v;
            if (u == n) break;
            in[u] = 1;
            total += key[u];
            for (Vertex v = 0; v < n; ++v)
                if (!in[v] && w[u][v] < key[v]) key[v] = w[u][v];
        }
    }
    return total;
}

template <class D>
void app_test(const std::string& name, unsigned seed) {
    std::mt19937_64 rng(seed);
    for (Vertex n : {1u, 2u, 50u, 300u}) {
        for (double density : {0.0, 0.5, 1.0, 3.0}) {
            std::uniform_int_distribution<Vertex> pick(0, n - 1);
            std::uniform_real_distribution<double> wdist(0, 1000);
            const auto m = static_cast<std::size_t>(density * n);
            dsu::EdgeList edges;
            std::vector<dsu::WEdge> wedges;
            for (std::size_t i = 0; i < m; ++i) {
                Vertex u = pick(rng), v = pick(rng);
                edges.emplace_back(u, v);
                wedges.push_back({u, v, wdist(rng)});
            }
            Vertex comps = 0;
            dsu::connected_components<D>(n, edges, &comps);
            CHECK(comps == bfs_components(n, edges), "%s: componentes (n=%u)",
                  name.c_str(), n);
            double k = dsu::kruskal<D>(n, wedges);
            double p = prim_mst(n, wedges);
            CHECK(std::fabs(k - p) < 1e-6, "%s: Kruskal %.3f != Prim %.3f",
                  name.c_str(), k, p);
        }
    }
}

int main() {
    dsu::for_each_variant([](const char* name, auto fast, auto counting) {
        using Fast = typename decltype(fast)::type;
        using Counting = typename decltype(counting)::type;
        for (Vertex n : {1u, 2u, 3u, 10u, 100u, 700u}) {
            for (unsigned seed = 1; seed <= 4; ++seed) {
                for (double p : {0.1, 0.5, 0.9}) {
                    random_test<Fast>(name, n, seed, 3 * n + 20, p);
                    random_test<Counting>(std::string(name) + "[cont]", n, seed,
                                          3 * n + 20, p);
                }
            }
        }
        app_test<Fast>(name, 42);
        std::printf("  ok  %s\n", name);
    });
    path_heuristics_test();
    list_cost_test();

    std::printf("%d verificacoes, %d falhas\n", g_checks, g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
