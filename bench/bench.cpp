// Benchmark das variantes de conjuntos disjuntos.
//
// Para cada variante e repetição:
//   * execução cronometrada com a versão SEM contadores (Count = false);
//   * uma execução extra com a versão COM contadores (Count = true) para
//     obter as métricas independentes de máquina (hops, writes, altura).
// Os resultados são impressos e, com --csv, acrescentados a um arquivo CSV.
//
// Exemplos:
//   ./build/dsu_bench --list
//   ./build/dsu_bench --impl all --workload random --n 1000000 --reps 5
//   ./build/dsu_bench --impl f-rank-compress,ll-weighted --workload grid
//                     --n 4000000 --order sequential --csv resultados/grid.csv
//   ./build/dsu_bench --impl all --workload file --file roadNet-CA.txt
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "dsu/dsu.hpp"
#include "workloads.hpp"

using Clock = std::chrono::steady_clock;
using bench::Op;
using bench::Workload;

struct Options {
    bench::Params p;
    std::vector<std::string> impls{"all"};
    int reps = 5;
    int warmup = 1;
    double budget_s = 30.0;
    bool count = true;
    std::string csv;
};

struct TimedResult {
    bool timeout = false;
    double init_ms = 0, time_ms = 0;
    std::uint64_t checksum = 0;
};

struct CountResult {
    bool timeout = false;
    dsu::Counters c;
    std::uint64_t components = 0, max_depth = 0, mem_bytes = 0, checksum = 0;
};

// Executa as operações em blocos, verificando o orçamento de tempo entre
// blocos (custo desprezível frente às operações).
template <class D>
bool run_ops(D& d, const Workload& w, Clock::time_point deadline,
             std::uint64_t& checksum) {
    constexpr std::size_t kBlock = 256;
    const std::size_t N = w.ops.size();
    std::uint64_t chk = 0;
    for (std::size_t i = 0; i < N;) {
        const std::size_t end = std::min(N, i + kBlock);
        for (; i < end; ++i) {
            const Op& o = w.ops[i];
            if (o.kind == Op::Unite)
                chk += d.unite(o.a, o.b) ? 1 : 0;
            else
                chk += d.same(o.a, o.b) ? 2 : 0;
        }
        if (Clock::now() > deadline) return false;
    }
    checksum = chk;  // igual para todas as variantes: checagem cruzada
    return true;
}

template <class D>
TimedResult timed_run(const Workload& w, double budget_s) {
    TimedResult r;
    const auto t0 = Clock::now();
    D d(w.n);
    const auto t1 = Clock::now();
    const auto deadline =
        t1 + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(budget_s));
    r.timeout = !run_ops(d, w, deadline, r.checksum);
    const auto t2 = Clock::now();
    r.init_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.time_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    return r;
}

template <class D>
CountResult count_run(const Workload& w, double budget_s) {
    CountResult r;
    D d(w.n);
    const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                             std::chrono::duration<double>(budget_s));
    r.timeout = !run_ops(d, w, deadline, r.checksum);
    r.c = d.counters();
    if (!r.timeout) {
        r.components = d.num_sets();
        r.max_depth = d.max_depth();
    }
    r.mem_bytes = d.memory_bytes();
    return r;
}

static void usage() {
    std::puts(
        "uso: dsu_bench [opcoes]\n"
        "  --impl NOMES        'all' ou lista separada por virgula (ver --list)\n"
        "  --workload W        random | ll-adversarial | chain | er | grid | file\n"
        "  --n N               numero de elementos (padrao 1000000)\n"
        "  --m M               operacoes (random) ou arestas (er); 0 = padrao\n"
        "  --queries Q         consultas same() extras ao final\n"
        "  --union-ratio R     fracao de unite na carga random (padrao 0.5)\n"
        "  --grid-p P          prob. de manter cada aresta da grade (padrao 0.5)\n"
        "  --order O           random | sequential (grid e file)\n"
        "  --file ARQ          lista de arestas para a carga file\n"
        "  --seed S            semente (padrao 1)\n"
        "  --reps R            repeticoes cronometradas (padrao 5)\n"
        "  --warmup W          execucoes de aquecimento descartadas (padrao 1)\n"
        "  --budget SEG        tempo maximo por execucao (padrao 30)\n"
        "  --no-count          nao executa a versao instrumentada\n"
        "  --csv ARQ           acrescenta resultados ao CSV\n"
        "  --list              lista variantes e cargas");
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep))
        if (!item.empty()) out.push_back(item);
    return out;
}

static Options parse(int argc, char** argv) {
    Options o;
    auto need = [&](int& i) -> std::string {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "faltou valor para %s\n", argv[i]);
            std::exit(2);
        }
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--impl") o.impls = split(need(i), ',');
        else if (a == "--workload") o.p.workload = need(i);
        else if (a == "--n") o.p.n = static_cast<bench::Vertex>(std::stoull(need(i)));
        else if (a == "--m") o.p.m = std::stoull(need(i));
        else if (a == "--queries") o.p.queries = std::stoull(need(i));
        else if (a == "--union-ratio") o.p.union_ratio = std::stod(need(i));
        else if (a == "--grid-p") o.p.grid_p = std::stod(need(i));
        else if (a == "--order") o.p.order = need(i);
        else if (a == "--file") o.p.file = need(i);
        else if (a == "--seed") o.p.seed = std::stoull(need(i));
        else if (a == "--reps") o.reps = std::stoi(need(i));
        else if (a == "--warmup") o.warmup = std::stoi(need(i));
        else if (a == "--budget") o.budget_s = std::stod(need(i));
        else if (a == "--no-count") o.count = false;
        else if (a == "--csv") o.csv = need(i);
        else if (a == "--list") {
            std::puts("variantes:");
            dsu::for_each_variant([](const char* name, auto, auto) {
                std::printf("  %s\n", name);
            });
            std::puts("cargas: random ll-adversarial chain er grid file");
            std::exit(0);
        } else if (a == "-h" || a == "--help") {
            usage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "opcao desconhecida: %s\n", a.c_str());
            usage();
            std::exit(2);
        }
    }
    return o;
}

