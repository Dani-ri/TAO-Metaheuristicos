# -*- coding: utf-8 -*-
"""
estructura.py -- Compara la ESTRUCTURA de las soluciones con la de la optima.

Para cada instancia de calibracion ejecuta el voraz (criterios 2 y 3), la
busqueda local y las cinco metaheuristicas (configuracion final, semilla 1) con
la opcion ver=1, que hace que el programa en C escriba las columnas de la
solucion. Cuenta cuantas columnas tiene cada solucion y cuantas de ellas estan
tambien en la solucion optima (fichero optimos_soluciones.csv).
Escribe resultados_estructura.csv. Es un analisis, no interviene en ninguna
decision de calibracion.

Uso:   python3 estructura.py
"""
import csv, json, os, subprocess
from concurrent.futures import ThreadPoolExecutor
from comun import *

def columnas_solucion(inst, alg, config):
    """Ejecuta el programa con ver=1 y devuelve (coste, conjunto de columnas de la solucion)."""
    orden = [EJECUTABLE, os.path.join(DIR_CALIBRACION, inst + '.dat'), alg,
             'tiempo=%s' % TIEMPO, 'semilla=1', 'ver=1']
    orden += ['%s=%s' % (p, v) for p, v in config.items()]
    coste, cols = None, None
    for linea in subprocess.run(orden, capture_output=True, text=True).stdout.splitlines():
        if linea.startswith('Solucion'):
            cols = set(int(x) for x in linea.split(':')[1].split())
        if linea.startswith('RESULTADO;'):
            coste = float(linea.split(';')[4])
    return coste, cols

def main():
    optimos = leer_optimos()
    sol_opt = {}
    with open(os.path.join(AQUI, 'optimos_soluciones.csv')) as f:
        for fila in csv.DictReader(f, delimiter=';'):
            sol_opt[fila['instancia']] = set(int(x) for x in fila['columnas_de_la_solucion_optima'].split())
    with open(os.path.join(AQUI, 'decisiones.json')) as f:
        decisiones = json.load(f)
    pruebas = [('voraz', 'criterio=2', {'criterio': 2}), ('voraz', 'criterio=3', {'criterio': 3}),
               ('local', 'entornos=3', {'entornos': 3})]
    pruebas += [(alg, 'final', d['final']) for alg, d in decisiones.items()]
    tareas = [(inst, alg, etiqueta, config) for alg, etiqueta, config in pruebas
              for inst in instancias(DIR_CALIBRACION)]
    def una(t):
        inst, alg, etiqueta, config = t
        coste, cols = columnas_solucion(inst, alg, config)
        return '%s;%s;%s;%.4f;%.4f;%d;%d' % (alg, etiqueta, inst, coste,
               100.0 * (coste - optimos[inst]) / optimos[inst], len(cols), len(cols & sol_opt[inst]))
    with ThreadPoolExecutor(max_workers=PROCESOS) as grupo:
        lineas = list(grupo.map(una, tareas))
    with open(os.path.join(AQUI, 'resultados_estructura.csv'), 'w') as f:
        f.write('algoritmo;config;instancia;coste;rpd;columnas;comunes_con_optima\n')
        f.write('\n'.join(lineas) + '\n')
    print('Hecho: resultados_estructura.csv')

if __name__ == '__main__':
    main()
