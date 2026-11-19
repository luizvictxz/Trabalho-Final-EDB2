# Conjuntos disjuntos: implementação e avaliação experimental

Código do trabalho de **Estrutura de Dados Básicas II (IMD0039, UFRN)** sobre
conjuntos disjuntos (*union-find*). A biblioteca é escrita em C++17, usa
apenas cabeçalhos e depende só da biblioteca padrão.

## Organização

```
include/dsu/        biblioteca
  linked_list.hpp     listas ligadas (ingênua e com união ponderada)
  forest.hpp          florestas, com políticas de ligação e de busca
  counters.hpp        contadores de instrumentação
  dsu.hpp             cabeçalho único e nomes das variantes
  apps.hpp            aplicações: componentes conexas e Kruskal
tests/test_dsu.cpp  testes de corretude e de invariantes
bench/              programa de benchmark e geradores de carga
examples/           componentes.cpp e kruskal.cpp
scripts/            experimentos (avaliacao.sh) e análise (analise.py)
artigo/artigo.pdf   artigo final
```

O artigo final está em [`artigo/artigo.pdf`](artigo/artigo.pdf).

## Compilação e testes

```bash
make                 # compila tudo em build/
make test            # roda os testes (cerca de 10,7 milhões de verificações)
make test-sanitize   # testes com AddressSanitizer e UBSan (opcional)
```

No Windows ou no Visual Studio, use CMake:
`cmake -B build && cmake --build build --config Release`.

## Uso

```cpp
#include "dsu/dsu.hpp"

dsu::ForestRankCompress<> d(n);   // versão otimizada principal
d.unite(3, 7);                    // retorna false se já estavam juntos
d.find(7);                        // representante do conjunto
d.same(3, 7);                     // true
d.num_sets();                     // quantidade de conjuntos

dsu::ForestRankCompress<true> dc(n);  // mesma estrutura, com contadores
dc.counters().hops;                   // arestas percorridas nas buscas
```

## Variantes

| Nome no benchmark  | Tipo em C++              | Ligação         | Busca                 | Custo total no pior caso |
|--------------------|--------------------------|-----------------|-----------------------|--------------------------|
| `ll-naive`         | `ListNaive`              | anexa y a x     | ponteiro direto       | O(m + n²)                |
| `ll-weighted`      | `ListWeighted`           | menor na maior  | ponteiro direto       | O(m + n lg n)            |
| `f-naive`          | `ForestNaive`            | ingênua         | sem alteração         | O(n + m·n)               |
| `f-rank`           | `ForestRank`             | pelo posto      | sem alteração         | O(n + m lg n)            |
| `f-size`           | `ForestSize`             | pelo tamanho    | sem alteração         | O(n + m lg n)            |
| `f-compress`       | `ForestCompress`         | ingênua         | compressão de caminho | O(m log₍₂₊ₘ/ₙ₎ n)        |
| `f-rank-compress`  | `ForestRankCompress` ★   | pelo posto      | compressão de caminho | O(m α(m, n))             |
| `f-size-compress`  | `ForestSizeCompress`     | pelo tamanho    | compressão de caminho | O(m α(m, n))             |
| `f-rank-halving`   | `ForestRankHalving`      | pelo posto      | *path halving*        | O(m α(m, n))             |
| `f-rank-splitting` | `ForestRankSplitting`    | pelo posto      | *path splitting*      | O(m α(m, n))             |

n é o número de elementos e m, o de operações. ★ indica a versão otimizada
principal. α é a inversa da função de Ackermann e não passa de 4 para
qualquer n usado na prática. Os limites das listas vêm de Cormen et al.
(2022); os das florestas, de Tarjan (1975) e de Tarjan e van Leeuwen (1984).

### Decisões de implementação

* **Nós como índices em vetores**, também nas listas. Cada nó continua
  apontando para o próximo, mas sem alocação individual. Assim, a comparação
  com a floresta, que também usa vetores, mede os algoritmos, e não o
  alocador de memória.
* **Busca iterativa.** A floresta ingênua pode formar caminhos de
  comprimento n−1, e uma versão recursiva estouraria a pilha.
* **Posto em 1 byte.** Com união pelo posto, o posto nunca passa de ⌊lg n⌋.
* **Contadores sem custo.** A instrumentação é ligada por parâmetro de
  template (`if constexpr`). Na versão cronometrada, ela nem chega ao código
  compilado.

## Benchmark

```bash
./build/dsu_bench --list
./build/dsu_bench --impl all --workload random --n 1000000 --reps 5 --csv resultados/r.csv
./build/dsu_bench --impl f-rank-compress,ll-weighted --workload grid --n 4000000 --order sequential
./build/dsu_bench --impl all --workload file --file dados/roadNet-CA.txt
```

