# -*- coding: utf-8 -*-
"""
calcular_optimos.py -- Calcula el valor OPTIMO de cada instancia.

NO forma parte de los algoritmos de la practica. Solo sirve para tener una vara
de medir: el RPD de una solucion es su distancia (en %) a este valor optimo.

Resuelve el modelo de programacion entera
        min c'x   sujeto a   Ax >= 1,   x binaria
con el resolutor exacto HiGHS, a traves de la funcion milp de la biblioteca
SciPy (https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.milp.html).
Cada instancia tarda uno o dos segundos.

Uso:   python3 calcular_optimos.py        (escribe optimos.csv y optimos_soluciones.csv)
Necesita las bibliotecas numpy y scipy (version 1.9 o posterior).
"""
import os
import numpy as np
from scipy.optimize import milp, linprog, LinearConstraint, Bounds
from scipy.sparse import csr_matrix

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
CARPETAS = [os.path.join(RAIZ, 'datos'), os.path.join(RAIZ, 'Validacion_datos')]

def leer(fichero):
    """Lee un fichero SCPXX.dat y devuelve m, n, el vector de costes y la matriz A (dispersa)."""
    t = open(fichero).read().split()              # todos los numeros del fichero, uno tras otro
    m, n = int(t[0]), int(t[1])                   # numero de filas y de columnas
    c = np.array(t[2:2 + n], float)               # los n costes
    p = 2 + n                                     # posicion de lectura
    filas, columnas = [], []
    for i in range(m):                            # para cada fila...
        k = int(t[p]); p += 1                     # ...cuantas columnas la cubren
        for q in t[p:p + k]:                      # ...y cuales son (el fichero numera desde 1)
            filas.append(i); columnas.append(int(q) - 1)
        p += k
    A = csr_matrix((np.ones(len(filas)), (filas, columnas)), shape=(m, n))
    return m, n, c, A

def main():
    lineas = ['instancia;optimo;columnas;relajacion_lineal']
    soluciones = ['instancia;columnas_de_la_solucion_optima']
    for carpeta in CARPETAS:
        for nombre in sorted(os.listdir(carpeta)):
            if not nombre.endswith('.dat'):
                continue
            m, n, c, A = leer(os.path.join(carpeta, nombre))
            r = milp(c,                                                    # minimizar c'x
                     constraints=LinearConstraint(A, lb=np.ones(m), ub=np.inf),   # Ax >= 1
                     integrality=np.ones(n),                               # todas las variables enteras...
                     bounds=Bounds(0, 1),                                  # ...entre 0 y 1: binarias
                     options={'mip_rel_gap': 1e-9})                        # hasta optimalidad demostrada
            assert r.status == 0, 'HiGHS no ha demostrado la optimalidad de ' + nombre
            x = np.round(r.x)                                              # solucion optima (0/1)
            # Relajacion lineal (0 <= x <= 1 sin exigir enteros): es una cota inferior del optimo.
            # Solo se calcula como dato informativo sobre la dificultad de la instancia.
            rl = linprog(c, A_ub=-A, b_ub=-np.ones(m), bounds=(0, 1), method='highs')
            lineas.append('%s;%.4f;%d;%.4f' % (nombre[:-4], float(c @ x), int(x.sum()), rl.fun))
            soluciones.append('%s;%s' % (nombre[:-4],                    # columnas elegidas, numeradas desde 1
                              ' '.join(str(j + 1) for j in np.nonzero(x)[0])))
            print(lineas[-1], flush=True)
    with open(os.path.join(AQUI, 'optimos.csv'), 'w') as f:
        f.write('\n'.join(lineas) + '\n')
    with open(os.path.join(AQUI, 'optimos_soluciones.csv'), 'w') as f:
        f.write('\n'.join(soluciones) + '\n')

if __name__ == '__main__':
    main()
