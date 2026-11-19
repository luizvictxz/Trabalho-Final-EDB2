#!/usr/bin/env python3
"""Análise dos experimentos da seção de Avaliação.

Lê resultados/e*.csv (gerados por scripts/avaliacao.sh) e produz:
  artigo/avaliacao/figuras/*.pdf   figuras do artigo (vetoriais)
  slides/figuras/aval_*.png        as mesmas figuras em PNG para os slides
  artigo/avaliacao/tab_*.tex       tabelas em LaTeX (booktabs)
  resultados/resumo.txt            todos os números agregados, para o texto

Agregação: para cada (variante, carga, parâmetros, n, semente) toma-se a
MEDIANA das repetições cronometradas; entre sementes, a MÉDIA dessas
medianas (e o desvio-padrão). Contadores (hops, writes, altura) são
determinísticos para uma semente e vêm da execução instrumentada.

Uso: .venv/bin/python scripts/analise.py
"""
from __future__ import annotations

import math
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

RAIZ = Path(__file__).resolve().parent.parent
RES = RAIZ / "resultados"
OUT = RAIZ / "artigo" / "avaliacao"
FIG = OUT / "figuras"
SLIDES = RAIZ / "slides" / "figuras"

# ---------------------------------------------------------------------------
# Identidade visual: cada variante tem SEMPRE a mesma cor e o mesmo marcador
# em todas as figuras (a cor acompanha a variante, não a posição na legenda).
# Paleta categórica validada (ordem fixa); as ingênuas, fora das 8 cores,
# usam cinza-escuro com traço/marcador próprios.
# ---------------------------------------------------------------------------
ESTILO = {
    "f-rank-compress":  dict(color="#2a78d6", marker="o", ls="-"),
    "ll-weighted":      dict(color="#eb6834", marker="s", ls="-"),
    "f-rank":           dict(color="#1baf7a", marker="^", ls="-"),
    "f-compress":       dict(color="#eda100", marker="D", ls="-"),
    "f-rank-halving":   dict(color="#e87ba4", marker="v", ls="-"),
    "f-rank-splitting": dict(color="#008300", marker="P", ls="-"),
    "f-size-compress":  dict(color="#6250d6", marker="X", ls="-"),
    "f-size":           dict(color="#e34948", marker="<", ls="-"),
    "ll-naive":         dict(color="#52514e", marker="s", ls="--"),
    "f-naive":          dict(color="#52514e", marker="o", ls=":"),
}
ORDEM = ["ll-naive", "ll-weighted", "f-naive", "f-rank", "f-size", "f-compress",
         "f-rank-compress", "f-size-compress", "f-rank-halving", "f-rank-splitting"]

TINTA = "#0b0b0b"
TINTA2 = "#52514e"
GRADE = "#e4e3df"

plt.rcParams.update({
    "font.family": "STIXGeneral",
    "mathtext.fontset": "stix",
    "font.size": 9,
    "axes.titlesize": 9,
    "axes.labelsize": 9,
    "legend.fontsize": 7.5,
    "xtick.labelsize": 8,
    "ytick.labelsize": 8,
    "axes.edgecolor": TINTA2,
    "axes.labelcolor": TINTA,
    "xtick.color": TINTA2,
    "ytick.color": TINTA2,
    "axes.linewidth": 0.6,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.color": GRADE,
    "grid.linewidth": 0.5,
    "grid.linestyle": "-",
    "lines.linewidth": 1.6,
    "lines.markersize": 4.5,
    "legend.frameon": False,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.02,
})

LARGURA = 6.3  # polegadas: largura útil de texto do template SBC (~16 cm)


# ---------------------------------------------------------------------------
# Leitura e agregação
# ---------------------------------------------------------------------------
def ler(nome: str) -> pd.DataFrame | None:
    p = RES / nome
    if not p.exists() or p.stat().st_size == 0:
        print(f"aviso: {p} ausente; partes dependentes serão puladas")
        return None
    df = pd.read_csv(p)
    df["params"] = df["params"].astype(object).fillna("").astype(str)
    return df


