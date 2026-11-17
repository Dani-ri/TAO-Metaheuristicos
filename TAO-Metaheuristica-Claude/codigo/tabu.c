/* ==========================================================================
 *  tabu.c  --  Busqueda Tabu (Tabu Search)
 * --------------------------------------------------------------------------
 *  Idea: es una busqueda local que NO se detiene al llegar a un optimo local.
 *  En cada iteracion se ejecuta el MEJOR movimiento disponible aunque empeore
 *  la solucion. Para no deshacer enseguida lo que se acaba de hacer (y entrar
 *  en un ciclo), los movimientos recientes quedan prohibidos ("tabu") durante
 *  unas cuantas iteraciones: es la MEMORIA A CORTO PLAZO.
 *
 *  Esquema de los apuntes ("Esquema de un algoritmo de busqueda tabu"):
 *
 *     Inicializacion:  s en X;  s* = s;  k = 0;  T = vacio;  A(z)
 *     do
 *         M(s,k) = { m en M / m no es tabu  o  f(s+m) < A(f(s)) }
 *         V*     = { s' / s' = s+m, m en M(s,k) }
 *         Se elige la mejor solucion s' de V*
 *         If f(s') < f(s*)  entonces  s* = s'
 *         Actualizar T y A(.)
 *         k = k + 1;  s = s'
 *     while (condicion de parada)
 *
 *  Adaptacion al problema de cubrimiento:
 *    - MOVIMIENTOS. Hay de dos tipos, para poder avanzar en los dos sentidos:
 *        * ENTRADA de la columna k: entra k y salen las columnas que k vuelve
 *          redundantes (una o varias). Es el entorno Delta_3 de la busqueda
 *          local. Valor v(m) = c(k) - (costes de las que salen).
 *        * SALIDA de la columna j: sale j y las filas que solo cubria ella se
 *          vuelven a cubrir con el voraz (entran una o varias columnas).
 *          Valor v(m) = (costes de las que entran) - c(j).
 *      Si v(m) es negativo la solucion mejora; si es positivo, empeora.
 *      En cada iteracion se valoran TODOS los movimientos y se ejecuta el de
 *      menor valor que no este prohibido, aunque sea positivo.
 *    - MEMORIA A CORTO PLAZO (dos listas tabu):
 *        * una columna que SALE no puede volver a ENTRAR durante un cierto
 *          numero de iteraciones (impide deshacer el movimiento);
 *        * una columna que ENTRA no puede SALIR durante un cierto numero de
 *          iteraciones (impide que la echen nada mas llegar).
 *      La duracion se sortea en cada movimiento entre "ten" y "2*ten" (una
 *      duracion fija puede producir ciclos de longitud mayor que ella).
 *    - FUNCION DE ASPIRACION A(z) = f(s*): un movimiento tabu se permite si
 *      lleva a una solucion mejor que la mejor conocida.
 *    - MEMORIA A LARGO PLAZO (diversificacion): se cuenta cuantas veces ha
 *      entrado cada columna (frecuencia) y se penaliza el valor del movimiento
 *      con ts_beta * frecuencia, como en el ejemplo de los apuntes.
 *    - INTENSIFICACION: tras ts_reinicio iteraciones sin mejorar s*, la
 *      busqueda vuelve a s* (la mejor zona conocida) con las listas vacias.
 *    - Parada: limite de tiempo de CPU.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

/* Sortea una duracion tabu entre ten y 2*ten iteraciones. */
static int duracion(int ten)
{
    return ten + azar_entero(ten + 1);             /* ten + (entero al azar entre 0 y ten)          */
}

