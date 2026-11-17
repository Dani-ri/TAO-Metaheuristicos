# -*- coding: utf-8 -*-
"""
basicos.py -- Ejecuta los algoritmos basicos (constructivo y busqueda local),
que son deterministas y casi instantaneos, sobre las 30 instancias:
  - voraz con los criterios 1 (coste), 2 (cobertura) y 3 (cociente);
  - voraz (criterio 3) + busqueda local con 1, 2 y 3 estructuras de entornos.
Escribe resultados_basicos.csv con el RPD respecto del optimo.
"""
import os
from comun import *

def main():
    optimos = leer_optimos()
    with open(os.path.join(AQUI, 'resultados_basicos.csv'), 'w') as f:
        f.write('conjunto;instancia;algoritmo;config;coste;columnas;t_total;factible;rpd\n')
        for conjunto, carpeta in (('calibracion', DIR_CALIBRACION), ('validacion', DIR_VALIDACION)):
            for inst in instancias(carpeta):
                pruebas = [('voraz', {'criterio': c}) for c in (1, 2, 3)]
                pruebas += [('local', {'entornos': e}) for e in (1, 2, 3)]
                for alg, config in pruebas:
                    r = ejecutar(carpeta, inst, alg, config, 1)
                    rpd = 100.0 * (r['coste'] - optimos[inst]) / optimos[inst]
                    f.write('%s;%s;%s;%s;%.4f;%d;%.3f;%d;%.4f\n' % (conjunto, inst, alg, texto_config(config),
                            r['coste'], r['columnas'], r['t_total'], r['factible'], rpd))
    print('Hecho: resultados_basicos.csv')

if __name__ == '__main__':
    main()
