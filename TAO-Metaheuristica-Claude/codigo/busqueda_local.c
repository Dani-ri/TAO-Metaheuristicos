/* ==========================================================================
 *  busqueda_local.c  --  Estructuras de entornos y busqueda local
 * --------------------------------------------------------------------------
 *  Una BUSQUEDA LOCAL parte de una solucion factible y la mejora poco a poco:
 *  en cada paso examina las soluciones "vecinas" (las que se obtienen con un
 *  pequeno cambio, llamado MOVIMIENTO) y se mueve a la mejor de ellas si es
 *  mejor que la actual. Termina cuando ninguna vecina mejora: la solucion es
 *  entonces un OPTIMO LOCAL respecto de esa estructura de entornos.
 *
 *  Se definen TRES estructuras de entornos (de la mas pequena y barata de
 *  explorar a la mas grande y cara). Todas mantienen la factibilidad:
 *
 *    Delta_1 (eliminacion):  quitar una columna redundante.
 *    Delta_2 (intercambio):  cambiar una columna de la solucion por otra de
 *                            fuera que sea mas barata y cubra todas las filas
 *                            que solo cubria la que sale.
 *    Delta_3 (insercion con expulsion): meter una columna de fuera y sacar
 *                            TODAS las columnas que, gracias a ella, se
 *                            vuelven redundantes (pueden ser 1, 2, 3...).
 *
 *  En los tres casos se aplica la estrategia del "mejor vecino": se evaluan
 *  todos los movimientos del entorno y se ejecuta el que mas abarata.
 *
 *  La combinacion de varios entornos se hace con la Busqueda Descendente por
 *  entornos (VND) de las transparencias de VNS.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

/* -------------------------------------------------------------------------- */
/*  Preparacion: filas cubiertas una sola vez                                 */
/* -------------------------------------------------------------------------- */
/*  Para cada fila cubierta por UNA sola columna de S se apunta cual es esa   */
/*  columna (su "duena"), y para cada columna de S se cuenta cuantas filas    */
/*  cubre ella sola (nunic). Una columna puede salir de la solucion solo si   */
/*  alguien cubre esas filas "unicas" en su lugar.                            */

