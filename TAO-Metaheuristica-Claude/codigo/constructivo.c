/* ==========================================================================
 *  constructivo.c  --  Algoritmo constructivo voraz (greedy)
 * --------------------------------------------------------------------------
 *  Un algoritmo CONSTRUCTIVO crea una solucion "desde la nada", paso a paso,
 *  anadiendo en cada paso el elemento que parece mejor segun un criterio
 *  (apuntes: "Metodos Constructivos" / "Algoritmos voraces").
 *
 *  Para el problema de cubrimiento:
 *    1) Se empieza sin ninguna columna (todas las filas sin cubrir).
 *    2) Mientras quede alguna fila sin cubrir, se anade la columna de mejor
 *       puntuacion. Con el criterio 3 (el recomendado) es la columna que
 *       cubre mas filas NUEVAS por unidad de coste: maximiza gan[j] / c(j).
 *       Es el voraz clasico de Chvatal (1979) para el cubrimiento.
 *    3) Al final se eliminan las columnas redundantes: una columna elegida
 *       al principio puede haberse vuelto innecesaria por las posteriores.
 *
 *  El criterio es ADAPTATIVO: gan[j] se recalcula despues de cada insercion,
 *  porque las filas ya cubiertas dejan de contar.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

/* Construye en S una solucion factible con el voraz del criterio indicado. */
void constructivo_voraz(Instancia *I, Solucion *S, int criterio)
{
    sol_vaciar(I, S);                              /* paso 1: solucion vacia                        */
    completar(I, S, criterio, 1, NULL, 1.0);       /* paso 2: voraz puro (LRC de tamano 1, sin
                                                      memoria); termina cuando todo esta cubierto   */
    quitar_redundantes(I, S);                      /* paso 3: se quitan las columnas sobrantes      */
}
