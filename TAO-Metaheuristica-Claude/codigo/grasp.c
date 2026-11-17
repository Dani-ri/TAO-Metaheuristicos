/* ==========================================================================
 *  grasp.c  --  GRASP (Greedy Randomized Adaptive Search Procedure)
 * --------------------------------------------------------------------------
 *  Idea: repetir muchas veces dos fases y quedarse con la mejor solucion:
 *    1) FASE CONSTRUCTIVA voraz, aleatorizada y adaptativa: como el voraz,
 *       pero en cada paso no se toma siempre la mejor columna sino UNA AL AZAR
 *       entre las mejores (lista restringida de candidatos, LRC). Asi cada
 *       construccion da una solucion distinta.
 *    2) FASE DE BUSQUEDA LOCAL: se mejora la solucion construida.
 *
 *  Esquema de los apuntes ("Algoritmo general" de GRASP):
 *
 *     Repetir {
 *         Construir una solucion x de forma greedy, aleatorizada y adaptativa
 *         Aplicar una busqueda local a la solucion x
 *         Actualizar la mejor solucion conocida x*
 *         Actualizar la informacion relativa a la memoria
 *     } Mientras no se haya alcanzado el criterio de parada
 *
 *  MEMORIA ("guia inteligente de la construccion" de los apuntes):
 *    - Se guarda un CONJUNTO ELITE con las mejores soluciones distintas.
 *    - memoria[j] = fraccion de soluciones elite que contienen la columna j.
 *    - Las columnas se puntuan con   lambda*[voraz] + (1-lambda)*[memoria].
 *    - lambda empieza en gr_lam0 (grande: manda el criterio voraz, porque aun
 *      no se sabe nada) y baja linealmente con el tiempo hasta gr_lamf (la
 *      memoria pesa cada vez mas, porque cada vez se sabe mas).
 *  Parada: limite de tiempo de CPU.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

Resultado grasp(Instancia *I, Solucion *mejor, Parametros *P)
{
    Solucion  x;                                   /* solucion de la iteracion en curso             */
    Solucion *elite;                               /* vector con las soluciones del conjunto elite  */
    double   *memoria = vector_double(I->n);       /* memoria[j]: frecuencia de j en el elite       */
    Resultado R;                                   /* estadisticas                                  */
    int       ne = 0;                              /* numero de soluciones que hay en el elite      */
    int       maxe = P->gr_elite;                  /* tamano maximo del conjunto elite              */
    int       e, t, peor, repetida;                /* auxiliares                                    */
    double    lambda;                              /* peso actual del criterio voraz                */

    if (maxe < 1) maxe = 1;                        /* al menos una solucion elite                   */
    elite = (Solucion *)calloc((size_t)maxe, sizeof(Solucion));   /* reserva del vector de soluciones */
    if (elite == NULL) {                           /* comprobacion de la reserva                    */
        fprintf(stderr, "ERROR: memoria insuficiente\n");
        exit(1);
    }
    for (e = 0; e < maxe; e++) {                   /* memoria para cada solucion elite              */
        sol_crear(I, &elite[e]);
    }
    sol_crear(I, &x);                              /* memoria para la solucion de trabajo           */
    R.iter = 0;
    R.t_mejor = 0.0;
    mejor->z = 1e30;                               /* aun no hay mejor solucion: coste "infinito"   */

    while (!tiempo_agotado(P) || R.iter == 0) {    /* criterio de parada (al menos una iteracion)   */

        /* --- valor actual de lambda (baja linealmente con el tiempo) ------ */
        lambda = P->gr_lam0 + (P->gr_lamf - P->gr_lam0) * (tiempo_usado(P) / P->tiempo);
        if (lambda < 0.0) lambda = 0.0;            /* lambda debe estar siempre entre 0 y 1         */
        if (lambda > 1.0) lambda = 1.0;

        /* --- fase 1: construccion voraz aleatorizada y adaptativa --------- */
        sol_vaciar(I, &x);                         /* se parte de cero                              */
        completar(I, &x, 3, P->gr_lrc,             /* criterio del cociente, LRC de tamano gr_lrc   */
                  ne > 0 ? memoria : NULL,         /* con memoria en cuanto exista algun elite      */
                  lambda);
        quitar_redundantes(I, &x);                 /* limpieza de columnas sobrantes                */

        /* --- fase 2: busqueda local --------------------------------------- */
        busqueda_local_vnd(I, &x, P->gr_entornos); /* descenso por entornos Delta_1..Delta_gr_entornos */
        R.iter++;                                  /* una iteracion GRASP mas                       */

        /* --- actualizar la mejor solucion conocida x* --------------------- */
        if (x.z < mejor->z - EPS) {
            sol_copiar(I, mejor, &x);
            R.t_mejor = tiempo_usado(P);
            anotar_mejora(P, mejor->z);
        }

        /* --- actualizar la memoria (conjunto elite) ----------------------- */
        repetida = 0;                              /* se comprueba si x ya esta en el elite         */
        peor = 0;                                  /* y, de paso, cual es la peor solucion elite    */
        for (e = 0; e < ne; e++) {
            if (fabs(elite[e].z - x.z) < 1e-7 && sol_iguales(&elite[e], &x)) {
                repetida = 1;                      /* misma solucion: no aporta diversidad          */
            }
            if (elite[e].z > elite[peor].z) {
                peor = e;                          /* la de mayor coste es la peor                  */
            }
        }
        if (!repetida) {                           /* solo entran soluciones nuevas                 */
            if (ne < maxe) {                       /* si aun hay sitio...                           */
                sol_copiar(I, &elite[ne], &x);     /* ...entra directamente                         */
                ne++;
            } else if (x.z < elite[peor].z - EPS) {/* si esta lleno, solo entra si mejora a la peor */
                sol_copiar(I, &elite[peor], &x);   /* y ocupa su lugar                              */
            } else {
                repetida = 1;                      /* no ha entrado: la memoria no cambia           */
            }
        }
        if (!repetida) {                           /* si el elite ha cambiado, se recalcula la memoria */
            for (t = 0; t < I->n; t++) {
                memoria[t] = 0.0;                  /* se pone a cero...                             */
            }
            for (e = 0; e < ne; e++) {             /* ...y cada solucion elite suma 1/ne a sus columnas */
                for (t = 0; t < elite[e].ncols; t++) {
                    memoria[elite[e].lista[t]] += 1.0 / (double)ne;
                }
            }
        }
    }

    mejor->z = sol_coste_exacto(I, mejor);         /* coste final sin errores de redondeo           */
    sol_liberar(&x);                               /* se devuelve toda la memoria                   */
    for (e = 0; e < maxe; e++) {
        sol_liberar(&elite[e]);
    }
    free(elite);
    free(memoria);
    return R;
}
