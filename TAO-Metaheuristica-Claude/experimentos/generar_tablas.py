# -*- coding: utf-8 -*-
"""
generar_tablas.py -- Convierte los ficheros de resultados en las tablas (.tex)
y las figuras (.pdf) que incluye la memoria.

Lee (de esta misma carpeta):
    optimos.csv, resultados_basicos.csv, resultados_calibracion.csv,
    decisiones.json, resultados_validacion.csv, resultados_trazas.csv
Escribe en ../memoria/tablas/ y ../memoria/figuras/.

Uso:   python3 generar_tablas.py
Necesita la biblioteca matplotlib (solo para las figuras).
"""
import csv, json, os, statistics
from collections import defaultdict

AQUI = os.path.dirname(os.path.abspath(__file__))
TABLAS = os.path.join(AQUI, '..', 'memoria', 'tablas')
FIGURAS = os.path.join(AQUI, '..', 'memoria', 'figuras')
os.makedirs(TABLAS, exist_ok=True)
os.makedirs(FIGURAS, exist_ok=True)

ALGORITMOS = ['temple', 'tabu', 'grasp', 'genetico', 'vns']
NOMBRE = {'voraz': 'Constructivo voraz', 'local': 'Búsqueda local', 'temple': 'Temple Simulado',
          'tabu': 'Búsqueda Tabú', 'grasp': 'GRASP', 'genetico': 'Algoritmo Genético', 'vns': 'VNS'}
CORTO = {'voraz': 'Voraz', 'local': 'B. local', 'temple': 'Temple', 'tabu': 'Tabú',
         'grasp': 'GRASP', 'genetico': 'Genético', 'vns': 'VNS'}

def leer(nombre):
    with open(os.path.join(AQUI, nombre), encoding='utf8') as f:
        return list(csv.DictReader(f, delimiter=';'))

def escribir(nombre, texto):
    with open(os.path.join(TABLAS, nombre), 'w', encoding='utf8') as f:
        f.write(texto)

def media(v):
    return sum(v) / len(v)

def miles(n):
    """1446 -> '1\\,446' (separador fino de millares para LaTeX)."""
    s = '%d' % n
    return s if n < 10000 else s[:-3] + '\\,' + s[-3:]

def tt(texto):
    """Texto en letra de maquina, protegiendo el guion bajo."""
    return '\\texttt{%s}' % str(texto).replace('_', '\\_')

def valor(v):
    """Escribe un valor de parametro de forma compacta (1000000 -> 10^6)."""
    if v == 1000000:
        return '$10^6$'
    return str(v)

def texto_config(config):
    return ' '.join('%s=%s' % (p, config[p]) for p in sorted(config))

# ----------------------------------------------------------------------------
optimos = {r['instancia']: float(r['optimo']) for r in leer('optimos.csv')}
basicos = leer('resultados_basicos.csv')
calib = leer('resultados_calibracion.csv')
valid = leer('resultados_validacion.csv') if os.path.exists(os.path.join(AQUI, 'resultados_validacion.csv')) else []
with open(os.path.join(AQUI, 'decisiones.json'), encoding='utf8') as f:
    decisiones = json.load(f)

# --- comprobaciones de coherencia -------------------------------------------
todas = basicos + calib + valid
assert all(r['factible'] == '1' for r in todas), 'hay alguna ejecucion no factible'
for r in todas:                                   # el RPD guardado coincide con el recalculado
    z, o = float(r['coste']), optimos[r['instancia']]
    assert abs(100 * (z - o) / o - float(r['rpd'])) < 1e-3
    assert z >= o - 1e-6, 'un heuristico no puede mejorar el optimo'
escribir('num_ejecuciones.tex', miles(len(todas)) + '%\n')
escribir('num_ejecuciones_cal.tex', miles(len(calib)) + '%\n')
escribir('num_ejecuciones_val.tex', miles(len(valid)) + '%\n')
escribir('num_configs.tex', '%d%%\n' % len(set((r['algoritmo'], r['config']) for r in calib)))

