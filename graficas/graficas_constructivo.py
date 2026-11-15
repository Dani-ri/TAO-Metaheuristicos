from pathlib import Path
import re
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def leer_tabla_configuraciones(lineas, titulo):
    inicio = next((i for i, linea in enumerate(lineas) if linea.strip() == titulo), None)
    if inicio is None:
        raise ValueError(f"No se encuentra la tabla: {titulo}")
    nombres = None
    for linea in lineas[inicio + 1:]:
        partes = linea.split()
        if not partes or set(linea.strip()) == {"-"}:
            continue
        if partes[0] == "Archivo":
            nombres = re.findall(r"(?:First|Best)_11(?:2)?", linea)
            continue
        if nombres is not None and len(partes) == len(nombres) + 1:
            try:
                valores = [float(x) for x in partes[1:]]
            except ValueError:
                continue
            return dict(zip(nombres, valores))
    raise ValueError(f"No se han podido leer los valores de: {titulo}")


def leer_promedios(lineas, titulo, nombres):
    inicio = next((i for i, linea in enumerate(lineas) if linea.startswith(titulo)), None)
    if inicio is None:
        raise ValueError(f"No se encuentra la tabla: {titulo}")
    encontrados = {}
    for linea in lineas[inicio + 1:]:
        partes = linea.split()
        if len(partes) == 3 and partes[0] in nombres:
            encontrados[partes[0]] = (float(partes[1]), float(partes[2]))
        if len(encontrados) == len(nombres):
            return [(nombre, *encontrados[nombre]) for nombre in nombres]
    raise ValueError(f"La tabla {titulo} esta incompleta")


def leer_datos(ruta):
    lineas = Path(ruta).read_text(encoding="utf-8-sig").splitlines()
    costes = leer_tabla_configuraciones(lineas, "VALOR OBJETIVO DE LOS ALGORITMOS")
    tiempos = leer_tabla_configuraciones(lineas, "TIEMPOS DE CALCULO (segundos)")
    if costes.keys() != tiempos.keys():
        raise ValueError("Las configuraciones de costes y tiempos no coinciden")
    return {
        "CONFIG": [(nombre, coste, tiempos[nombre]) for nombre, coste in costes.items()],
        "ESTRATEGIA": leer_promedios(
            lineas, "PROMEDIOS POR ESTRATEGIA DE BUSQUEDA LOCAL", ["First", "Best"]
        ),
        "VECINDARIO": leer_promedios(
            lineas, "PROMEDIOS POR VECINDARIO", ["1-1", "1-1+1-2"]
        ),
    }


def dibujar(grupo, indicador, titulo, nombre_archivo, destino):
    etiquetas = [x[0].replace("_", "\n") for x in grupo]
    valores = [x[indicador] for x in grupo]
    fig, ax = plt.subplots(figsize=(max(7, len(grupo) * 0.9), 4.8))
    posiciones = list(range(len(grupo)))
    barras = ax.bar(posiciones, valores, width=0.68)
    ax.set_xticks(posiciones, etiquetas)
    ax.tick_params(axis="x", labelsize=8)
    ax.set_ylabel("Coste final" if indicador == 1 else "Tiempo total (s)")
    ax.set_title(titulo)
    ax.grid(axis="y", alpha=0.25)
    ax.set_axisbelow(True)
    ax.set_ylim(bottom=0)
    for barra, valor in zip(barras, valores):
        texto = f"{valor:.3f}" if indicador == 1 else f"{valor:.4f}"
        ax.annotate(texto, (barra.get_x() + barra.get_width() / 2, barra.get_height()),
                    xytext=(0, 3), textcoords="offset points",
                    ha="center", va="bottom", fontsize=8)
    fig.tight_layout()
    fig.savefig(destino / nombre_archivo, dpi=220, bbox_inches="tight")
    plt.close(fig)


def main():
    if len(sys.argv) > 1:
        ruta = Path(sys.argv[1])
    else:
        ruta = Path(__file__).resolve().parent.parent / "resultados" / "resultados.txt"
    datos = leer_datos(ruta)
    destino = ruta.resolve().parent / "graficas_scp"
    destino.mkdir(exist_ok=True)
    vistas = [
        ("CONFIG", 1, "Coste final por configuracion", "01_costes_4_configuraciones.png"),
        ("CONFIG", 2, "Tiempo por configuracion", "02_tiempos_4_configuraciones.png"),
        ("ESTRATEGIA", 1, "Coste medio por estrategia de busqueda local", "04_coste_medio_estrategia.png"),
        ("VECINDARIO", 1, "Coste medio por tipo de vecindario", "05_coste_medio_vecindario.png"),
        ("ESTRATEGIA", 2, "Tiempo medio por estrategia de busqueda local", "06_tiempo_medio_estrategia.png"),
        ("VECINDARIO", 2, "Tiempo medio por tipo de vecindario", "07_tiempo_medio_vecindario.png"),
    ]
    for tipo, indicador, titulo, archivo in vistas:
        dibujar(datos[tipo], indicador, titulo, archivo, destino)
    print(f"Se han creado {len(vistas)} graficas PNG en {destino}")


if __name__ == "__main__":
    main()
