# Práctica de metaheurísticas — Problema de Cubrimiento (SCP)

## Carpetas
- `codigo/` — programa en C comentado línea a línea (un ejecutable, `scp`, con los 7 algoritmos).
- `experimentos/` — guiones de Python para calibrar y validar, y sus resultados.
- `memoria/` — `memoria.pdf` y su fuente LaTeX (`memoria.tex`, `secciones/`, `tablas/`, `figuras/`).
- `datos/` — 6 instancias de **calibración**.  `Validacion_datos/` — 24 instancias de **validación**.

## Compilar y probar
```
cd codigo
bash compilar.sh                      # o: gcc -O2 -std=c99 -Wall -o scp *.c -lm
./scp ../datos/SCP01.dat vns          # algoritmos: voraz local temple tabu grasp genetico vns
```

## Repetir la calibración y la validación en este ordenador
```
cd experimentos
bash repetir_todo.sh                  # compila, calibra, valida, genera tablas/figuras y el PDF
```
Guarda los resultados anteriores en `resultados_anteriores_.../` (no borra nada).
Paso a paso, si se prefiere:
```
python3 basicos.py
python3 calibrar.py                   # PROCESOS=4 python3 calibrar.py  -> 4 ejecuciones a la vez
python3 validar.py                    # VUELTAS=2 python3 calibrar.py   -> segunda vuelta opcional
python3 trazas.py
python3 estructura.py
python3 generar_tablas.py
cd ../memoria && pdflatex memoria && pdflatex memoria
```
`calibrar.py` y `validar.py` reanudan donde lo dejaron: si ya existe el CSV de resultados no repiten
lo que contiene (para recalcular, apartar antes `resultados_calibracion.csv` y `resultados_validacion.csv`).

Todo está explicado en `memoria/memoria.pdf` (sección 15: uso; secciones 12–14: calibración y validación).