# ----------------------------------------------------------------------------
#  Tabla de los algoritmos basicos
# ----------------------------------------------------------------------------
def tabla_basicos():
    g = defaultdict(list)
    for r in basicos:
        g[(r['algoritmo'], r['config'], r['conjunto'])].append(r)
    filas = [('voraz', 'criterio=1', 'Voraz, criterio 1 (menor coste)'),
             ('voraz', 'criterio=2', 'Voraz, criterio 2 (mayor cobertura)'),
             ('voraz', 'criterio=3', 'Voraz, criterio 3 (cociente)'),
             ('local', 'entornos=1', 'Búsqueda local, $j_A=1$'),
             ('local', 'entornos=2', 'Búsqueda local, $j_A=2$'),
             ('local', 'entornos=3', 'Búsqueda local, $j_A=3$')]
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Algoritmos básicos (deterministas): RPD medio respecto del óptimo, número medio de '
         'columnas de la solución y tiempo medio, en las 6 instancias de calibración y en las 24 de '
         'validación.}\n\\label{tab:basicos}\n\\small\\setlength{\\tabcolsep}{4.5pt}\n'
         '\\begin{tabular}{@{}lrrrrrr@{}}\n\\toprule\n'
         ' & \\multicolumn{3}{c}{Calibración (6)} & \\multicolumn{3}{c}{Validación (24)} \\\\\n'
         '\\cmidrule(lr){2-4}\\cmidrule(l){5-7}\n'
         'Algoritmo & RPD (\\%) & Columnas & $t$ (s) & RPD (\\%) & Columnas & $t$ (s) \\\\ \\midrule\n']
    for alg, cfg, nombre in filas:
        celdas = []
        for conj in ('calibracion', 'validacion'):
            rs = g[(alg, cfg, conj)]
            celdas += ['%.2f' % media([float(r['rpd']) for r in rs]),
                       '%.0f' % media([int(r['columnas']) for r in rs]),
                       '%.3f' % media([float(r['t_total']) for r in rs])]
        t.append('%s & %s \\\\\n' % (nombre, ' & '.join(celdas)))
        if cfg == 'criterio=3':
            t.append('\\midrule\n')
    t.append('\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('basicos.tex', ''.join(t))

# ----------------------------------------------------------------------------
#  Tablas de calibracion (una por algoritmo) y tabla de configuraciones
# ----------------------------------------------------------------------------
def tabla_calibracion(alg):
    d = decisiones[alg]
    dos = any(e['vuelta'] == 2 for e in d['etapas'])          # hay segunda vuelta?
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Calibración de %s. Cada fila es una etapa: RPD medio (\\%%) sobre las 18 ejecuciones '
         '(6 instancias $\\times$ 3 semillas) de cada valor probado, con los demás parámetros fijos. En '
         'negrita, el mejor.}\n\\label{tab:cal-%s}\n\\small\n'
         '\\begin{tabular}{@{}cl>{\\raggedright\\arraybackslash}p{10.4cm}c@{}}\n\\toprule\n'
         '%s & Parámetro & Valor probado $\\rightarrow$ RPD medio & Elegido \\\\ \\midrule\n'
         % (NOMBRE[alg], alg, 'Vuelta' if dos else 'Etapa')]
    anterior = None
    for n, e in enumerate(d['etapas'], start=1):
        if anterior is not None and e['vuelta'] != anterior:
            t.append('\\midrule\n')
        anterior = e['vuelta']
        mejor = min(e['rpd'])
        trozos = []
        for v, x in zip(e['valores'], e['rpd']):
            num = '%.3f' % x
            if abs(x - mejor) < 5e-5:
                num = '\\textbf{%s}' % num
            trozos.append('%s\\,$\\rightarrow$\\,%s' % (valor(v), num))
        t.append('%d & %s & %s & %s \\\\\n' % (e['vuelta'] if dos else n, tt(e['parametro']),
                                              ';\\quad '.join(trozos), valor(e['elegido'])))
    t.append('\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('cal_%s.tex' % alg, ''.join(t))

def tabla_configuraciones():
    dos = any(e['vuelta'] == 2 for alg in ALGORITMOS for e in decisiones[alg]['etapas'])
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Configuración de partida (base) y configuración final de cada metaheurística, con su '
         'RPD medio (\\%) en las instancias de calibración. En negrita, los valores que la calibración ha '
         'cambiado.}\n\\label{tab:config}\n'
         '\\begin{tabular}{@{}ll' + ('ccc' if dos else 'cc') + '@{}}\n\\toprule\n'
         'Algoritmo & Parámetro & Base & ' + ('Tras la 1.ª vuelta & ' if dos else '') + 'Final \\\\ \\midrule\n']
    for alg in ALGORITMOS:
        d = decisiones[alg]
        tras1 = dict(d['base'])
        for e in d['etapas']:
            if e['vuelta'] == 1:
                tras1[e['parametro']] = e['elegido']
        params = [e['parametro'] for e in d['etapas'] if e['vuelta'] == 1]
        for n, p in enumerate(params):
            fin = valor(d['final'][p])
            if d['final'][p] != d['base'][p]:
                fin = '\\textbf{%s}' % fin
            celdas = [NOMBRE[alg] if n == 0 else '', tt(p), valor(d['base'][p])]
            celdas += ([valor(tras1[p])] if dos else []) + [fin]
            t.append(' & '.join(celdas) + ' \\\\\n')
        configs = (d['base'], tras1, d['final']) if dos else (d['base'], d['final'])
        t.append(' & \\emph{RPD medio} & ' + ' & '.join('\\emph{%.3f}' % rpd_config(alg, c) for c in configs)
                 + ' \\\\\n')
        t.append('\\midrule\n' if alg != ALGORITMOS[-1] else '')
    t.append('\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('config_final.tex', ''.join(t))

_rpd_cal = defaultdict(list)
for r in calib:
    _rpd_cal[(r['algoritmo'], r['config'])].append(float(r['rpd']))

def rpd_config(alg, config):
    v = _rpd_cal[(alg, texto_config(config))]
    assert len(v) == 18, (alg, config, len(v))
    return media(v)

# ----------------------------------------------------------------------------
#  Tablas de validacion
# ----------------------------------------------------------------------------
def filas_finales(alg, conjunto):
    """Ejecuciones de la configuracion final de alg en 'calibracion' o 'validacion'."""
    cfg = texto_config(decisiones[alg]['final'])
    datos = calib if conjunto == 'calibracion' else valid
    return [r for r in datos if r['algoritmo'] == alg and r['config'] == cfg]

def filas_basico(alg, conjunto):
    cfg = {'voraz': 'criterio=3', 'local': 'entornos=3'}[alg]
    return [r for r in basicos if r['algoritmo'] == alg and r['config'] == cfg and r['conjunto'] == conjunto]

def tabla_validacion_resumen():
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Calibración frente a validación. RPD (\\%) respecto del óptimo de la configuración '
         'final de cada algoritmo: media en las 6 instancias de calibración y media, desviación típica, '
         'mínimo y máximo sobre las ejecuciones en las 24 instancias de validación. Las dos últimas '
         'columnas son el número medio de columnas de la solución y el instante medio en que se '
         'encontró la mejor solución, en validación.}\n\\label{tab:validacion}\n'
         '\\begin{tabular}{@{}lrrrrrrr@{}}\n\\toprule\n'
         ' & Calibración & \\multicolumn{6}{c}{Validación} \\\\\n\\cmidrule(lr){2-2}\\cmidrule(l){3-8}\n'
         'Algoritmo & Media & Media & Desv. & Mín. & Máx. & Columnas & $t_{\\text{mejor}}$ (s) \\\\ \\midrule\n']
    resumen = {}
    for alg in ['voraz', 'local'] + ALGORITMOS:
        f = filas_basico if alg in ('voraz', 'local') else filas_finales
        c = [float(r['rpd']) for r in f(alg, 'calibracion')]
        v = f(alg, 'validacion')
        rv = [float(r['rpd']) for r in v]
        assert len(c) == (6 if alg in ('voraz', 'local') else 18)
        assert len(rv) == (24 if alg in ('voraz', 'local') else 72)
        tm = '---' if alg in ('voraz', 'local') else '%.2f' % media([float(r['t_mejor']) for r in v])
        resumen[alg] = dict(cal=media(c), val=media(rv))
        t.append('%s & %.3f & %.3f & %.3f & %.3f & %.3f & %.0f & %s \\\\\n' % (
            NOMBRE[alg] + (' ($j_A=3$)' if alg == 'local' else ''), media(c), media(rv),
            statistics.pstdev(rv), min(rv), max(rv), media([int(r['columnas']) for r in v]), tm))
        if alg == 'local':
            t.append('\\midrule\n')
    t.append('\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('validacion_resumen.tex', ''.join(t))
    with open(os.path.join(AQUI, 'resumen.json'), 'w') as f:
        json.dump(resumen, f, indent=1)
    return resumen

def tabla_validacion_instancias():
    algs = ['voraz', 'local'] + ALGORITMOS
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Validación, instancia a instancia: valor óptimo y RPD (\\%) de cada algoritmo (en las '
         'metaheurísticas, media de 3 semillas). En negrita, el mejor de cada fila.}\n'
         '\\label{tab:validacion-inst}\n\\small\n'
         '\\begin{tabular}{@{}lr' + 'r' * len(algs) + '@{}}\n\\toprule\n'
         'Instancia & Óptimo & ' + ' & '.join(CORTO[a] for a in algs) + ' \\\\ \\midrule\n']
    por = defaultdict(list)
    for alg in algs:
        f = filas_basico if alg in ('voraz', 'local') else filas_finales
        for r in f(alg, 'validacion'):
            por[(alg, r['instancia'])].append(float(r['rpd']))
    insts = sorted(set(r['instancia'] for r in valid))
    victorias = defaultdict(int)
    for i in insts:
        m = [media(por[(a, i)]) for a in algs]
        mejor = min(m)
        victorias[algs[m.index(mejor)]] += 1
        t.append('%s & %.4f & %s \\\\\n' % (i, optimos[i], ' & '.join(
            ('\\textbf{%.2f}' if abs(x - mejor) < 1e-9 else '%.2f') % x for x in m)))
    m = [media([media(por[(a, i)]) for i in insts]) for a in algs]
    t.append('\\midrule\nMedia & & %s \\\\\n' % ' & '.join('%.2f' % x for x in m))
    t.append('Veces mejor & & %s \\\\\n' % ' & '.join('%d' % victorias[a] for a in algs))
    t.append('\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('validacion_instancias.tex', ''.join(t))

def tabla_estructura():
    ruta = os.path.join(AQUI, 'resultados_estructura.csv')
    if not os.path.exists(ruta):
        return
    g = defaultdict(list)
    for r in leer('resultados_estructura.csv'):
        g[(r['algoritmo'], r['config'])].append(r)
    filas = [('voraz', 'criterio=2', 'Voraz, criterio 2 (mayor cobertura)'),
             ('voraz', 'criterio=3', 'Voraz, criterio 3 (cociente)'),
             ('local', 'entornos=3', 'Búsqueda local ($j_A=3$)')]
    filas += [(a, 'final', NOMBRE[a]) for a in ALGORITMOS]
    t = ['\\begin{table}[H]\n\\centering\n'
         '\\caption{Estructura de las soluciones en las 6 instancias de calibración (semilla 1): RPD, '
         'número de columnas y cuántas de ellas pertenecen también a la solución óptima, que tiene 50 '
         'columnas. Medias de las 6 instancias.}\n\\label{tab:estructura}\n'
         '\\begin{tabular}{@{}lrrr@{}}\n\\toprule\n'
         'Solución & RPD (\\%) & Columnas & Comunes con la óptima \\\\ \\midrule\n']
    for alg, cfg, nombre in filas:
        rs = g[(alg, cfg)]
        assert len(rs) == 6, (alg, cfg, len(rs))
        t.append('%s & %.2f & %.1f & %.1f \\\\\n' % (nombre, media([float(r['rpd']) for r in rs]),
                 media([int(r['columnas']) for r in rs]), media([int(r['comunes_con_optima']) for r in rs])))
        if cfg == 'entornos=3':
            t.append('\\midrule\n')
    t.append('Solución óptima & 0.00 & 50.0 & 50.0 \\\\\n\\bottomrule\n\\end{tabular}\n\\end{table}\n')
    escribir('estructura.tex', ''.join(t))

# ----------------------------------------------------------------------------
#  Figuras
# ----------------------------------------------------------------------------
AZUL, NARANJA, AGUA, AMARILLO, MAGENTA = '#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4'
TINTA, TINTA2, REJILLA = '#0b0b0b', '#52514e', '#e4e3df'

def estilo(plt):
    plt.rcParams.update({
        'font.size': 8.5, 'axes.titlesize': 9, 'axes.labelsize': 8.5, 'legend.fontsize': 8,
        'xtick.labelsize': 8, 'ytick.labelsize': 8, 'axes.edgecolor': TINTA2, 'axes.labelcolor': TINTA,
        'xtick.color': TINTA2, 'ytick.color': TINTA2, 'text.color': TINTA, 'axes.linewidth': 0.6,
        'axes.spines.top': False, 'axes.spines.right': False, 'axes.grid': True, 'grid.color': REJILLA,
        'grid.linewidth': 0.6, 'axes.axisbelow': True, 'legend.frameon': False,
        'figure.facecolor': 'white', 'axes.facecolor': 'white'})

def figura_calibracion(plt, alg):
    d = decisiones[alg]
    params = [e['parametro'] for e in d['etapas'] if e['vuelta'] == 1]
    dos_vueltas = any(e['vuelta'] == 2 for e in d['etapas'])
    ref = media([float(r['rpd']) for r in filas_basico('local', 'calibracion')])   # busqueda local
    fig, ejes = plt.subplots(1, len(params), figsize=(1.3 * len(params), 2.15), sharey=True)
    fig.subplots_adjust(left=0.55 / (1.3 * len(params)), right=0.995, bottom=0.21, top=0.76, wspace=0.10)
    for ax, p in zip(ejes, params):
        etapas = [e for e in d['etapas'] if e['parametro'] == p]
        todos = sorted(set(v for e in etapas for v in e['valores']))
        pos = {v: n for n, v in enumerate(todos)}
        ax.axhline(ref, color=TINTA2, linewidth=0.9, linestyle=(0, (4, 3)), label='Búsqueda local', zorder=2)
        nombres_series = ('1.ª vuelta', '2.ª vuelta') if dos_vueltas else ('Metaheurística', '')
        for e, color, marca, nombre in zip(etapas, (AZUL, NARANJA), ('o', 's'), nombres_series):
            ax.plot([pos[v] for v in e['valores']], e['rpd'], color=color, marker=marca, markersize=4.5,
                    linewidth=1.6, label=nombre, markeredgecolor='white', markeredgewidth=0.8, zorder=3)
        etiquetas = ['1e6' if v == 1000000 else ('%g' % v) for v in todos]
        ax.set_xticks(range(len(todos)))
        ax.set_xticklabels(etiquetas, fontsize=7, rotation=35 if max(len(x) for x in etiquetas) > 3 else 0)
        ax.set_xlim(-0.45, len(todos) - 0.55)
        ax.set_title(p, family='monospace')
        ax.grid(axis='x', visible=False)
    ejes[0].set_ylabel('RPD medio (%)')
    marcas, nombres = ejes[0].get_legend_handles_labels()
    orden = list(range(1, len(marcas))) + [0]             # las series primero, la referencia al final
    fig.legend([marcas[k] for k in orden], [nombres[k] for k in orden], loc='upper center', ncol=3,
               bbox_to_anchor=(0.5, 1.0), handlelength=2.2, columnspacing=1.6)
    fig.savefig(os.path.join(FIGURAS, 'cal_%s.pdf' % alg))
    plt.close(fig)

def figura_validacion(plt):
    algs = ['local'] + ALGORITMOS
    fig, ax = plt.subplots(figsize=(6.3, 2.9))
    for n, alg in enumerate(algs):
        f = filas_basico if alg == 'local' else filas_finales
        for conj, dx, color in (('calibracion', -0.19, AZUL), ('validacion', 0.19, NARANJA)):
            v = [float(r['rpd']) for r in f(alg, conj)]
            cj = ax.boxplot([v], positions=[n + dx], widths=0.3, patch_artist=True, showfliers=True,
                            medianprops=dict(color='white', linewidth=1.4),
                            boxprops=dict(facecolor=color, edgecolor=color, linewidth=0.8),
                            whiskerprops=dict(color=color, linewidth=1.0), capprops=dict(color=color, linewidth=1.0),
                            flierprops=dict(marker='o', markersize=2.5, markerfacecolor=color, markeredgecolor='none'))
    ax.set_xticks(range(len(algs)))
    ax.set_xticklabels([CORTO[a] for a in algs])
    ax.set_xlim(-0.6, len(algs) - 0.4)
    ax.set_ylabel('RPD respecto del óptimo (%)')
    ax.grid(axis='x', visible=False)
    from matplotlib.patches import Patch
    ax.legend(handles=[Patch(color=AZUL, label='Calibración (6 instancias)'),
                       Patch(color=NARANJA, label='Validación (24 instancias)')],
              loc='upper right', ncol=1)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURAS, 'validacion.pdf'))
    plt.close(fig)

