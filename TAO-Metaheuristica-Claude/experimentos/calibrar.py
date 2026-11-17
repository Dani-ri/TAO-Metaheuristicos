# -*- coding: utf-8 -*-
"""
calibrar.py -- Calibracion de los parametros de las cinco metaheuristicas.

Metodo: "un factor cada vez". Para cada algoritmo se parte de una configuracion
base razonable y se recorren sus parametros en un orden fijo. En cada etapa se
prueban varios valores de UN parametro (dejando los demas como estan), se
ejecuta cada valor sobre las 6 instancias de datos/ con 3 semillas, y se fija
el valor con menor RPD medio antes de pasar al parametro siguiente.

SEGUNDA VUELTA (opcional). Como el mejor valor de un parametro puede depender
del valor de los demas, se puede dar una segunda vuelta: se repasan otra vez
todos los parametros, ahora con los demas en sus valores ya elegidos. Se activa
con la variable de entorno VUELTAS (ver mas abajo) y duplica el tiempo de
calculo. En ella se aplica ademas la regla de ampliacion de la tabla AMPLIA.

Solo se usan las instancias de datos/. Las de Validacion_datos/ NO se tocan.

Uso:   python3 calibrar.py          (se puede interrumpir y volver a lanzar)
"""
import json, os
from comun import *

# Numero de vueltas (repasos de todos los parametros). Por defecto 1. Para dar
# tambien la segunda vuelta:   VUELTAS=2 python3 calibrar.py
VUELTAS = int(os.environ.get('VUELTAS', '1'))

# REGLA DE AMPLIACION (solo en la segunda vuelta). Si el valor vigente de un
# parametro es el mas pequeno (o el mas grande) de su lista, puede que el mejor
# valor este todavia mas alla. En ese caso se anade a la lista UN valor mas en
# esa direccion. Para cada parametro: (valor extra por abajo, valor extra por
# arriba); None = no tiene sentido ampliar en esa direccion.
AMPLIA = {
    'temple':   {'k': (None, 8), 'lrc': (None, 10), 'p0': (0.005, 0.8), 'alfa': (0.8, 0.995), 'L': (50, 20000)},
    'tabu':     {'tent': (2, 100), 'tsal': (0, 20), 'beta': (None, 0.5), 'reinicio': (100, None)},
    'grasp':    {'lrc': (None, 20), 'lamf': (0.0, None), 'elite': (2, 40)},
    'genetico': {'pob': (10, 200), 'nmut': (None, 10), 'bl': (None, 3)},
    'vns':      {'jb': (1, 40), 'lrc': (None, 10)},
}

def lista_de_valores(alg, param, valores, vigente, vuelta):
    """Devuelve la lista de valores a probar, ampliada si procede (ver AMPLIA)."""
    valores = list(valores)
    if vuelta >= 2 and param in AMPLIA.get(alg, {}):
        abajo, arriba = AMPLIA[alg][param]
        if vigente == min(valores) and abajo is not None:
            valores = [abajo] + valores
        elif vigente == max(valores) and arriba is not None:
            valores = valores + [arriba]
    return valores

# Para cada algoritmo: configuracion base y etapas (parametro, valores a probar)
PLAN = {
    'temple': dict(
        base={'k': 2, 'lrc': 2, 'p0': 0.5, 'alfa': 0.95, 'L': 1000},
        etapas=[('k', [1, 2, 3, 5]), ('lrc', [1, 2, 3, 5]), ('p0', [0.02, 0.1, 0.3, 0.5]), ('alfa', [0.9, 0.95, 0.99]),
                ('L', [200, 1000, 5000])]),
    'tabu': dict(
        base={'tent': 10, 'tsal': 3, 'beta': 0.01, 'reinicio': 2000},
        etapas=[('tent', [5, 10, 20, 50]), ('tsal', [1, 3, 5, 10]),
                ('beta', [0, 0.01, 0.05, 0.2]), ('reinicio', [500, 2000, 1000000])]),
    'grasp': dict(
        base={'lrc': 3, 'lamf': 0.5, 'elite': 10, 'grent': 3},
        etapas=[('lrc', [2, 3, 5, 10]), ('lamf', [1.0, 0.8, 0.5, 0.2]),
                ('elite', [5, 10, 20]), ('grent', [2, 3])]),
    'genetico': dict(
        base={'pob': 50, 'cruce': 2, 'pmut': 0.2, 'nmut': 2, 'bl': 0},
        etapas=[('cruce', [1, 2, 3]), ('pob', [20, 50, 100]), ('pmut', [0, 0.2, 0.5, 1.0]),
                ('nmut', [1, 2, 5]), ('bl', [0, 2])]),
    'vns': dict(
        base={'jb': 5, 'ja': 3, 'lrc': 2},
        etapas=[('ja', [1, 2, 3]), ('jb', [2, 5, 10, 20]), ('lrc', [1, 2, 3, 5])]),
}

def main():
    optimos = leer_optimos()
    inst = instancias(DIR_CALIBRACION)
    reg = Registro('resultados_calibracion.csv')
    decisiones = {}
    for alg, plan in PLAN.items():
        actual = dict(plan['base'])                       # configuracion vigente
        decisiones[alg] = dict(base=dict(plan['base']), etapas=[])
        for vuelta in range(1, VUELTAS + 1):
            cambios = 0                                   # parametros que cambian en esta vuelta
            for n_etapa, (param, valores) in enumerate(plan['etapas'], start=1):
                valores = lista_de_valores(alg, param, valores, actual[param], vuelta)
                configs = []
                for v in valores:                         # una configuracion por valor
                    c = dict(actual); c[param] = v; configs.append(c)
                tareas = [(alg, '%d.%d' % (vuelta, n_etapa), c, i, s)
                          for c in configs for i in inst for s in SEMILLAS]
                reg.lanzar(DIR_CALIBRACION, tareas, optimos)   # ejecuta lo que falte
                medias = [reg.rpd_medio(alg, c, inst) for c in configs]
                # gana el menor RPD medio; en caso de empate se conserva el valor vigente
                mejor = min(range(len(valores)),
                            key=lambda t: (round(medias[t], 6), valores[t] != actual[param]))
                decisiones[alg]['etapas'].append(dict(
                    vuelta=vuelta, parametro=param, valores=valores,
                    rpd=[round(x, 4) for x in medias],
                    anterior=actual[param], elegido=valores[mejor]))
                if valores[mejor] != actual[param]:
                    cambios += 1
                actual[param] = valores[mejor]            # se fija el mejor valor
                print('%-9s vuelta %d etapa %d  %-9s' % (alg, vuelta, n_etapa, param),
                      '  '.join('%s: %.3f' % (v, x) for v, x in zip(valores, medias)),
                      ' -> elegido', valores[mejor], flush=True)
            if vuelta > 1 and cambios == 0:               # vuelta sin cambios: configuracion estable
                break
        decisiones[alg]['final'] = actual
        decisiones[alg]['rpd_final'] = round(reg.rpd_medio(alg, actual, inst), 4)
        with open(os.path.join(AQUI, 'decisiones.json'), 'w') as f:
            json.dump(decisiones, f, indent=1)
    print('Calibracion terminada. Configuraciones finales en decisiones.json')

if __name__ == '__main__':
    main()
