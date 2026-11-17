# -*- coding: utf-8 -*-
"""
trazas.py -- Curvas de convergencia.

Ejecuta cada metaheuristica UNA vez, con su configuracion final (decisiones.json)
y la opcion traza=1, sobre una instancia de calibracion. Con traza=1 el programa
en C escribe una linea "TRAZA;tiempo;coste" cada vez que mejora la mejor
solucion. Se guardan en resultados_trazas.csv para dibujar como evoluciona la
calidad de la solucion con el tiempo.

Uso:   python3 trazas.py
"""
import json, os, subprocess
from comun import *

INSTANCIA = 'SCP01'
SEMILLA = 1

def main():
    optimo = leer_optimos()[INSTANCIA]
    with open(os.path.join(AQUI, 'decisiones.json')) as f:
        decisiones = json.load(f)
    with open(os.path.join(AQUI, 'resultados_trazas.csv'), 'w') as salida:
        salida.write('algoritmo;instancia;semilla;t;coste;rpd\n')
        for alg, d in decisiones.items():
            orden = [EJECUTABLE, os.path.join(DIR_CALIBRACION, INSTANCIA + '.dat'), alg,
                     'tiempo=%s' % TIEMPO, 'semilla=%d' % SEMILLA, 'traza=1']
            orden += ['%s=%s' % (p, v) for p, v in d['final'].items()]
            for linea in subprocess.run(orden, capture_output=True, text=True).stdout.splitlines():
                if linea.startswith('TRAZA;'):
                    _, t, z = linea.split(';')
                    salida.write('%s;%s;%d;%s;%s;%.4f\n' % (alg, INSTANCIA, SEMILLA, t, z,
                                 100.0 * (float(z) - optimo) / optimo))
    print('Hecho: resultados_trazas.csv')

if __name__ == '__main__':
    main()