static bool file_is_empty(const std::string& path) {
    std::ifstream in(path, std::ios::ate);
    return !in || in.tellg() == 0;
}

int main(int argc, char** argv) {
    Options opt = parse(argc, argv);

    Workload w;
    try {
        w = bench::make_workload(opt.p);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "erro: %s\n", e.what());
        return 2;
    }
    std::printf("carga=%s n=%u ops=%zu %s\n", w.name.c_str(), w.n, w.ops.size(),
                w.note.c_str());

    std::ofstream csv;
    if (!opt.csv.empty()) {
        const bool header = file_is_empty(opt.csv);
        csv.open(opt.csv, std::ios::app);
        if (!csv) {
            std::fprintf(stderr, "nao foi possivel abrir %s\n", opt.csv.c_str());
            return 2;
        }
        if (header)
            csv << "impl,workload,params,n,ops,seed,rep,status,init_ms,time_ms,"
                   "ns_per_op,links,components,finds,hops,writes,hops_per_find,"
                   "max_depth,mem_bytes,checksum\n";
    }

    const std::set<std::string> wanted(opt.impls.begin(), opt.impls.end());
    const bool all = wanted.count("all") > 0;
    std::set<std::string> known;
    std::set<std::uint64_t> checksums;

    std::printf("%-17s %10s %10s %12s %10s %12s %9s\n", "variante", "tempo_ms",
                "ns/op", "hops/find", "writes", "altura", "status");

    dsu::for_each_variant([&](const char* name, auto fast, auto counting) {
        known.insert(name);
        if (!all && !wanted.count(name)) return;
        using Fast = typename decltype(fast)::type;
        using Counting = typename decltype(counting)::type;

        CountResult cr;
        if (opt.count) cr = count_run<Counting>(w, opt.budget_s);

        for (int i = 0; i < opt.warmup; ++i) {
            if (timed_run<Fast>(w, opt.budget_s).timeout) break;
        }
        std::vector<double> times;
        for (int rep = 0; rep < opt.reps; ++rep) {
            TimedResult tr = timed_run<Fast>(w, opt.budget_s);
            const char* status = tr.timeout ? "timeout" : "ok";
            if (!tr.timeout) {
                times.push_back(tr.time_ms);
                checksums.insert(tr.checksum);
            }
            const double ns_per_op =
                w.ops.empty() ? 0 : tr.time_ms * 1e6 / double(w.ops.size());
            const double hpf =
                cr.c.finds ? double(cr.c.hops) / double(cr.c.finds) : 0.0;
            if (csv) {
                csv << name << ',' << w.name << ",\"" << w.note << "\"," << w.n
                    << ',' << w.ops.size() << ',' << opt.p.seed << ',' << rep << ','
                    << status << ',' << tr.init_ms << ',' << tr.time_ms << ','
                    << ns_per_op << ',';
                // Métricas só são gravadas se a execução instrumentada
                // terminou; contadores parciais seriam enganosos.
                if (opt.count && !cr.timeout)
                    csv << cr.c.links << ',' << cr.components << ',' << cr.c.finds
                        << ',' << cr.c.hops << ',' << cr.c.writes << ',' << hpf
                        << ',' << cr.max_depth << ',';
                else
                    csv << ",,,,,,,";
                csv << cr.mem_bytes << ',' << (tr.timeout ? 0 : tr.checksum) << '\n';
            }
            if (tr.timeout) break;  // não insiste em variantes inviáveis
        }

        std::sort(times.begin(), times.end());
        const double median = times.empty() ? NAN : times[times.size() / 2];
        const double hpf = cr.c.finds ? double(cr.c.hops) / double(cr.c.finds) : 0.0;
        std::printf("%-17s %10.2f %10.2f", name, median,
                    w.ops.empty() ? 0.0 : median * 1e6 / double(w.ops.size()));
        if (opt.count && !cr.timeout)
            std::printf(" %12.3f %10llu %12llu", hpf, (unsigned long long)cr.c.writes,
                        (unsigned long long)cr.max_depth);
        else
            std::printf(" %12s %10s %12s", "-", "-", "-");
        std::printf(" %9s\n", times.empty() ? "timeout" : "ok");
        std::fflush(stdout);
    });

    for (const auto& n : wanted)
        if (n != "all" && !known.count(n))
            std::fprintf(stderr, "aviso: variante desconhecida '%s'\n", n.c_str());
    if (checksums.size() > 1) {
        std::fprintf(stderr,
                     "ERRO: as variantes discordaram no resultado (checksum)!\n");
        return 1;
    }
    return 0;
}