def parametro(params: str, chave: str):
    m = re.search(rf"{chave}=([^;]+)", params)
    return m.group(1) if m else None


def agrega(df: pd.DataFrame) -> pd.DataFrame:
    """Mediana entre repetições, depois média entre sementes."""
    chave = ["impl", "workload", "params", "n", "ops", "seed"]
    met = ["links", "components", "finds", "hops", "writes", "hops_per_find",
           "max_depth", "mem_bytes"]

    def por_semente(g: pd.DataFrame) -> pd.Series:
        ok = g[g["status"] == "ok"]
        s = {
            "timeout": len(ok) == 0 or (g["status"] == "timeout").any(),
            "time_ms": ok["time_ms"].median() if len(ok) else np.nan,
            "ns_per_op": ok["ns_per_op"].median() if len(ok) else np.nan,
            "cv": (ok["time_ms"].std(ddof=1) / ok["time_ms"].mean()
                   if len(ok) > 1 else np.nan),
            "reps": len(ok),
        }
        for c in met:
            s[c] = g[c].iloc[0]
        return pd.Series(s)

    s = df.groupby(chave, sort=False).apply(por_semente, include_groups=False).reset_index()
    # "ops" fica fora da chave: na grade, o nº de arestas varia com a semente.
    chave2 = ["impl", "workload", "params", "n"]
    agg = s.groupby(chave2, sort=False).agg(
        ops=("ops", "mean"),
        timeout=("timeout", "any"),
        time_ms=("time_ms", "mean"),
        ns_per_op=("ns_per_op", "mean"),
        ns_sd=("ns_per_op", "std"),
        cv_max=("cv", "max"),
        sementes=("seed", "nunique"),
        **{c: (c, "mean") for c in met},
    ).reset_index()
    agg.loc[agg["timeout"], ["time_ms", "ns_per_op"]] = np.nan
    agg["writes_per_find"] = agg["writes"] / agg["finds"]
    return agg


# ---------------------------------------------------------------------------
# Utilidades de figura e tabela
# ---------------------------------------------------------------------------
def linhas(ax, d: pd.DataFrame, x: str, y: str, impls: list[str], rotulos=True):
    for impl in impls:
        s = d[(d["impl"] == impl)].sort_values(x)
        s = s[np.isfinite(s[y])]
        if s.empty:
            continue
        st = ESTILO[impl]
        ax.plot(s[x], s[y], label=impl, color=st["color"], ls=st["ls"],
                marker=st["marker"], markeredgecolor="white",
                markeredgewidth=0.6, zorder=3)


def salvar(fig, nome: str):
    FIG.mkdir(parents=True, exist_ok=True)
    SLIDES.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIG / f"{nome}.pdf")
    fig.savefig(SLIDES / f"aval_{nome}.png", dpi=250)
    plt.close(fig)
    print(f"figura: {nome}")


def legenda_comum(fig, axes, ncol=5):
    vistos = {}
    for ax in np.atleast_1d(axes):
        for h, l in zip(*ax.get_legend_handles_labels()):
            vistos.setdefault(l, h)
    labs = [l for l in ORDEM if l in vistos]
    fig.legend([vistos[l] for l in labs], labs,
               loc="lower center", ncol=ncol,
               bbox_to_anchor=(0.5, 0.98),
               handlelength=2.4, columnspacing=1.2)


def fmt(v, casas=1):
    if v is None or (isinstance(v, float) and not math.isfinite(v)):
        return "--"
    s = f"{v:,.{casas}f}"
    return s.replace(",", "X").replace(".", ",").replace("X", ".")


def fmt_tempo(v):
    """ns/op com 3 algarismos significativos, vírgula decimal."""
    if v is None or not math.isfinite(v):
        return "--"
    if v >= 100:
        return fmt(v, 0)
    if v >= 10:
        return fmt(v, 1)
    return fmt(v, 2)


def tt(impl):
    return f"\\texttt{{{impl}}}"