def figura_convergencia(plt):
    ruta = os.path.join(AQUI, 'resultados_trazas.csv')
    if not os.path.exists(ruta):
        return
    tr = defaultdict(list)
    for r in leer('resultados_trazas.csv'):
        tr[r['algoritmo']].append((float(r['t']), float(r['rpd'])))
    fig, ax = plt.subplots(figsize=(6.3, 3.0))
    for alg, color in zip(ALGORITMOS, (AZUL, NARANJA, AGUA, AMARILLO, MAGENTA)):
        pts = sorted(tr[alg])
        xs = [max(t, 0.01) for t, _ in pts] + [5.0]
        ys = [y for _, y in pts] + [pts[-1][1]]
        ax.step(xs, ys, where='post', color=color, linewidth=1.8, label=CORTO[alg])
    ax.set_xscale('log')
    ax.set_xlim(0.01, 5.0)
    ax.set_xticks([0.01, 0.1, 1, 5])
    ax.set_xticklabels(['0.01', '0.1', '1', '5'])
    ax.set_xlabel('Tiempo de CPU (s, escala logarítmica)')
    ax.set_ylabel('RPD de la mejor solución (%)')
    ax.legend(loc='upper right', ncol=5, bbox_to_anchor=(1.0, 1.16), handlelength=1.4, columnspacing=1.0)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURAS, 'convergencia.pdf'))
    plt.close(fig)

