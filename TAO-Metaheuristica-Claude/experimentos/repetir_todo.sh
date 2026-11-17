#!/bin/bash
# ============================================================================
#  repetir_todo.sh -- Repite TODOS los experimentos en este ordenador y
#  regenera las tablas, las figuras y el PDF de la memoria.
#
#  Uso (desde la carpeta experimentos/):     bash repetir_todo.sh
#
#  Que hace, por orden:
#    1. Compila el programa en C.
#    2. Guarda los resultados anteriores en una carpeta resultados_anteriores_...
#       (no borra nada) para que todo se vuelva a calcular en ESTE ordenador.
#    3. Ejecuta los algoritmos basicos, la calibracion, la validacion, las
#       curvas de convergencia y el analisis de estructura.
#    4. Genera tablas y figuras y compila la memoria.
#
#  Duracion orientativa: 2 horas de calculo repartidas entre los procesos que
#  se lancen a la vez (con 4 procesos, una media hora).
#  Si se interrumpe (Ctrl+C), NO vuelva a lanzar este guion (guardaria aparte
#  lo ya hecho): continue a mano con  python3 calibrar.py , python3 validar.py ,
#  etc., que siguen donde lo dejaron.
# ============================================================================
set -e                                             # detenerse si alguna orden falla
cd "$(dirname "$0")"                               # situarse en la carpeta experimentos/

# --- cuantas ejecuciones lanzar a la vez --------------------------------------
# En los Mac con procesador Apple solo se cuentan los nucleos de alto rendimiento,
# para que todas las ejecuciones corran a la misma velocidad.
if [ -z "$PROCESOS" ]; then
    PROCESOS=$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null || true)
    if [ -z "$PROCESOS" ]; then
        N=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
        PROCESOS=$(( N / 2 ))
    fi
    [ "$PROCESOS" -lt 1 ] && PROCESOS=1
fi
export PROCESOS
echo "Se lanzaran $PROCESOS ejecuciones a la vez."

# --- 1. compilar ---------------------------------------------------------------
( cd ../codigo && gcc -O2 -std=c99 -Wall -o scp base.c constructivo.c busqueda_local.c \
      temple.c tabu.c grasp.c genetico.c vns.c main.c -lm )

# --- 2. apartar los resultados anteriores ---------------------------------------
ANTERIORES="resultados_anteriores_$(date +%Y%m%d_%H%M%S)"
mkdir "$ANTERIORES"
for f in resultados_basicos.csv resultados_calibracion.csv resultados_validacion.csv \
         resultados_trazas.csv resultados_estructura.csv decisiones.json resumen.json \
         calibracion.log validacion.log; do
    [ -f "$f" ] && mv "$f" "$ANTERIORES/"
done

# --- 3. experimentos -----------------------------------------------------------
python3 basicos.py
python3 calibrar.py | tee calibracion.log          # VUELTAS=2 bash repetir_todo.sh para dos vueltas
python3 validar.py  | tee validacion.log
python3 trazas.py
python3 estructura.py

# --- 4. tablas, figuras y PDF --------------------------------------------------
python3 generar_tablas.py
if command -v pdflatex >/dev/null 2>&1; then
    ( cd ../memoria && pdflatex -interaction=nonstopmode memoria.tex >/dev/null \
                    && pdflatex -interaction=nonstopmode memoria.tex >/dev/null \
                    && echo "Memoria compilada: memoria/memoria.pdf" )
else
    echo "pdflatex no esta instalado: compile memoria/memoria.tex con su editor de LaTeX."
fi
echo "Hecho. IMPORTANTE: los valores por defecto de codigo/main.c son los de la calibracion"
echo "de referencia; si su calibracion elige otros (decisiones.json), actualicelos alli."