### Cargas de trabalho (`--workload`)

| Carga            | Descrição |
|------------------|-----------|
| `random`         | m operações intercaladas; cada uma é `unite` com probabilidade `--union-ratio` e `same` no restante. |
| `er`             | Componentes conexas de um grafo aleatório G(n, m) (Erdős–Rényi). |
| `grid`           | Grade √n × √n em que cada aresta existe com probabilidade `--grid-p` (percolação, rotulação de imagens). `--order sequential` processa linha a linha. |
| `file`           | Lista de arestas `u v`, no formato do SNAP. Os vértices são renumerados de 0 a n−1. |
| `chain`          | `unite(i, i+1)` seguido de consultas. É o pior caso da floresta ingênua (altura n−1). |
| `ll-adversarial` | `unite(i, 0)`. É o pior caso da lista ingênua: Θ(n²) reapontamentos. |

Em qualquer carga, `--queries Q` acrescenta Q consultas `same` aleatórias no
final.

### Como as medições são feitas

* As operações são geradas antes de o relógio começar. O tempo medido cobre
  só as operações; a criação da estrutura aparece à parte, em `init_ms`.
* Há uma execução de aquecimento (`--warmup`) e depois `--reps` execuções
  cronometradas. A saída na tela mostra a mediana.
* Uma execução separada, com contadores ligados, fornece as métricas que
  não dependem da máquina.
* Cada execução tem um limite de tempo (`--budget`, 30 s por padrão). A
  variante que estoura o limite é marcada como `timeout` e não é repetida.
* O `checksum` precisa ser o mesmo em todas as variantes para a mesma carga.
  É uma conferência cruzada de corretude: se houver diferença, o programa
  termina com erro.

### Colunas do CSV

| Coluna | Significado |
|--------|-------------|
| `impl`, `workload`, `params`, `n`, `ops`, `seed`, `rep` | identificação da execução |
| `status` | `ok` ou `timeout` |
| `init_ms` | tempo para criar a estrutura (MakeSet de todos os elementos) |
| `time_ms`, `ns_per_op` | tempo das operações e tempo médio por operação |
| `links` | uniões que de fato juntaram dois conjuntos |
| `components` | conjuntos restantes no final |
| `finds` | chamadas a `find` (cada `unite` faz duas) |
| `hops` | arestas percorridas nas buscas, ou seja, a soma das profundidades (nas listas, 1 por busca) |
| `writes` | ponteiros reescritos (compressão, *halving* ou *splitting*; nas listas, reapontamentos na união) |
| `hops_per_find` | `hops / finds` |
| `max_depth` | altura máxima da floresta no final |
| `mem_bytes` | memória ocupada pelos vetores da estrutura |
| `checksum` | resultado agregado das operações, que deve coincidir entre variantes |

Quando a execução com contadores estoura o limite de tempo, as colunas de
contadores ficam vazias.

Para as medições do artigo, compile com
`make CXXFLAGS="-std=c++17 -O2 -march=native"`, feche outros programas e
registre processador, memória, compilador e opções de compilação.

## Referências

* B. A. Galler e M. J. Fischer. An improved equivalence algorithm. *Communications of the ACM*, 7(5):301–303, 1964.
* R. E. Tarjan. Efficiency of a good but not linear set union algorithm. *Journal of the ACM*, 22(2):215–225, 1975.
* R. E. Tarjan e J. van Leeuwen. Worst-case analysis of set union algorithms. *Journal of the ACM*, 31(2):245–281, 1984.
* T. H. Cormen, C. E. Leiserson, R. L. Rivest e C. Stein. *Algoritmos: Teoria e Prática*. Elsevier, 2012 (tradução da 3.ª ed.).
* M. M. A. Patwary, J. Blair e F. Manne. Experiments on union-find algorithms for the disjoint-set data structure. *SEA 2010*, LNCS 6049, p. 411–423.

## Reprodução dos experimentos

```bash
./scripts/avaliacao.sh            # compila com -O2 -march=native e grava resultados/*.csv (~1 h)
python3 scripts/analise.py        # gera as figuras e tabelas e resultados/resumo.txt
```

Os grafos reais do experimento E6 (`roadNet-CA.txt`, `web-Google.txt` e
`com-lj.ungraph.txt`) devem ser baixados da coleção SNAP
(<https://snap.stanford.edu/data/>) para a pasta `dados/`. A análise usa
Python 3 com pandas, numpy e matplotlib.

## Uso de IA

O uso de ferramentas de IA por cada integrante está declarado no Anexo A do
artigo.
