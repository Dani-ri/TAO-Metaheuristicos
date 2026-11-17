#!/bin/bash
# ============================================================================
#  compilar.sh -- Compila el programa "scp" (los siete algoritmos en un solo
#  ejecutable).
#
#  Uso desde la terminal, dentro de la carpeta codigo/:     bash compilar.sh
#
#  Significado de las opciones del compilador:
#    -O2        optimizar el codigo (el programa va varias veces mas rapido)
#    -std=c99   usar el lenguaje C estandar de 1999
#    -Wall      mostrar todos los avisos
#    -o scp     nombre del ejecutable que se crea
#    -lm        enlazar la biblioteca matematica (funciones exp y log del temple)
#  En macOS la orden "gcc" es en realidad el compilador clang; sirve igual.
# ============================================================================
cd "$(dirname "$0")"                               # situarse en la carpeta codigo/
gcc -O2 -std=c99 -Wall -o scp base.c constructivo.c busqueda_local.c temple.c tabu.c \
    grasp.c genetico.c vns.c main.c -lm && echo "Compilado: ./scp"
