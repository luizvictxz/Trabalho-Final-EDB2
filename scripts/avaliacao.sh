#!/usr/bin/env bash
# Bateria de experimentos da seção de Avaliação (Luiz Pinto).
#
# Uso:
#   ./scripts/avaliacao.sh            roda tudo e grava resultados/*.csv
#   ./scripts/avaliacao.sh E2 E4      roda só os experimentos indicados
#
# Pré-requisitos:
#   * grafos do SNAP descompactados em dados/ (só para o E6):
#       roadNet-CA.txt, web-Google.txt, com-lj.ungraph.txt
#     https://snap.stanford.edu/data/
#   * feche outros programas e, em notebook, mantenha na tomada.
#
# Depois: .venv/bin/python scripts/analise.py  (tabelas e figuras)
set -euo pipefail
cd "$(dirname "$0")/.."

# Binário das medições finais: -O2 com instruções nativas.
make -s clean
make -s build/dsu_bench CXXFLAGS="-std=c++17 -O2 -march=native"
mkdir -p resultados

B=./build/dsu_bench
REPS=5          # repetições cronometradas (a tela/CSV guardam todas)
BUDGET=30       # segundos por execução antes de declarar timeout
SEEDS="1 2 3"   # sementes independentes para as cargas aleatórias

EFICIENTES=ll-weighted,f-rank,f-size,f-compress,f-rank-compress,f-size-compress,f-rank-halving,f-rank-splitting

want() {  # want E3 -> verdadeiro se E3 foi pedido (ou se nada foi pedido)
  [ ${#SEL[@]} -eq 0 ] && return 0
  for s in "${SEL[@]}"; do [ "$s" = "$1" ] && return 0; done
  return 1
}
SEL=("$@")

# ---- Registro do ambiente ------------------------------------------------
{
  echo "data: $(date -Iseconds)"
  echo "cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')"
  echo "nucleos_logicos: $(nproc)"
  lscpu | grep -iE 'L1d|L2|L3' | sed 's/  */ /g'
  echo "memoria: $(free -h | awk '/^Mem|^Mem\.:/ {print $2}')"
  echo "governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/d)"
  echo "so: $(. /etc/os-release && echo "$PRETTY_NAME") / kernel $(uname -r)"
  echo "compilador: $(g++ --version | head -1)"
  echo "flags: -std=c++17 -O2 -march=native"
  echo "reps: $REPS  budget_s: $BUDGET  seeds: $SEEDS"
  echo "commit: $(git rev-parse --short HEAD 2>/dev/null || echo n/d)"
} > resultados/ambiente.txt

# ---- E1: pior caso das versões ingênuas ----------------------------------
if want E1; then
  rm -f resultados/e1_adversarial.csv
  for n in 1000 3000 10000 30000 100000; do
    $B --impl all --workload ll-adversarial --n $n --reps $REPS --budget $BUDGET \
       --csv resultados/e1_adversarial.csv
    $B --impl all --workload chain --n $n --reps $REPS --budget $BUDGET \
       --csv resultados/e1_adversarial.csv
  done
fi

# ---- E2: escalabilidade com operações aleatórias intercaladas ------------
if want E2; then
  rm -f resultados/e2_escalabilidade.csv
  for n in 10000 30000 100000 300000 1000000 3000000 10000000; do
    impls=all; [ $n -gt 100000 ] && impls=$EFICIENTES
    for seed in $SEEDS; do
      $B --impl $impls --workload random --n $n --seed $seed --reps $REPS \
         --budget $BUDGET --csv resultados/e2_escalabilidade.csv
    done
  done
fi

# ---- E3: proporção uniões/consultas --------------------------------------
if want E3; then
  rm -f resultados/e3_proporcao.csv
  for r in 0.1 0.3 0.5 0.7 0.9; do
    for seed in $SEEDS; do
      $B --impl $EFICIENTES --workload random --n 1000000 --m 4000000 \
         --union-ratio $r --seed $seed --reps $REPS --budget $BUDGET \
         --csv resultados/e3_proporcao.csv
    done
  done
fi

# ---- E4: componentes conexas em G(n, m) com densidades diferentes --------
if want E4; then
  rm -f resultados/e4_er.csv
  n=1000000
  for deg in 0.5 1 2 4 8 16; do
    m=$(awk -v n=$n -v d=$deg 'BEGIN { printf "%d", n * d / 2 }')
    for seed in $SEEDS; do
      $B --impl $EFICIENTES --workload er --n $n --m $m --seed $seed \
         --reps $REPS --budget $BUDGET --csv resultados/e4_er.csv
    done
  done
fi

# ---- E5: grade (percolação / rotulação de imagens) ------------------------
if want E5; then
  rm -f resultados/e5_grade.csv
  for order in random sequential; do
    for p in 0.3 0.5 0.7; do
      for seed in $SEEDS; do
        $B --impl $EFICIENTES --workload grid --n 4000000 --grid-p $p \
           --order $order --seed $seed --reps $REPS --budget $BUDGET \
           --csv resultados/e5_grade.csv
      done
    done
  done
fi

# ---- E6: componentes conexas em grafos reais (SNAP) -----------------------
if want E6; then
  rm -f resultados/e6_reais.csv
  for g in roadNet-CA web-Google com-lj.ungraph; do
    f=dados/$g.txt
    if [ ! -f "$f" ]; then echo "aviso: $f ausente, pulando"; continue; fi
    # Ingênuas com orçamento curto: só para registrar que são inviáveis.
    for order in sequential random; do
      $B --impl ll-naive,f-naive --workload file --file "$f" --order $order \
         --reps 1 --warmup 0 --budget 10 --csv resultados/e6_reais.csv
      $B --impl $EFICIENTES --workload file --file "$f" --order $order \
         --reps $REPS --budget $BUDGET --csv resultados/e6_reais.csv
    done
  done
fi

echo "concluido: $(date -Iseconds)" >> resultados/ambiente.txt