def inclinacao(n, t):
    """Expoente k de t ~ n^k por mínimos quadrados em log-log."""
    n, t = np.asarray(n, float), np.asarray(t, float)
    ok = np.isfinite(t) & (t > 0)
    if ok.sum() < 2:
        return np.nan
    return np.polyfit(np.log(n[ok]), np.log(t[ok]), 1)[0]


resumo: list[str] = []


def R(*linhas_txt):
    for l in linhas_txt:
        resumo.append(str(l))


# ---------------------------------------------------------------------------
# E1: pior caso
# ---------------------------------------------------------------------------
def e1():
    df = ler("e1_adversarial.csv")
    if df is None:
        return
    a = agrega(df)
    R("=" * 70, "E1 - pior caso (tempo total em ms; mediana de 5 repetições)", "=" * 70)
    fig, axes = plt.subplots(1, 2, figsize=(LARGURA, 2.5), sharey=True)
    titulos = {"ll-adversarial": r"(a) ll-adversarial: Union$(i,0)$",
               "chain": r"(b) chain: Union$(i,i{+}1)$ + $n$ consultas"}
    impls = ["ll-naive", "ll-weighted", "f-naive", "f-compress", "f-rank-compress"]
    slopes = {}
    for ax, w in zip(axes, ["ll-adversarial", "chain"]):
        d = a[a["workload"] == w]
        linhas(ax, d, "n", "time_ms", impls)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(titulos[w], loc="left")
        ax.set_xlabel("$n$ (elementos)")
        R(f"\n[{w}]")
        piv = d.pivot_table(index="impl", columns="n", values="time_ms")
        piv = piv.reindex([i for i in ORDEM if i in piv.index])
        R(piv.to_string(float_format=lambda v: f"{v:.3f}"))
        R("expoente k (t ~ n^k), ajuste com n >= 1e4:")
        for impl in piv.index:
            cols = [c for c in piv.columns if c >= 10000]  # regime assintótico
            k = inclinacao(cols, piv.loc[impl, cols].values)
            slopes[(w, impl)] = k
            R(f"  {impl:18s} k = {k:.2f}")
        R("writes:")
        R(d.pivot_table(index="impl", columns="n", values="writes")
          .reindex([i for i in ORDEM if i in piv.index]).to_string())
        R("altura final:")
        R(d.pivot_table(index="impl", columns="n", values="max_depth")
          .reindex([i for i in ORDEM if i in piv.index]).to_string())
    axes[0].set_ylabel("tempo total (ms)")
    legenda_comum(fig, axes)
    fig.tight_layout()
    salvar(fig, "fig_e1_pior_caso")

    # Tabela: tempo em n = 10^5 e expoente empírico
    linhas_tab = []
    nmax = a["n"].max()
    for impl in ORDEM:
        cel = [tt(impl)]
        for w in ["ll-adversarial", "chain"]:
            d = a[(a["workload"] == w) & (a["impl"] == impl) & (a["n"] == nmax)]
            t = d["time_ms"].iloc[0] if len(d) else np.nan
            cel.append(fmt(t, 2) if t < 10 else fmt(t, 0) if math.isfinite(t) else "--")
            cel.append(fmt(slopes.get((w, impl), np.nan), 2))
        linhas_tab.append(" & ".join(cel) + r" \\")
    tex = r"""\begin{table}[ht]
\centering
\caption{Pior caso (E1): tempo total para $n=10^5$ e expoente empírico $k$
do ajuste $t \propto n^k$ ($n$ de $10^4$ a $10^5$). Mediana de 5 repetições.}
\label{tab:e1}
\small
\begin{tabular}{@{}lrrrr@{}}
\toprule
 & \multicolumn{2}{c}{\texttt{ll-adversarial}} & \multicolumn{2}{c}{\texttt{chain}} \\
\cmidrule(lr){2-3}\cmidrule(l){4-5}
\textbf{Variante} & \textbf{tempo (ms)} & $k$ & \textbf{tempo (ms)} & $k$ \\
\midrule
""" + "\n".join(linhas_tab) + r"""
\bottomrule
\end{tabular}
\end{table}
"""
    (OUT / "tab_e1_pior_caso.tex").write_text(tex)