/* -------------------------------------------------------------------------- */
/*  Valor del movimiento "sale la columna j"                                  */
/* -------------------------------------------------------------------------- */
/*  Si j sale, sus filas unicas quedan sin cubrir y hay que volver a          */
/*  cubrirlas. Se hace con el voraz del cociente, pero mirando solo esas      */
/*  filas (son muy pocas) y sin usar columnas con prohibicion de entrar.      */
/*  Devuelve  valor = (costes de las columnas que entran) - c(j)  y deja en   */
/*  nuevas[0..nn-1] las columnas que entrarian. No modifica la solucion.      */
/*  Si las filas no se pueden cubrir con columnas permitidas devuelve 1e30.   */
static double valor_salida(Instancia *I, const Solucion *S, int j,
                           const long *tabu_ent, long iter, int *nuevas, int *nn)
{
    int    *cnt  = I->cnt;                         /* cnt[k] = filas pendientes que cubriria k      */
    int    *toc  = I->lis2;                        /* columnas con cnt > 0 (para volver a ponerlo a 0) */
    int     pend[MAX_EXP];                         /* filas pendientes de cubrir                    */
    int     np = 0, nt, p, q, i, k, t, mejor_c;    /* auxiliares                                    */
    double  suma = 0.0, punt, mejor_p;             /* coste acumulado y puntuaciones                */

    *nn = 0;                                       /* de momento no entra ninguna columna           */
    for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {  /* filas unicas de j = filas pendientes  */
        i = I->col_fil[p];
        if (S->cub[i] == 1 && np < MAX_EXP) {
            pend[np] = i;
            np++;
        }
    }
    while (np > 0) {                               /* mientras quede alguna fila pendiente          */
        nt = 0;                                    /* se cuentan, para cada columna candidata,      */
        for (t = 0; t < np; t++) {                 /* cuantas filas pendientes cubriria             */
            i = pend[t];
            for (q = I->fila_ini[i]; q < I->fila_ini[i + 1]; q++) {
                k = I->fila_col[q];
                if (k == j || tabu_ent[k] > iter) continue;   /* ni la que sale ni las tabu         */
                if (cnt[k] == 0) {                 /* primera vez que aparece k                     */
                    toc[nt] = k;
                    nt++;
                }
                cnt[k]++;
            }
        }
        mejor_c = -1;                              /* se elige la de mejor cociente filas/coste     */
        mejor_p = -1.0;
        for (t = 0; t < nt; t++) {
            k = toc[t];
            punt = (double)cnt[k] / (I->coste[k] + 1e-12);
            if (punt > mejor_p) {
                mejor_p = punt;
                mejor_c = k;
            }
            cnt[k] = 0;                            /* el contador queda a cero para la proxima vez  */
        }
        if (mejor_c < 0) {                         /* ninguna columna permitida cubre lo pendiente  */
            return 1e30;                           /* movimiento imposible                          */
        }
        suma += I->coste[mejor_c];                 /* entra la elegida                              */
        nuevas[*nn] = mejor_c;
        (*nn)++;
        q = 0;                                     /* se quitan de la lista las filas que cubre     */
        for (t = 0; t < np; t++) {
            if (!CUBRE(I, pend[t], mejor_c)) {
                pend[q] = pend[t];
                q++;
            }
        }
        np = q;
    }
    return suma - I->coste[j];                     /* variacion de coste del movimiento             */
}

/* Tras un movimiento pueden quedar columnas redundantes. Esta funcion quita  */
/* las que no esten protegidas (la mas cara primero); cada columna que sale   */
/* queda con prohibicion de volver a entrar.                                  */
static void limpiar(const Instancia *I, Solucion *S, const long *tabu_sal,
                    long *tabu_ent, long iter, int ten_ent)
{
    int t, j, peor;                                /* indices; peor = redundante mas cara           */
    for (;;) {                                     /* se repite hasta que no quede ninguna          */
        peor = -1;
        for (t = 0; t < S->ncols; t++) {           /* recorre las columnas de la solucion           */
            j = S->lista[t];
            if (tabu_sal[j] > iter) continue;      /* protegida: todavia no puede salir             */
            if ((peor < 0 || I->coste[j] > I->coste[peor]) && sol_redundante(I, S, j)) {
                peor = j;                          /* redundante mas cara encontrada                */
            }
        }
        if (peor < 0) break;                       /* no queda ninguna: fin                         */
        sol_quitar(I, S, peor);                    /* sale de la solucion                           */
        tabu_ent[peor] = iter + duracion(ten_ent); /* y no podra volver a entrar durante un tiempo  */
    }
}