void calcular_unicas(Instancia *I, const Solucion *S)
{
    int t, j, p, i;                                /* indices auxiliares                            */
    for (t = 0; t < S->ncols; t++) {               /* para cada columna j de la solucion...         */
        j = S->lista[t];
        I->nunic[j] = 0;                           /* ...de momento no tiene filas unicas           */
        for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {  /* para cada fila i que cubre j      */
            i = I->col_fil[p];
            if (S->cub[i] == 1) {                  /* si a i solo la cubre una columna (que es j)   */
                I->duena[i] = j;                   /* j es la duena de la fila i                    */
                I->nunic[j]++;                     /* j tiene una fila unica mas                    */
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Valor del movimiento "insertar la columna k"                              */
/* -------------------------------------------------------------------------- */
/*  Calcula cuanto cambiaria el coste si se inserta k (que no esta en S) y se */
/*  expulsan las columnas que k vuelve redundantes:                           */
/*            valor = c(k) - (suma de costes de las expulsadas).              */
/*  Valor negativo = la solucion mejora. No modifica la solucion.             */
/*  En expulsadas[0..nexp-1] devuelve las columnas que saldrian (como mucho   */
/*  MAX_EXP). Si "fija" no es NULL, las columnas con fija[j] = 1 no pueden    */
/*  ser expulsadas (lo usa la busqueda tabu para proteger columnas).          */
/*  REQUISITO: haber llamado antes a calcular_unicas(I, S).                   */
/*                                                                            */
/*  Idea: una columna j de S se vuelve redundante al entrar k si y solo si k  */
/*  cubre TODAS las filas unicas de j. Basta recorrer las filas de k, mirar   */
/*  cuales tienen duena y contar cuantas filas unicas de cada duena cubre k.  */

double valor_insercion(Instancia *I, Solucion *S, int k, const unsigned char *fija,
                       int *expulsadas, int *nexp)
{
    int   *cnt = I->cnt;                           /* cnt[j] = filas unicas de j cubiertas por k    */
    int   *toc = I->lis2;                          /* columnas "tocadas" (duenas de filas de k)     */
    int    nt = 0;                                 /* numero de columnas tocadas                    */
    int    ncan = 0;                               /* numero de columnas que se vuelven redundantes */
    int    p, q, i, j, t, u, x, ok;                /* indices auxiliares                            */
    double ahorro = 0.0;                           /* suma de costes de las columnas expulsadas     */

    *nexp = 0;                                     /* de momento no se expulsa ninguna              */
    for (p = I->col_ini[k]; p < I->col_ini[k + 1]; p++) {  /* para cada fila i que cubre k...       */
        i = I->col_fil[p];
        if (S->cub[i] != 1) continue;              /* ...solo interesan las de cobertura unica      */
        j = I->duena[i];                           /* j = columna duena de la fila i                */
        if (cnt[j] == 0) {                         /* primera vez que k toca a la columna j         */
            toc[nt] = j;                           /* se apunta en la lista de tocadas              */
            nt++;
        }
        cnt[j]++;                                  /* k cubre una fila unica mas de j               */
    }
    for (t = 0; t < nt; t++) {                     /* para cada columna tocada...                   */
        j = toc[t];
        if (cnt[j] == I->nunic[j] && ncan < MAX_EXP  /* si k cubre TODAS sus filas unicas...        */
            && (fija == NULL || !fija[j])) {       /* ...y j no esta protegida, j se vuelve redundante */
            toc[ncan] = j;                         /* se guarda al principio del mismo vector       */
            ncan++;
        }
        cnt[j] = 0;                                /* se deja el contador a cero para la proxima vez */
    }
    if (ncan == 0) {                               /* k no hace redundante a nadie:                 */
        return I->coste[k];                        /* insertar k solo anade su coste                */
    }
    if (ncan == 1) {                               /* caso mas frecuente: sale una sola columna     */
        expulsadas[0] = toc[0];
        *nexp = 1;
        return I->coste[k] - I->coste[toc[0]];     /* es un intercambio: entra k y sale esa columna */
    }

    /* --- dos o mas candidatas a salir: hay que simular con cuidado -------- */
    /* Dos columnas pueden ser redundantes por separado pero no a la vez (si  */
    /* comparten una fila que solo cubren ellas dos). Se ordenan de mas cara  */
    /* a mas barata y se van quitando solo las que SIGAN siendo redundantes.  */
    for (t = 1; t < ncan; t++) {                   /* ordenacion por insercion, coste decreciente   */
        x = toc[t];                                /* columna que se va a colocar                   */
        u = t;
        while (u > 0 && I->coste[toc[u - 1]] < I->coste[x]) {  /* desplaza las mas baratas          */
            toc[u] = toc[u - 1];
            u--;
        }
        toc[u] = x;                                /* coloca x en su sitio                          */
    }
    for (p = I->col_ini[k]; p < I->col_ini[k + 1]; p++) {  /* simula que k esta dentro:             */
        S->cub[I->col_fil[p]]++;                   /* sus filas tienen una cobertura mas            */
    }
    for (t = 0; t < ncan; t++) {                   /* para cada candidata, de mas cara a mas barata */
        j = toc[t];
        ok = 1;                                    /* se supone redundante hasta que se demuestre que no */
        for (q = I->col_ini[j]; q < I->col_ini[j + 1]; q++) {
            if (S->cub[I->col_fil[q]] < 2) {       /* alguna de sus filas se quedaria sin cubrir    */
                ok = 0;
                break;
            }
        }
        if (!ok) continue;                         /* ya no es redundante: se queda                 */
        for (q = I->col_ini[j]; q < I->col_ini[j + 1]; q++) {
            S->cub[I->col_fil[q]]--;               /* simula que j sale                             */
        }
        ahorro += I->coste[j];                     /* se ahorra su coste                            */
        expulsadas[*nexp] = j;                     /* se apunta como expulsada                      */
        (*nexp)++;
    }
    for (t = 0; t < *nexp; t++) {                  /* se DESHACE la simulacion: vuelven las expulsadas */
        j = expulsadas[t];
        for (q = I->col_ini[j]; q < I->col_ini[j + 1]; q++) {
            S->cub[I->col_fil[q]]++;
        }
    }
    for (p = I->col_ini[k]; p < I->col_ini[k + 1]; p++) {  /* y se retira k                         */
        S->cub[I->col_fil[p]]--;
    }
    return I->coste[k] - ahorro;                   /* variacion de coste del movimiento             */
}

/* -------------------------------------------------------------------------- */
/*  Mejor movimiento de Delta_2 (intercambio 1 por 1)                         */
/* -------------------------------------------------------------------------- */
/*  Para cada columna j de la solucion se buscan columnas k de fuera, mas     */
/*  baratas, que cubran todas las filas unicas de j. Como k debe cubrir, en   */
/*  particular, la primera fila unica de j, solo se prueban las columnas que  */
/*  cubren esa fila (unas 150 en lugar de las 10 000).                        */
/*  Ejecuta el mejor intercambio y devuelve 1; si no hay ninguno, devuelve 0. */

static int mover_intercambio(Instancia *I, Solucion *S)
{
    int   *unic = I->lis1;                         /* filas unicas de la columna j en estudio       */
    int    nu, t, p, i, j, k, u, r;                /* indices auxiliares                            */
    int    sale = -1, entra = -1;                  /* mejor intercambio encontrado                  */
    double mejor = EPS;                            /* mayor ahorro encontrado (debe superar EPS)    */
    double g;                                      /* ahorro del intercambio en estudio             */

    for (t = 0; t < S->ncols; t++) {               /* para cada columna j de la solucion...         */
        j = S->lista[t];
        if (I->coste[j] <= mejor) continue;        /* ni saliendo gratis ahorraria mas que el mejor */
        nu = 0;                                    /* se reunen sus filas unicas                    */
        for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {
            i = I->col_fil[p];
            if (S->cub[i] == 1) {
                unic[nu] = i;
                nu++;
            }
        }
        if (nu == 0) continue;                     /* j es redundante: eso es asunto de Delta_1     */
        r = unic[0];                               /* fila "pivote": la primera fila unica de j     */
        for (p = I->fila_ini[r]; p < I->fila_ini[r + 1]; p++) {  /* columnas k que cubren el pivote */
            k = I->fila_col[p];
            if (k == j) continue;                  /* la propia j no cuenta                         */
            g = I->coste[j] - I->coste[k];         /* ahorro si entra k y sale j                    */
            if (g <= mejor) continue;              /* no supera al mejor conocido                   */
            for (u = 1; u < nu; u++) {             /* comprueba que k cubre el resto de filas unicas */
                if (!CUBRE(I, unic[u], k)) break;  /* una sin cubrir: k no sirve                    */
            }
            if (u == nu) {                         /* k las cubre todas: intercambio factible       */
                mejor = g;                         /* se guarda como el mejor hasta ahora           */
                sale  = j;
                entra = k;
            }
        }
    }
    if (sale < 0) return 0;                        /* no hay intercambio que mejore                 */
    sol_quitar(I, S, sale);                        /* se ejecuta: sale la cara...                   */
    sol_insertar(I, S, entra);                     /* ...y entra la barata                          */
    return 1;                                      /* 1 = la solucion ha mejorado                   */
}

/* -------------------------------------------------------------------------- */
/*  Mejor movimiento de Delta_3 (insercion con expulsion)                     */
/* -------------------------------------------------------------------------- */
/*  Prueba a insertar cada una de las columnas de fuera y ejecuta la que mas  */
/*  abarata la solucion. Devuelve 1 si mejoro y 0 si no hay mejora posible.   */

static int mover_insercion(Instancia *I, Solucion *S)
{
    int    exp[MAX_EXP];                              /* columnas expulsadas por el movimiento         */
    int    ne, k, t;                               /* n. de expulsadas, columna e indice            */
    int    entra = -1;                             /* mejor columna a insertar                      */
    double mejor = -EPS;                           /* mejor valor (negativo = mejora)               */
    double v;                                      /* valor del movimiento en estudio               */

    calcular_unicas(I, S);                         /* prepara duena[] y nunic[]                     */
    for (k = 0; k < I->n; k++) {                   /* para cada columna k...                        */
        if (S->en[k]) continue;                    /* ...que no este ya en la solucion              */
        v = valor_insercion(I, S, k, NULL, exp, &ne);    /* variacion de coste si se inserta              */
        if (v < mejor) {                           /* mejor que el mejor conocido                   */
            mejor = v;
            entra = k;
        }
    }
    if (entra < 0) return 0;                       /* ningun movimiento mejora                      */
    (void)valor_insercion(I, S, entra, NULL, exp, &ne);  /* se recalcula que columnas expulsa la ganadora */
    sol_insertar(I, S, entra);                     /* entra la nueva                                */
    for (t = 0; t < ne; t++) {                     /* y salen las que ha vuelto redundantes         */
        sol_quitar(I, S, exp[t]);
    }
    return 1;                                      /* 1 = la solucion ha mejorado                   */
}

/* -------------------------------------------------------------------------- */
/*  Busqueda local en UNA estructura de entornos                              */
/* -------------------------------------------------------------------------- */
/*  Repite el mejor movimiento del entorno indicado hasta que ninguno mejora  */
/*  (optimo local). Devuelve 1 si la solucion ha mejorado y 0 si ya era un    */
/*  optimo local de ese entorno.                                              */

int descenso(Instancia *I, Solucion *S, int entorno)
{
    int mejorado = 0;                              /* 1 en cuanto se consiga alguna mejora          */
    if (entorno == 1) {                            /* Delta_1: quitar redundantes (la mas cara antes) */
        if (quitar_redundantes(I, S) > 0) mejorado = 1;
    } else if (entorno == 2) {                     /* Delta_2: intercambios                         */
        while (mover_intercambio(I, S)) mejorado = 1;   /* se repite mientras haya mejora           */
    } else {                                       /* Delta_3: insercion con expulsion              */
        while (mover_insercion(I, S)) mejorado = 1;     /* se repite mientras haya mejora           */
    }
    return mejorado;
}

/* -------------------------------------------------------------------------- */
/*  Busqueda Descendente por entornos (VND)                                   */
/* -------------------------------------------------------------------------- */
/*  Esquema de las transparencias ("Busqueda Descendente (VND)"):             */
/*    1. j <- 1                                                               */
/*    2. Repetir hasta que j > jA:                                            */
/*       2.1 Busqueda local en Delta_j partiendo de S  ->  S'                 */
/*       2.2 Si z(S') < z(S): S = S' y j = 1;  en otro caso: j = j + 1        */
/*    3. S es minimo local respecto de TODAS las estructuras Delta_1..Delta_jA */
/*  Con jA = 2 es la busqueda local basica (eliminacion + intercambio).       */

void busqueda_local_vnd(Instancia *I, Solucion *S, int jA)
{
    int j = 1;                                     /* paso 1: se empieza por el primer entorno      */
    while (j <= jA) {                              /* paso 2: hasta agotar los entornos             */
        if (descenso(I, S, j)) {                   /* 2.1: busqueda local en Delta_j (S se mejora "in situ") */
            j = 1;                                 /* 2.2: si mejoro, se vuelve al primer entorno   */
        } else {
            j = j + 1;                             /*      si no, se pasa al entorno siguiente      */
        }
    }
}