# ---------------------------------------------------------------------------
# E2: escalabilidade
# ---------------------------------------------------------------------------
def e2():
    df = ler("e2_escalabilidade.csv")
    if df is None:
        return
    a = agrega(df)
    R("", "=" * 70, "E2 - escalabilidade, carga random (ns/op; média de 3 sementes)", "=" * 70)
    R(a.pivot_table(index="impl", columns="n", values="ns_per_op")
      .reindex([i for i in ORDEM if i in set(a["impl"])])
      .to_string(float_format=lambda v: f"{v:.1f}"))
    R("desvio-padrão entre sementes (ns/op):")
    R(a.pivot_table(index="impl", columns="n", values="ns_sd")
      .reindex([i for i in ORDEM if i in set(a["impl"])])
      .to_string(float_format=lambda v: f"{v:.2f}"))
    R("CV máximo entre repetições (por semente):", f"{a['cv_max'].max():.3f}")
    R("hops/find:")
    R(a.pivot_table(index="impl", columns="n", values="hops_per_find")
      .reindex([i for i in ORDEM if i in set(a["impl"])])
      .to_string(float_format=lambda v: f"{v:.3f}"))
    R("writes/find:")
    R(a.pivot_table(index="impl", columns="n", values="writes_per_find")
      .reindex([i for i in ORDEM if i in set(a["impl"])])
      .to_string(float_format=lambda v: f"{v:.3f}"))
    R("altura final:")
    R(a.pivot_table(index="impl", columns="n", values="max_depth")
      .reindex([i for i in ORDEM if i in set(a["impl"])]).to_string())
    R("memória (bytes/elemento):")
    mem = a.groupby("impl").apply(lambda g: (g["mem_bytes"] / g["n"]).mean(),
                                  include_groups=False)
    R(mem.reindex([i for i in ORDEM if i in mem.index]).to_string())

    fig, axes = plt.subplots(1, 2, figsize=(LARGURA, 2.6), sharey=False)
    linhas(axes[0], a, "n", "ns_per_op",
           ["ll-naive", "ll-weighted", "f-naive", "f-rank", "f-compress", "f-rank-compress"])
    axes[0].set_yscale("log")
    axes[0].set_title("(a) efeito de cada heurística", loc="left")
    axes[0].set_ylabel("tempo por operação (ns)")
    linhas(axes[1], a, "n", "ns_per_op",
           ["f-rank-compress", "f-size-compress", "f-rank-halving", "f-rank-splitting"])
    axes[1].set_title("(b) variantes $O(m\\,\\alpha(m,n))$", loc="left")
    axes[1].set_ylim(bottom=0)
    for ax in axes:
        ax.set_xscale("log")
        ax.set_xlabel("$n$ (elementos; $m = 2n$ operações)")
    legenda_comum(fig, axes)
    fig.tight_layout()
    salvar(fig, "fig_e2_escalabilidade")


# ---------------------------------------------------------------------------
# E3 e E4: proporção de uniões e densidade (componentes conexas)
# ---------------------------------------------------------------------------
PRINCIPAIS = ["ll-weighted", "f-rank", "f-compress", "f-rank-compress", "f-rank-halving"]


