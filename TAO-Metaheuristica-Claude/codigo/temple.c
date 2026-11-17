/* ==========================================================================
 *  temple.c  --  Temple Simulado (Simulated Annealing)
 * --------------------------------------------------------------------------
 *  Idea: una busqueda local se queda atrapada en el primer optimo local que
 *  encuentra. El temple simulado permite, CON CIERTA PROBABILIDAD, pasar a
 *  una solucion peor para poder salir de esas "hondonadas". Esa probabilidad
 *  es  exp(-delta/T), donde delta es el empeoramiento y T la "temperatura":
 *    - cuanto mayor es el empeoramiento, mas dificil es aceptarlo;
 *    - con T alta se acepta casi todo (exploracion); con T baja casi nada
 *      (se comporta como una busqueda local).
 *  La temperatura empieza alta y se va bajando poco a poco (enfriamiento).
 *
 *  Esquema de los apuntes ("Algoritmo general" del Temple Simulado):
 *
 *     Construir una solucion inicial x y poner x* = x
 *     Determinar la temperatura inicial T > 0
 *     Repetir {
 *         n = 0
 *         Repetir {
 *             Generar una solucion vecina y de V(x)
 *             Evaluar delta = C(y) - C(x)
 *             Si delta < 0:  x = y;  si C(x) < C(x*) poner x* = x
 *             Si delta >= 0: generar u en (0,1); si u < exp(-delta/T), x = y
 *             n = n + 1
 *         } mientras n <= N(T)
 *         Disminuir T
 *     } mientras no se alcance el criterio de parada
 *
 *  Adaptacion al problema de cubrimiento:
 *    - Solucion inicial: la del constructivo voraz.
 *    - Vecina de x: se obtiene con perturbar() -> se quitan sa_k columnas al
 *      azar, se reparan las filas descubiertas (voraz con LRC) y se eliminan
 *      redundantes. La vecina siempre es factible.
 *    - Temperatura inicial: se elige de modo que, al principio, un
 *      empeoramiento "tipico" se acepte con probabilidad sa_p0.
 *    - Enfriamiento geometrico: T <- sa_alfa * T (el ejemplo de los apuntes).
 *    - N(T) = sa_L vecinas en cada temperatura.
 *    - Parada: limite de tiempo de CPU. Si el sistema se "congela" antes
 *      (ya no acepta ningun cambio), se recalienta para aprovechar el tiempo.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

Resultado temple_simulado(Instancia *I, Solucion *mejor, Parametros *P)
{
    Solucion  x, y, tmp;                           /* x = actual, y = vecina, tmp para intercambiarlas */
    Resultado R;                                   /* estadisticas que se devuelven                 */
    double    T, T0, delta, suma, u;               /* temperatura, incremento de coste, etc.        */
    int       n, t, npos, aceptadas, frias;        /* contadores                                    */

    sol_crear(I, &x);                              /* memoria para la solucion actual               */
    sol_crear(I, &y);                              /* memoria para la solucion vecina               */
    R.iter = 0;                                    /* todavia no se ha generado ninguna vecina      */
    R.t_mejor = 0.0;                               /* instante de la mejor solucion                 */

    /* ---------------- solucion inicial ------------------------------------ */
    constructivo_voraz(I, &x, 3);                  /* x = solucion del voraz (criterio del cociente) */
    sol_copiar(I, mejor, &x);                      /* x* = x                                        */
    R.t_mejor = tiempo_usado(P);                   /* se anota cuando se encontro                   */
    anotar_mejora(P, mejor->z);                    /* traza (si esta activada)                      */

    /* ---------------- temperatura inicial --------------------------------- */
    /* Se generan 100 vecinas de la solucion inicial y se calcula el          */
    /* empeoramiento medio (media de los delta > 0). Se quiere que ese        */
    /* empeoramiento se acepte con probabilidad p0:                           */
    /*      exp(-media/T0) = p0   =>   T0 = -media / ln(p0).                   */
    suma = 0.0;                                    /* suma de los empeoramientos observados         */
    npos = 0;                                      /* cuantos empeoramientos se han observado       */
    for (t = 0; t < 100; t++) {                    /* 100 vecinas de prueba                         */
        sol_copiar(I, &y, &x);                     /* y = copia de x                                */
        perturbar(I, &y, P->sa_k, P->sa_lrc);      /* y pasa a ser una vecina de x                  */
        delta = y.z - x.z;                         /* diferencia de coste                           */
        if (delta > EPS) {                         /* solo cuentan los empeoramientos               */
            suma += delta;
            npos++;
        }
    }
    if (npos > 0) {                                /* caso normal: hubo empeoramientos              */
        T0 = -(suma / (double)npos) / log(P->sa_p0);   /* formula anterior (log = logaritmo neperiano) */
    } else {                                       /* caso raro: ninguna vecina empeoraba           */
        T0 = 1.0;                                  /* se toma un valor cualquiera positivo          */
    }
    T = T0;                                        /* la temperatura actual empieza en T0           */
    frias = 0;                                     /* temperaturas seguidas sin aceptar nada        */

    /* ---------------- bucle principal -------------------------------------- */
    while (!tiempo_agotado(P)) {                   /* criterio de parada: tiempo de CPU             */
        aceptadas = 0;                             /* cambios aceptados en esta temperatura         */
        for (n = 0; n < P->sa_L && !tiempo_agotado(P); n++) {  /* N(T) vecinas por temperatura      */
            sol_copiar(I, &y, &x);                 /* y = copia de la solucion actual               */
            perturbar(I, &y, P->sa_k, P->sa_lrc);  /* "Generar una solucion vecina y de V(x)"       */
            delta = y.z - x.z;                     /* "Evaluar delta = C(y) - C(x)"                 */
            R.iter++;                              /* una vecina mas generada                       */
            if (delta < -EPS) {                    /* la vecina es MEJOR: se acepta siempre         */
                tmp = x;  x = y;  y = tmp;         /* x = y (se intercambian en vez de copiar)      */
                aceptadas++;
                if (x.z < mejor->z - EPS) {        /* ademas mejora a la mejor conocida x*          */
                    sol_copiar(I, mejor, &x);      /* x* = x                                        */
                    R.t_mejor = tiempo_usado(P);
                    anotar_mejora(P, mejor->z);
                }
            } else {                               /* la vecina NO es mejor                         */
                u = azar_01();                     /* numero aleatorio u en [0,1)                   */
                if (u < exp(-delta / T)) {         /* se acepta con probabilidad exp(-delta/T)      */
                    tmp = x;  x = y;  y = tmp;     /* x = y                                         */
                    if (delta > EPS) aceptadas++;  /* solo se cuenta si es un cambio real de coste  */
                }
            }
        }
        T = P->sa_alfa * T;                        /* "Disminuir T": enfriamiento geometrico        */

        /* --- recalentamiento si el sistema se ha congelado ----------------- */
        if (aceptadas == 0) frias++;               /* otra temperatura sin aceptar ningun cambio    */
        else frias = 0;
        if (frias >= 5) {                          /* 5 temperaturas seguidas congelado:            */
            T = T0;                                /* se vuelve a la temperatura inicial            */
            sol_copiar(I, &x, mejor);              /* y se reanuda desde la mejor solucion x*       */
            frias = 0;
        }
    }

    mejor->z = sol_coste_exacto(I, mejor);         /* coste final sin errores de redondeo           */
    sol_liberar(&x);                               /* se devuelve la memoria usada                  */
    sol_liberar(&y);
    return R;
}
