# -*- coding: utf-8 -*-
"""
validar.py -- Validacion de las configuraciones elegidas en la calibracion.

Lee decisiones.json (lo escribe calibrar.py) y ejecuta cada metaheuristica, con
su configuracion final y SIN TOCAR YA NINGUN PARAMETRO, sobre las 24 instancias
de Validacion_datos/ (3 semillas cada una). Escribe resultados_validacion.csv.

Uso:   python3 validar.py           (se puede interrumpir y volver a lanzar)
"""
import json, os
from comun import *

def main():
    optimos = leer_optimos()
    inst = instancias(DIR_VALIDACION)
    with open(os.path.join(AQUI, 'decisiones.json')) as f:
        decisiones = json.load(f)
    reg = Registro('resultados_validacion.csv')
    for alg, d in decisiones.items():
        config = d['final']
        tareas = [(alg, 0, config, i, s) for i in inst for s in SEMILLAS]
        reg.lanzar(DIR_VALIDACION, tareas, optimos)
        print('%-9s RPD medio en validacion: %.3f %%   (en calibracion: %.3f %%)'
              % (alg, reg.rpd_medio(alg, config, inst), d['rpd_final']), flush=True)

if __name__ == '__main__':
    main()
