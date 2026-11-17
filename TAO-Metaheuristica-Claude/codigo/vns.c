/* ==========================================================================
 *  vns.c  --  Busqueda en Entorno Variable General (GVNS)
 * --------------------------------------------------------------------------
 *  Idea (Hansen y Mladenovic, 1997): un optimo local respecto de una
 *  estructura de entornos no tiene por que serlo respecto de otra. Por eso
 *  se CAMBIA de entorno de forma sistematica:
 *    - para MEJORAR la solucion se usa la busqueda descendente VND, que
 *      combina varios entornos Delta_1..Delta_jA (fichero busqueda_local.c);
 *    - para ESCAPAR del optimo local se "agita" la solucion con entornos de
 *      perturbacion Omega_1..Omega_jB cada vez mas amplios.
 *
 *  Esquema de las transparencias ("Busqueda en Entorno Variable General"):
 *
 *     1. Inicializacion:
 *        1.1 Construir una solucion inicial S y definir A para VND
 *        1.2 B = {Omega_1, ..., Omega_jB}
 *        1.3 n <- 1
 *     2. Repetir hasta n = n_max:
 *        2.1 j <- 1
 *        2.2 Perturbacion: elegir aleatoriamente S' en Omega_j(S)
 *        2.3 Realizar una VND a partir de S' y A  ->  S''
 *        2.4 Si z(S'') < z(S):  S = S'', ir al paso 2.1
 *        2.5 Si j < jB:  j = j + 1, ir al paso 2.2
 *        2.6 n <- n + 1
 *
 *  Adaptacion al problema de cubrimiento:
 *    - A = {Delta_1, ..., Delta_jA}: eliminacion, intercambio, insercion.
 *    - Omega_j(S): soluciones que se obtienen quitando j columnas al azar de
 *      S y reparando (funcion perturbar). Cuanto mayor es j, mas lejos de S.
 *    - n_max se sustituye por el limite de tiempo de CPU.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

Resultado vns(Instancia *I, Solucion *mejor, Parametros *P)
{
    Solucion  S, S2, tmp;                          /* S = actual; S2 = S' y luego S''; tmp auxiliar */
    Resultado R;                                   /* estadisticas                                  */
    int       j;                                   /* indice del entorno de perturbacion            */

    sol_crear(I, &S);                              /* memoria para las dos soluciones               */
    sol_crear(I, &S2);
    R.iter = 0;
    R.t_mejor = 0.0;

    /* ---------------- paso 1: inicializacion ------------------------------ */
    constructivo_voraz(I, &S, 3);                  /* 1.1 solucion inicial con el voraz...          */
    busqueda_local_vnd(I, &S, P->vns_ja);          /*     ...mejorada con el VND                    */
    R.t_mejor = tiempo_usado(P);
    anotar_mejora(P, S.z);

    /* ---------------- paso 2: bucle principal ----------------------------- */
    while (!tiempo_agotado(P)) {                   /* "repetir hasta n = n_max" (aqui: tiempo)      */
        j = 1;                                     /* 2.1: se empieza por la perturbacion mas suave */
        while (j <= P->vns_jb && !tiempo_agotado(P)) {
            sol_copiar(I, &S2, &S);                /* S' parte de una copia de S                    */
            perturbar(I, &S2, j, P->vns_lrc);      /* 2.2: S' = solucion al azar de Omega_j(S)      */
            busqueda_local_vnd(I, &S2, P->vns_ja); /* 2.3: VND desde S'; S2 pasa a ser S''          */
            R.iter++;                              /* una perturbacion + VND mas                    */
            if (S2.z < S.z - EPS) {                /* 2.4: si z(S'') < z(S)...                      */
                tmp = S;  S = S2;  S2 = tmp;       /*      S = S'' (intercambio en vez de copia)    */
                R.t_mejor = tiempo_usado(P);
                anotar_mejora(P, S.z);
                j = 1;                             /*      ...y se vuelve al paso 2.1               */
            } else {
                j = j + 1;                         /* 2.5: si no mejora, perturbacion mas fuerte    */
            }
        }                                          /* 2.6: agotados los jB entornos, otra vuelta    */
    }

    sol_copiar(I, mejor, &S);                      /* en VNS la solucion actual es siempre la mejor */
    mejor->z = sol_coste_exacto(I, mejor);         /* coste final sin errores de redondeo           */
    sol_liberar(&S);                               /* se devuelve la memoria                        */
    sol_liberar(&S2);
    return R;
}