def e3_e4():
    d3, d4 = ler("e3_proporcao.csv"), ler("e4_er.csv")
    if d3 is None and d4 is None:
        return
    fig, axes = plt.subplots(1, 2, figsize=(LARGURA, 2.5))
    if d3 is not None:
        a = agrega(d3)
        a["r"] = a["params"].map(lambda p: float(parametro(p, "union_ratio")))
        R("", "=" * 70, "E3 - proporção de uniões (n=1e6, m=4e6) ns/op", "=" * 70)
        R(a.pivot_table(index="impl", columns="r", values="ns_per_op")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.1f}"))
        R("hops/find:")
        R(a.pivot_table(index="impl", columns="r", values="hops_per_find")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.3f}"))
        R("writes/find:")
        R(a.pivot_table(index="impl", columns="r", values="writes_per_find")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.3f}"))
        R("componentes no final:")
        R(a[a["impl"] == "f-rank-compress"][["r", "components"]].to_string(index=False))
        linhas(axes[0], a, "r", "ns_per_op", PRINCIPAIS)
        axes[0].set_xticks([0.1, 0.3, 0.5, 0.7, 0.9])
        axes[0].set_xticklabels(["0,1", "0,3", "0,5", "0,7", "0,9"])
        axes[0].set_xlabel("fração de uniões nas operações")
        axes[0].set_ylabel("tempo por operação (ns)")
        axes[0].set_title("(a) proporção uniões/consultas ($n=10^6$)", loc="left")
        axes[0].set_ylim(bottom=0)
    if d4 is not None:
        a = agrega(d4)
        a["grau"] = a["params"].map(lambda p: float(parametro(p, "avg_degree")))
        R("", "=" * 70, "E4 - componentes conexas G(n,m), n=1e6, ns/aresta", "=" * 70)
        R(a.pivot_table(index="impl", columns="grau", values="ns_per_op")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.1f}"))
        R("hops/find:")
        R(a.pivot_table(index="impl", columns="grau", values="hops_per_find")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.3f}"))
        R("writes/find:")
        R(a.pivot_table(index="impl", columns="grau", values="writes_per_find")
          .reindex([i for i in ORDEM if i in set(a["impl"])])
          .to_string(float_format=lambda v: f"{v:.3f}"))
        R("altura final:")
        R(a.pivot_table(index="impl", columns="grau", values="max_depth")
          .reindex([i for i in ORDEM if i in set(a["impl"])]).to_string())
        R("componentes / fração de uniões efetivas:")
        x = a[a["impl"] == "f-rank-compress"][["grau", "components", "links", "ops"]].copy()
        x["links/ops"] = x["links"] / x["ops"]
        R(x.to_string(index=False))
        linhas(axes[1], a, "grau", "ns_per_op", PRINCIPAIS)
        axes[1].set_xscale("log", base=2)
        axes[1].set_xticks([0.5, 1, 2, 4, 8, 16])
        axes[1].set_xticklabels(["0,5", "1", "2", "4", "8", "16"])
        axes[1].set_xlabel("grau médio $2m/n$")
        axes[1].set_title("(b) componentes conexas em $G(n,m)$", loc="left")
        axes[1].set_ylabel("tempo por aresta (ns)")
        axes[1].set_ylim(bottom=0)
    legenda_comum(fig, axes)
    fig.tight_layout()
    salvar(fig, "fig_e3_e4_carga")


# ---------------------------------------------------------------------------
# E5 e E6: grade e grafos reais (tabela principal) + métricas de máquina
# ---------------------------------------------------------------------------
NOMES_GRAFO = {"roadNet-CA.txt": "roadNet-CA", "web-Google.txt": "web-Google",
               "com-lj.ungraph.txt": "com-LiveJournal"}