Resultado busqueda_tabu(Instancia *I, Solucion *mejor, Parametros *P)
{
    Solucion  s;                                   /* solucion actual                               */
    Resultado R;                                   /* estadisticas                                  */
    long     *tabu_ent = (long *)calloc((size_t)I->n, sizeof(long));  /* tabu_ent[j]: iteracion
                                                      hasta la cual la columna j no puede ENTRAR    */
    long     *tabu_sal = (long *)calloc((size_t)I->n, sizeof(long));  /* tabu_sal[j]: iteracion
                                                      hasta la cual la columna j no puede SALIR     */
    int      *frec = vector_int(I->n);             /* frec[j]: veces que la columna j ha entrado    */
    unsigned char *fija = vector_byte((size_t)I->n);   /* fija[j] = 1 si j no puede salir ahora     */
    int       exp[MAX_EXP];                        /* columnas que expulsa un movimiento de entrada */
    int       nue[MAX_EXP];                        /* columnas que mete un movimiento de salida     */
    int       ne, nn, k, t, j;                     /* auxiliares                                    */
    int       mejor_k, mejor_j;                    /* mejor entrada y mejor salida encontradas      */
    long      iter = 0, sin_mejora = 0;            /* contador de iteraciones y de estancamiento    */
    double    v, vp, mejor_vp;                     /* valor, valor penalizado y el mejor de ellos   */

    if (tabu_ent == NULL || tabu_sal == NULL) {    /* comprobacion de la reserva de memoria         */
        fprintf(stderr, "ERROR: memoria insuficiente\n");
        exit(1);
    }
    sol_crear(I, &s);                              /* memoria para la solucion actual               */
    R.iter = 0;
    R.t_mejor = 0.0;

    /* ---------------- Inicializacion: s en X, s* = s, T = vacio ----------- */
    constructivo_voraz(I, &s, 3);                  /* s = solucion del constructivo voraz           */
    sol_copiar(I, mejor, &s);                      /* s* = s                                        */
    R.t_mejor = tiempo_usado(P);
    anotar_mejora(P, mejor->z);
    /* (las listas tabu y las frecuencias ya estan a cero: T = vacio)        */

    /* ---------------- Proceso --------------------------------------------- */
    while (!tiempo_agotado(P)) {                   /* condicion de parada: tiempo de CPU            */
        iter++;                                    /* k = k + 1                                     */

        for (t = 0; t < s.ncols; t++) {            /* se marcan las columnas que no pueden salir    */
            j = s.lista[t];
            fija[j] = (unsigned char)(tabu_sal[j] > iter);
        }
        calcular_unicas(I, &s);                    /* filas de cobertura unica (para valorar)       */

        /* --- se examinan todos los movimientos y se elige el mejor -------- */
        /* movimientos de ENTRADA: entra k y salen las que vuelve redundantes */
        mejor_k  = -1;                             /* aun no hay entrada elegida                    */
        mejor_vp = 1e29;                           /* "infinito" (menor que el 1e30 de lo imposible) */
        for (k = 0; k < I->n; k++) {
            if (s.en[k]) continue;                 /* k ya esta dentro: no es un movimiento         */
            v = valor_insercion(I, &s, k, fija, exp, &ne);   /* v(m): variacion del coste           */
            if (ne == 0) continue;                 /* no expulsa a nadie: no es un movimiento       */
            if (tabu_ent[k] > iter                 /* si el movimiento es tabu...                   */
                && !(s.z + v < mejor->z - EPS)) {  /* ...y no cumple la aspiracion f(s+m) < f(s*)   */
                continue;                          /* ...no pertenece a M(s,k): se descarta         */
            }
            vp = v + P->ts_beta * (double)frec[k]; /* valor penalizado por frecuencia (largo plazo) */
            if (vp < mejor_vp) {                   /* es el mejor movimiento valido hasta ahora     */
                mejor_vp = vp;
                mejor_k  = k;
            }
        }

        /* movimientos de SALIDA: sale j y sus filas unicas se cubren de nuevo */
        mejor_j = -1;                              /* aun no hay salida elegida                     */
        for (t = 0; t < s.ncols; t++) {
            j = s.lista[t];
            v = valor_salida(I, &s, j, tabu_ent, iter, nue, &nn);   /* v(m): variacion del coste    */
            if (fija[j]                            /* si el movimiento es tabu (j entro hace poco)... */
                && !(s.z + v < mejor->z - EPS)) {  /* ...y no cumple la aspiracion...               */
                continue;                          /* ...se descarta                                */
            }
            vp = v;                                /* valor penalizado: se suma la frecuencia de    */
            for (k = 0; k < nn; k++) {             /* cada columna que entraria                     */
                vp += P->ts_beta * (double)frec[nue[k]];
            }
            if (vp < mejor_vp) {                   /* mejor que todo lo visto (entradas incluidas)  */
                mejor_vp = vp;
                mejor_j  = j;
            }
        }

        if (mejor_j < 0 && mejor_k >= 0) {         /* gana una entrada: se recuerda a quien expulsa */
            (void)valor_insercion(I, &s, mejor_k, fija, exp, &ne);
        }
        for (t = 0; t < s.ncols; t++) {            /* se borran las marcas de proteccion            */
            fija[s.lista[t]] = 0;
        }

        if (mejor_j < 0 && mejor_k < 0) {          /* caso extremo: no queda ningun movimiento valido */
            for (j = 0; j < I->n; j++) {           /* se vacian las dos listas tabu                 */
                tabu_ent[j] = 0;
                tabu_sal[j] = 0;
            }
            continue;                              /* se pasa a la iteracion siguiente              */
        }

        /* --- se ejecuta el movimiento elegido: s = s' --------------------- */
        if (mejor_j >= 0) {                        /* MOVIMIENTO DE SALIDA                          */
            (void)valor_salida(I, &s, mejor_j, tabu_ent, iter, nue, &nn);   /* que columnas entran  */
            sol_quitar(I, &s, mejor_j);            /* sale la columna elegida                       */
            tabu_ent[mejor_j] = iter + duracion(P->ts_ten_ent);   /* y no podra volver en un tiempo */
            for (t = 0; t < nn; t++) {             /* entran las que cubren sus filas               */
                sol_insertar(I, &s, nue[t]);
                tabu_sal[nue[t]] = iter + duracion(P->ts_ten_sal);   /* y no podran salir en un tiempo */
                frec[nue[t]]++;
            }
        } else {                                   /* MOVIMIENTO DE ENTRADA                         */
            sol_insertar(I, &s, mejor_k);          /* entra la columna elegida                      */
            tabu_sal[mejor_k] = iter + duracion(P->ts_ten_sal);   /* no podra salir en un tiempo    */
            frec[mejor_k]++;                       /* memoria a largo plazo: ha entrado una vez mas */
            for (t = 0; t < ne; t++) {             /* salen las que se han vuelto redundantes       */
                sol_quitar(I, &s, exp[t]);
                tabu_ent[exp[t]] = iter + duracion(P->ts_ten_ent);   /* y no podran volver en un tiempo */
            }
        }
        if (s.nsin > 0) {                          /* seguridad: si quedara alguna fila sin cubrir  */
            completar(I, &s, 3, 1, NULL, 1.0);     /* (columnas con muchisimas filas), se cubre     */
        }
        limpiar(I, &s, tabu_sal, tabu_ent, iter, P->ts_ten_ent);  /* fuera redundantes no protegidas */

        /* --- If f(s') < f(s*) entonces s* = s' ----------------------------- */
        if (s.z < mejor->z - EPS) {
            sol_copiar(I, mejor, &s);              /* nueva mejor solucion                          */
            quitar_redundantes(I, mejor);          /* (sin columnas protegidas que le sobren)       */
            R.t_mejor = tiempo_usado(P);
            anotar_mejora(P, mejor->z);
            sin_mejora = 0;                        /* se reinicia el contador de estancamiento      */
        } else {
            sin_mejora++;                          /* otra iteracion sin mejorar s*                 */
        }

        /* --- intensificacion: volver a la mejor solucion conocida --------- */
        if (sin_mejora >= P->ts_reinicio) {
            sol_copiar(I, &s, mejor);              /* s = s*                                        */
            for (j = 0; j < I->n; j++) {           /* listas tabu vacias (las frecuencias se conservan) */
                tabu_ent[j] = 0;
                tabu_sal[j] = 0;
            }
            sin_mejora = 0;
        }
    }

    R.iter = iter;                                 /* numero total de iteraciones                   */
    mejor->z = sol_coste_exacto(I, mejor);         /* coste final sin errores de redondeo           */
    sol_liberar(&s);                               /* se devuelve la memoria                        */
    free(tabu_ent);  free(tabu_sal);  free(frec);  free(fija);
    return R;
}