# ----------------------------------------------------------------------------
def validacion_completa():
    """Cierto si estan las 72 ejecuciones de validacion de la configuracion final de cada algoritmo."""
    return all(len(filas_finales(alg, 'validacion')) == 72 for alg in ALGORITMOS)

PENDIENTE = ('\\begin{center}\\fbox{\\parbox{0.8\\textwidth}{\\centering\\textbf{Tabla pendiente.} Faltan resultados: '
             'ejecute \\texttt{%s} y después \\texttt{generar\\_tablas.py}.}}\\end{center}\n')

def main():
    tabla_basicos()
    for alg in ALGORITMOS:
        tabla_calibracion(alg)
    tabla_configuraciones()
    if validacion_completa():
        resumen = tabla_validacion_resumen()
        tabla_validacion_instancias()
    else:
        print('AVISO: la validacion no esta completa para la configuracion final; ejecute validar.py')
        resumen = None
        escribir('validacion_resumen.tex', PENDIENTE % 'validar.py')
        escribir('validacion_instancias.tex', PENDIENTE % 'validar.py')
    if os.path.exists(os.path.join(AQUI, 'resultados_estructura.csv')):
        tabla_estructura()
    else:
        escribir('estructura.tex', PENDIENTE % 'estructura.py')
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print('AVISO: matplotlib no esta instalado; se generan las tablas pero no las figuras')
    else:
        estilo(plt)
        for alg in ALGORITMOS:
            figura_calibracion(plt, alg)
        if resumen:
            figura_validacion(plt)
        figura_convergencia(plt)
    if resumen:
        for alg in ['voraz', 'local'] + ALGORITMOS:
            print('%-9s RPD calibracion %.3f   validacion %.3f' % (alg, resumen[alg]['cal'], resumen[alg]['val']))
    print('Tablas y figuras generadas.')

if __name__ == '__main__':
    main()