def e5_e6():
    d5, d6 = ler("e5_grade.csv"), ler("e6_reais.csv")
    colunas = []   # (rótulo, dataframe agregado filtrado)
    if d5 is not None:
        a = agrega(d5)
        a["p"] = a["params"].map(lambda s: float(parametro(s, "p")))
        a["ordem"] = a["params"].map(lambda s: parametro(s, "order"))
        R("", "=" * 70, "E5 - grade 2000x2000 (ns/aresta)", "=" * 70)
        for ordem in ["random", "sequential"]:
            x = a[a["ordem"] == ordem]
            R(f"[ordem={ordem}] ns/op")
            R(x.pivot_table(index="impl", columns="p", values="ns_per_op")
              .reindex([i for i in ORDEM if i in set(x["impl"])])
              .to_string(float_format=lambda v: f"{v:.1f}"))
            R("hops/find")
            R(x.pivot_table(index="impl", columns="p", values="hops_per_find")
              .reindex([i for i in ORDEM if i in set(x["impl"])])
              .to_string(float_format=lambda v: f"{v:.3f}"))
            R("writes/find")
            R(x.pivot_table(index="impl", columns="p", values="writes_per_find")
              .reindex([i for i in ORDEM if i in set(x["impl"])])
              .to_string(float_format=lambda v: f"{v:.3f}"))
            R("altura")
            R(x.pivot_table(index="impl", columns="p", values="max_depth")
              .reindex([i for i in ORDEM if i in set(x["impl"])]).to_string())
        for ordem, rot in [("random", "aleat."), ("sequential", "varred.")]:
            colunas.append((f"Grade {rot}", "grade", a[(a["p"] == 0.5) & (a["ordem"] == ordem)]))
    if d6 is not None:
        a = agrega(d6)
        a["grafo"] = a["params"].map(lambda s: NOMES_GRAFO.get(parametro(s, "file"),
                                                               parametro(s, "file")))
        a["ordem"] = a["params"].map(lambda s: parametro(s, "order"))
        R("", "=" * 70, "E6 - grafos reais (ns/aresta)", "=" * 70)
        info = a[a["impl"] == "f-rank-compress"][["grafo", "ordem", "n", "ops", "components"]]
        R(info.to_string(index=False))
        for g in NOMES_GRAFO.values():
            x = a[a["grafo"] == g]
            if x.empty:
                continue
            R(f"[{g}]")
            for met, f in [("ns_per_op", "{:.2f}"), ("hops_per_find", "{:.3f}"),
                           ("writes_per_find", "{:.3f}"), ("max_depth", "{:.0f}"),
                           ("time_ms", "{:.1f}")]:
                R(f"  {met}")
                R(x.pivot_table(index="impl", columns="ordem", values=met)
                  .reindex([i for i in ORDEM if i in set(x["impl"])])
                  .to_string(float_format=lambda v, f=f: f.format(v)))
            for ordem, rot in [("sequential", "orig."), ("random", "aleat.")]:
                colunas.append((f"{g} {rot}", g, x[x["ordem"] == ordem]))
    if not colunas:
        return

    # Tabela de tempo (ns por aresta) com destaque para o menor de cada coluna.
    impls = [i for i in ORDEM]
    corpo = []
    melhores = {}
    for j, (_, _, x) in enumerate(colunas):
        v = x.set_index("impl")["ns_per_op"]
        melhores[j] = v.min()
    for impl in impls:
        cel = [tt(impl)]
        for j, (_, _, x) in enumerate(colunas):
            r = x[x["impl"] == impl]
            if r.empty:
                cel.append("")
                continue
            v = r["ns_per_op"].iloc[0]
            if not math.isfinite(v):
                cel.append("t.o.")
            elif abs(v - melhores[j]) < 1e-9:
                cel.append(r"\textbf{" + fmt_tempo(v) + "}")
            else:
                cel.append(fmt_tempo(v))
        corpo.append(" & ".join(cel) + r" \\")
        if impl in ("ll-weighted",):
            corpo.append(r"\midrule")
    # Cabeçalho: agrupa por instância
    grupos = []
    for rot, g, x in colunas:
        if not grupos or grupos[-1][0] != g:
            grupos.append([g, 0])
        grupos[-1][1] += 1
    nome_inst = {"grade": "Grade"}
    cab1, cmid, c0 = [], [], 2
    for g, k in grupos:
        cab1.append(rf"\multicolumn{{{k}}}{{c}}{{{nome_inst.get(g, g)}}}")
        cmid.append(rf"\cmidrule(lr){{{c0}-{c0 + k - 1}}}")
        c0 += k
    sub = []
    for rot, g, x in colunas:
        sub.append("aleat." if "aleat" in rot else ("varred." if "varred" in rot else "orig."))
    # Linha com n e m de cada instância
    tam = []
    for rot, g, x in colunas[::2]:
        r = x.iloc[0]
        tam.append(rf"\multicolumn{{2}}{{c}}{{\shortstack{{$n={fmt(r['n']/1e6, 2).replace(',', '{,}')}$\,M\\ $m={fmt(r['ops']/1e6, 1).replace(',', '{,}')}$\,M}}}}")
    tex = (r"""\begin{table}[ht]
\centering
\caption{Componentes conexas (E5, grade com $p=0{,}5$, e E6): tempo por aresta, em ns, na ordem
do arquivo (orig.), embaralhada (aleat.) ou em varredura (varred.). Em
negrito, o menor da coluna; t.o.\ = estourou 10\,s.}
\label{tab:e5e6}
\scriptsize
\setlength{\tabcolsep}{5pt}
\begin{tabular}{@{}l""" + "r" * len(colunas) + r"""@{}}
\toprule
 & """ + " & ".join(cab1) + r""" \\
 & """ + " & ".join(tam) + r""" \\
""" + "".join(cmid) + "\n" + r"""\textbf{Variante} & """ + " & ".join(sub) + r""" \\
\midrule
""" + "\n".join(corpo) + r"""
\bottomrule
\end{tabular}
\end{table}
""")
    (OUT / "tab_e5_e6_componentes.tex").write_text(tex)
    print("tabela: tab_e5_e6_componentes")


