# -*- coding: utf-8 -*-
"""
comun.py -- Funciones compartidas por los guiones de experimentos.

Estos guiones NO forman parte de los algoritmos (que estan integramente en C,
en la carpeta codigo/). Solo sirven para lanzar muchas veces el ejecutable
"scp" con distintos parametros, guardar lo que devuelve y hacer medias.
"""
import csv, os, subprocess, threading
from concurrent.futures import ThreadPoolExecutor

AQUI      = os.path.dirname(os.path.abspath(__file__))        # carpeta experimentos/
RAIZ      = os.path.dirname(AQUI)                             # carpeta de la practica
EJECUTABLE = os.path.join(RAIZ, 'codigo', 'scp')              # programa en C ya compilado
DIR_CALIBRACION = os.path.join(RAIZ, 'datos')                 # 6 instancias para calibrar
DIR_VALIDACION  = os.path.join(RAIZ, 'Validacion_datos')      # 24 instancias para validar
TIEMPO   = 5.0                                                # segundos de CPU por ejecucion
SEMILLAS = [1, 2, 3]                                          # semillas de cada configuracion
PROCESOS = int(os.environ.get('PROCESOS', '2'))               # ejecuciones simultaneas

CAMPOS = ['algoritmo', 'etapa', 'config', 'instancia', 'semilla', 'coste', 'columnas',
          't_mejor', 't_total', 'iteraciones', 'factible', 'rpd']

def leer_optimos():
    """Devuelve un diccionario instancia -> valor optimo (fichero optimos.csv)."""
    opt = {}
    with open(os.path.join(AQUI, 'optimos.csv')) as f:
        for fila in csv.DictReader(f, delimiter=';'):
            opt[fila['instancia']] = float(fila['optimo'])
    return opt

def instancias(carpeta):
    """Lista ordenada de los nombres (sin .dat) de las instancias de una carpeta."""
    return sorted(x[:-4] for x in os.listdir(carpeta) if x.endswith('.dat'))

def texto_config(config):
    """Convierte {'k': 2, 'alfa': 0.95} en el texto 'alfa=0.95 k=2' (orden alfabetico)."""
    return ' '.join('%s=%s' % (p, config[p]) for p in sorted(config))

def ejecutar(carpeta, inst, algoritmo, config, semilla):
    """Lanza una ejecucion del programa en C y devuelve la linea RESULTADO troceada."""
    orden = [EJECUTABLE, os.path.join(carpeta, inst + '.dat'), algoritmo,
             'tiempo=%s' % TIEMPO, 'semilla=%d' % semilla]
    orden += ['%s=%s' % (p, v) for p, v in config.items()]
    salida = subprocess.run(orden, capture_output=True, text=True).stdout
    for linea in salida.splitlines():
        if linea.startswith('RESULTADO;'):
            c = linea.split(';')
            return dict(coste=float(c[4]), columnas=int(c[5]), t_mejor=float(c[6]),
                        t_total=float(c[7]), iteraciones=int(c[8]), factible=int(c[9]))
    raise RuntimeError('La ejecucion ha fallado: ' + ' '.join(orden))

class Registro:
    """Fichero CSV de resultados. Permite REANUDAR: lo ya ejecutado no se repite."""
    def __init__(self, nombre):
        self.ruta = os.path.join(AQUI, nombre)
        self.filas = []
        self.hechas = {}
        self.cerrojo = threading.Lock()
        if os.path.exists(self.ruta):
            with open(self.ruta) as f:
                for fila in csv.DictReader(f, delimiter=';'):
                    self.filas.append(fila)
                    self.hechas[self.clave(fila)] = fila
        else:
            with open(self.ruta, 'w') as f:
                f.write(';'.join(CAMPOS) + '\n')

    @staticmethod
    def clave(fila):
        return (fila['algoritmo'], fila['config'], fila['instancia'], str(fila['semilla']))

    def lanzar(self, carpeta, tareas, optimos):
        """tareas: lista de (algoritmo, etapa, config, instancia, semilla). Ejecuta las que falten."""
        def una(t):
            alg, etapa, config, inst, sem = t
            fila = dict(algoritmo=alg, etapa=etapa, config=texto_config(config),
                        instancia=inst, semilla=sem)
            if self.clave(fila) in self.hechas:
                return
            r = ejecutar(carpeta, inst, alg, config, sem)
            fila.update(r)
            fila['rpd'] = '%.4f' % (100.0 * (r['coste'] - optimos[inst]) / optimos[inst])
            with self.cerrojo:
                with open(self.ruta, 'a') as f:
                    f.write(';'.join(str(fila[c]) for c in CAMPOS) + '\n')
                self.filas.append({c: str(fila[c]) for c in CAMPOS})
                self.hechas[self.clave(fila)] = fila
        with ThreadPoolExecutor(max_workers=PROCESOS) as grupo:
            list(grupo.map(una, tareas))

    def rpd_medio(self, alg, config, lista_inst):
        """RPD medio (%) de una configuracion sobre las instancias y semillas indicadas."""
        tc = texto_config(config)
        v = [float(self.hechas[(alg, tc, i, str(s))]['rpd']) for i in lista_inst for s in SEMILLAS]
        return sum(v) / len(v)