# ---------------------------------------------------------------------------
# Tabela de métricas independentes de máquina (hops/find, writes/find)
# ---------------------------------------------------------------------------
def metricas():
    fontes = []
    d2 = ler("e2_escalabilidade.csv")
    if d2 is not None:
        a = agrega(d2)
        fontes.append(("random $n{=}10^7$", a[a["n"] == a["n"].max()]))
    d5 = ler("e5_grade.csv")
    if d5 is not None:
        a = agrega(d5)
        a = a[a["params"].str.contains("p=0.5") & a["params"].str.contains("order=sequential")]
        fontes.append(("grade varred.", a))
    d6 = ler("e6_reais.csv")
    if d6 is not None:
        a = agrega(d6)
        a = a[a["params"].str.contains("com-lj") & a["params"].str.contains("order=random")]
        if not a.empty:
            fontes.append(("LiveJournal aleat.", a))
    if not fontes:
        return
    impls = ["ll-weighted", "f-rank", "f-size", "f-compress", "f-rank-compress",
             "f-size-compress", "f-rank-halving", "f-rank-splitting"]
    corpo = []
    for impl in impls:
        cel = [tt(impl)]
        for _, a in fontes:
            r = a[a["impl"] == impl]
            if r.empty:
                cel += ["--"] * 3
                continue
            r = r.iloc[0]
            cel += [fmt(r["hops_per_find"], 3), fmt(r["writes_per_find"], 3),
                    fmt(r["max_depth"], 0)]
        corpo.append(" & ".join(cel) + r" \\")
    cab = " & ".join(rf"\multicolumn{{3}}{{c}}{{{nome}}}" for nome, _ in fontes)
    cmid = "".join(rf"\cmidrule(lr){{{2 + 3 * i}-{4 + 3 * i}}}" for i in range(len(fontes)))
    sub = " & ".join(["hops & writes & $h$"] * len(fontes))
    tex = (r"""\begin{table}[ht]
\centering
\caption{Métricas independentes de máquina: arestas percorridas por busca
(hops), ponteiros reescritos por busca (writes) e altura final $h$ da
floresta. Nas listas, cada busca custa 1 e writes conta os reapontamentos.}
\label{tab:metricas}
\scriptsize
\setlength{\tabcolsep}{3.5pt}
\begin{tabular}{@{}l""" + "rrr" * len(fontes) + r"""@{}}
\toprule
 & """ + cab + r""" \\
""" + cmid + "\n" + r"""\textbf{Variante} & """ + sub + r""" \\
\midrule
""" + "\n".join(corpo) + r"""
\bottomrule
\end{tabular}
\end{table}
""")
    (OUT / "tab_metricas.tex").write_text(tex)
    print("tabela: tab_metricas")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    amb = RES / "ambiente.txt"
    if amb.exists():
        R("AMBIENTE", amb.read_text())
    e1()
    e2()
    e3_e4()
    e5_e6()
    metricas()
    (RES / "resumo.txt").write_text("\n".join(resumo) + "\n")
    print(f"resumo: {RES / 'resumo.txt'}")


if __name__ == "__main__":
    main()
