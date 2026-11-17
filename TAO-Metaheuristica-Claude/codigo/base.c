/* ==========================================================================
 *  base.c  --  Herramientas comunes a todos los algoritmos
 * --------------------------------------------------------------------------
 *  Contiene:
 *    1. Generador de numeros aleatorios propio (reproducible).
 *    2. Medida del tiempo de CPU.
 *    3. Reserva de memoria.
 *    4. Lectura de la instancia desde el fichero SCPXX.dat.
 *    5. Operaciones elementales sobre una solucion (insertar/quitar columna).
 *    6. Eliminacion de columnas redundantes.
 *    7. "completar": cubre las filas sin cubrir (voraz, opcionalmente
 *       aleatorizado y con memoria). Es la pieza que comparten el algoritmo
 *       constructivo, GRASP, el temple, el genetico y la VNS.
 *    8. "perturbar": quita k columnas al azar y repara la solucion.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

/* ========================================================================== */
/*  1. GENERADOR DE NUMEROS ALEATORIOS                                        */
/* ========================================================================== */
/*  No se usa rand() de C porque da secuencias distintas en cada sistema      */
/*  operativo. Con un generador propio (tipo "xorshift" de 64 bits), la misma */
/*  semilla produce exactamente la misma ejecucion en cualquier ordenador,    */
/*  lo que hace los experimentos REPRODUCIBLES.                               */

static unsigned long long estado_azar = 88172645463325252ULL;  /* estado interno del generador */

/* Devuelve el siguiente numero entero de 64 bits de la secuencia. */
static unsigned long long azar_bruto(void)
{
    estado_azar ^= estado_azar << 13;              /* mezcla los bits desplazandolos a la izquierda */
    estado_azar ^= estado_azar >> 7;               /* los mezcla desplazandolos a la derecha        */
    estado_azar ^= estado_azar << 17;              /* y otra vez a la izquierda                     */
    return estado_azar;                            /* el nuevo estado es el numero generado         */
}

/* Fija la semilla: a semillas distintas, secuencias distintas. */
void azar_semilla(unsigned long semilla)
{
    int i;                                         /* contador del bucle de calentamiento           */
    estado_azar = (unsigned long long)semilla * 2685821657736338717ULL  /* dispersa la semilla...   */
                  + 1442695040888963407ULL;        /* ...para que semillas 1, 2, 3 den estados muy distintos */
    if (estado_azar == 0ULL) {                     /* el estado 0 esta prohibido (se quedaria en 0) */
        estado_azar = 88172645463325252ULL;        /* en ese caso se usa un valor fijo no nulo      */
    }
    for (i = 0; i < 20; i++) {                     /* se descartan los 20 primeros numeros...       */
        (void)azar_bruto();                        /* ...para "calentar" el generador               */
    }
}

/* Numero real uniforme en el intervalo [0,1). */
double azar_01(void)
{
    return (double)(azar_bruto() >> 11)            /* se toman los 53 bits mas altos (los mejores)  */
           / 9007199254740992.0;                   /* y se divide entre 2^53 para caer en [0,1)     */
}

/* Numero entero uniforme en {0, 1, ..., k-1}. */
int azar_entero(int k)
{
    return (int)(azar_01() * (double)k);           /* [0,1) * k = [0,k) y se trunca a entero        */
}

/* ========================================================================== */
/*  2. TIEMPO                                                                 */
/* ========================================================================== */

/* Segundos de CPU consumidos por el programa hasta este momento. */
double reloj(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;  /* clock() cuenta "tics"; se pasan a segundos */
}

/* Segundos transcurridos desde que arranco el algoritmo en curso. */
double tiempo_usado(const Parametros *P)
{
    return reloj() - P->t_ini;                     /* instante actual menos instante de comienzo    */
}

/* Devuelve 1 si ya se ha consumido el tiempo permitido (criterio de parada). */
int tiempo_agotado(const Parametros *P)
{
    return tiempo_usado(P) >= P->tiempo;           /* compara lo gastado con el limite              */
}

/* Si se pidio traza, escribe el instante y el coste de cada mejora. */
void anotar_mejora(const Parametros *P, double z)
{
    if (P->traza) {                                /* solo si el usuario activo la opcion traza=1   */
        printf("TRAZA;%.4f;%.4f\n", tiempo_usado(P), z);  /* tiempo ; coste de la nueva mejor sol.  */
    }
}

/* ========================================================================== */
/*  3. RESERVA DE MEMORIA                                                     */
/* ========================================================================== */
/*  calloc reserva memoria y la deja rellena de ceros. Si el sistema no tiene */
/*  memoria suficiente devuelve NULL; en ese caso se avisa y se termina.      */

/* Reserva "num" elementos de "tam" bytes cada uno, todos a cero. */
static void *reservar(size_t num, size_t tam)
{
    void *p = calloc(num > 0 ? num : 1, tam);      /* se pide al menos 1 elemento                   */
    if (p == NULL) {                               /* si no habia memoria...                        */
        fprintf(stderr, "ERROR: memoria insuficiente\n");  /* ...se avisa por la salida de errores  */
        exit(1);                                   /* ...y se termina el programa                   */
    }
    return p;                                      /* direccion del bloque de memoria reservado     */
}

int *vector_int(int n)                             /* vector de n numeros enteros                   */
{
    return (int *)reservar((size_t)n, sizeof(int));
}

double *vector_double(int n)                       /* vector de n numeros reales                    */
{
    return (double *)reservar((size_t)n, sizeof(double));
}

unsigned char *vector_byte(size_t n)               /* vector de n bytes (valores de 0 a 255)        */
{
    return (unsigned char *)reservar(n, sizeof(unsigned char));
}

/* ========================================================================== */
/*  4. LECTURA DE LA INSTANCIA                                                */
/* ========================================================================== */
/*  Formato del fichero (enunciado de la practica):                           */
/*    - primera linea: m (filas) y n (columnas);                              */
/*    - a continuacion los n costes;                                          */
/*    - para cada fila i: el numero de columnas que la cubren y, despues,     */
/*      los indices (empezando en 1) de esas columnas.                        */
/*  El fichero se lee DOS veces: la primera solo para contar cuantos unos     */
/*  tiene la matriz (y asi saber cuanta memoria reservar) y la segunda para   */
/*  guardar los datos. Devuelve 1 si todo fue bien y 0 si hubo algun error.   */

int leer_instancia(const char *nombre, Instancia *I)
{
    FILE  *f;                                      /* manejador del fichero                         */
    int    i, j, k, t, p;                          /* indices y contadores auxiliares               */
    double x;                                      /* para leer costes en la primera pasada         */
    int   *sig;                                    /* siguiente hueco libre de cada columna         */

    f = fopen(nombre, "r");                        /* abre el fichero en modo lectura ("r" = read)  */
    if (f == NULL) {                               /* si no se pudo abrir (no existe, mala ruta...) */
        fprintf(stderr, "ERROR: no se puede abrir el fichero %s\n", nombre);
        return 0;                                  /* se devuelve 0 = error                         */
    }

    /* ---------------- PRIMERA PASADA: contar los unos --------------------- */
    if (fscanf(f, "%d %d", &I->m, &I->n) != 2 || I->m <= 0 || I->n <= 0) {  /* lee m y n            */
        fprintf(stderr, "ERROR: cabecera incorrecta en %s\n", nombre);
        fclose(f);                                 /* cierra el fichero antes de salir              */
        return 0;
    }
    for (j = 0; j < I->n; j++) {                   /* salta los n costes (se leeran en la 2.a pasada) */
        if (fscanf(f, "%lf", &x) != 1) {           /* si falta algun coste...                       */
            fprintf(stderr, "ERROR: faltan costes en %s\n", nombre);
            fclose(f);
            return 0;
        }
    }
    I->nnz = 0;                                    /* contador de unos de la matriz                 */
    for (i = 0; i < I->m; i++) {                   /* para cada fila...                             */
        if (fscanf(f, "%d", &k) != 1 || k <= 0) {  /* ...lee cuantas columnas la cubren             */
            fprintf(stderr, "ERROR: la fila %d no se puede cubrir o esta mal escrita\n", i + 1);
            fclose(f);
            return 0;                              /* una fila sin columnas haria el problema infactible */
        }
        I->nnz += k;                               /* acumula el numero de unos                     */
        for (t = 0; t < k; t++) {                  /* salta los k indices de columna                */
            if (fscanf(f, "%d", &j) != 1 || j < 1 || j > I->n) {   /* comprobando que son validos   */
                fprintf(stderr, "ERROR: indice de columna incorrecto en la fila %d\n", i + 1);
                fclose(f);
                return 0;
            }
        }
    }

    /* ---------------- reserva de memoria ---------------------------------- */
    I->coste    = vector_double(I->n);             /* un coste por columna                          */
    I->fila_ini = vector_int(I->m + 1);            /* comienzo de cada fila (+1 para marcar el final) */
    I->fila_col = vector_int(I->nnz);              /* columnas de todas las filas, una tras otra    */
    I->col_ini  = vector_int(I->n + 1);            /* comienzo de cada columna (+1 para el final)   */
    I->col_fil  = vector_int(I->nnz);              /* filas de todas las columnas, una tras otra    */
    I->a        = vector_byte((size_t)I->m * (size_t)I->n);  /* matriz A completa (m x n bytes)     */
    I->gan      = vector_int(I->n);                /* vectores de trabajo (ver scp.h)               */
    I->cand     = vector_int(I->n);
    I->veto     = vector_byte((size_t)I->n);
    I->duena    = vector_int(I->m);
    I->nunic    = vector_int(I->n);
    I->cnt      = vector_int(I->n);
    I->lis1     = vector_int(I->n);
    I->lis2     = vector_int(I->n);

    /* ---------------- SEGUNDA PASADA: guardar los datos ------------------- */
    rewind(f);                                     /* vuelve al principio del fichero               */
    if (fscanf(f, "%d %d", &I->m, &I->n) != 2) {   /* relee m y n (ya se sabe que son correctos)    */
        fclose(f);
        return 0;
    }
    for (j = 0; j < I->n; j++) {                   /* lee y guarda los n costes                     */
        if (fscanf(f, "%lf", &I->coste[j]) != 1) {
            fclose(f);
            return 0;
        }
    }
    p = 0;                                         /* posicion de escritura dentro de fila_col      */
    for (i = 0; i < I->m; i++) {                   /* para cada fila...                             */
        I->fila_ini[i] = p;                        /* ...sus columnas empiezan en la posicion p     */
        if (fscanf(f, "%d", &k) != 1) {            /* numero de columnas que cubren la fila         */
            fclose(f);
            return 0;
        }
        for (t = 0; t < k; t++) {                  /* lee los k indices                             */
            if (fscanf(f, "%d", &j) != 1) {
                fclose(f);
                return 0;
            }
            j = j - 1;                             /* el fichero numera desde 1; el programa desde 0 */
            if (CUBRE(I, i, j)) {                  /* si esa columna ya estaba apuntada en la fila... */
                I->nnz--;                          /* ...es un dato repetido: se descuenta...       */
                continue;                          /* ...y se ignora                                */
            }
            I->fila_col[p] = j;                    /* guarda la columna en la lista de la fila      */
            p++;                                   /* avanza la posicion de escritura               */
            CUBRE(I, i, j) = 1;                    /* marca el 1 en la matriz completa              */
            I->col_ini[j + 1]++;                   /* de paso cuenta cuantas filas cubre la columna j */
        }
    }
    I->fila_ini[I->m] = p;                         /* marca el final de la ultima fila              */
    fclose(f);                                     /* ya no se necesita el fichero                  */

    /* ------- construir la lista "por columnas" a partir de la de filas ---- */
    for (j = 0; j < I->n; j++) {                   /* suma acumulada de los contadores: tras esto,  */
        I->col_ini[j + 1] += I->col_ini[j];        /* col_ini[j] = posicion donde empieza la columna j */
    }
    sig = vector_int(I->n);                        /* sig[j] = siguiente hueco libre de la columna j */
    for (j = 0; j < I->n; j++) {
        sig[j] = I->col_ini[j];                    /* al principio el hueco libre es el primero     */
    }
    for (i = 0; i < I->m; i++) {                   /* recorre todas las filas...                    */
        for (p = I->fila_ini[i]; p < I->fila_ini[i + 1]; p++) {  /* ...y todas sus columnas         */
            j = I->fila_col[p];                    /* columna j que cubre la fila i                 */
            I->col_fil[sig[j]] = i;                /* apunta la fila i en la lista de la columna j  */
            sig[j]++;                              /* avanza el hueco libre de esa columna          */
        }
    }
    free(sig);                                     /* el vector auxiliar ya no hace falta           */
    return 1;                                      /* 1 = lectura correcta                          */
}

/* Devuelve al sistema toda la memoria reservada para la instancia. */
void liberar_instancia(Instancia *I)
{
    free(I->coste);  free(I->fila_ini);  free(I->fila_col);   /* free libera un bloque reservado    */
    free(I->col_ini);  free(I->col_fil);  free(I->a);
    free(I->gan);  free(I->cand);  free(I->veto);  free(I->duena);
    free(I->nunic);  free(I->cnt);  free(I->lis1);  free(I->lis2);
}

/* ========================================================================== */
/*  5. OPERACIONES ELEMENTALES SOBRE UNA SOLUCION                             */
/* ========================================================================== */

/* Reserva la memoria de una solucion y la deja vacia. */
void sol_crear(const Instancia *I, Solucion *S)
{
    S->en    = vector_byte((size_t)I->n);          /* indicador de pertenencia de cada columna      */
    S->cub   = vector_int(I->m);                   /* veces que esta cubierta cada fila             */
    S->lista = vector_int(I->n);                   /* lista de columnas elegidas                    */
    S->pos   = vector_int(I->n);                   /* posicion de cada columna en la lista          */
    sol_vaciar(I, S);                              /* la deja sin ninguna columna                   */
}

/* Devuelve la memoria de una solucion. */
void sol_liberar(Solucion *S)
{
    free(S->en);  free(S->cub);  free(S->lista);  free(S->pos);
}

/* Deja la solucion sin columnas (todas las filas sin cubrir, coste 0). */
void sol_vaciar(const Instancia *I, Solucion *S)
{
    int i, j;                                      /* indices de fila y de columna                  */
    for (j = 0; j < I->n; j++) {                   /* para cada columna...                          */
        S->en[j]  = 0;                             /* ...no esta en la solucion                     */
        S->pos[j] = -1;                            /* ...y por tanto no tiene posicion en la lista  */
    }
    for (i = 0; i < I->m; i++) {                   /* para cada fila...                             */
        S->cub[i] = 0;                             /* ...ninguna columna la cubre                   */
    }
    S->ncols = 0;                                  /* cero columnas elegidas                        */
    S->nsin  = I->m;                               /* las m filas estan sin cubrir                  */
    S->z     = 0.0;                                /* coste nulo                                    */
}

/* Copia la solucion src sobre dst (dst <- src). */
void sol_copiar(const Instancia *I, Solucion *dst, const Solucion *src)
{
    memcpy(dst->en,    src->en,    (size_t)I->n * sizeof(unsigned char));  /* copia el vector en    */
    memcpy(dst->cub,   src->cub,   (size_t)I->m * sizeof(int));            /* copia el vector cub   */
    memcpy(dst->lista, src->lista, (size_t)src->ncols * sizeof(int));      /* solo la parte usada   */
    memcpy(dst->pos,   src->pos,   (size_t)I->n * sizeof(int));            /* copia el vector pos   */
    dst->ncols = src->ncols;                       /* copia el numero de columnas                   */
    dst->nsin  = src->nsin;                        /* copia el numero de filas sin cubrir           */
    dst->z     = src->z;                           /* copia el coste                                */
}

/* Anade la columna j a la solucion (se supone que no estaba). */
void sol_insertar(const Instancia *I, Solucion *S, int j)
{
    int p, i;                                      /* posicion en la lista de filas y fila          */
    S->en[j] = 1;                                  /* marca la columna como elegida                 */
    S->pos[j] = S->ncols;                          /* ocupara la ultima posicion de la lista        */
    S->lista[S->ncols] = j;                        /* se apunta al final de la lista                */
    S->ncols++;                                    /* hay una columna mas                           */
    S->z += I->coste[j];                           /* el coste total aumenta en c(j)                */
    for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {  /* para cada fila i que cubre j...       */
        i = I->col_fil[p];
        if (S->cub[i] == 0) {                      /* ...si estaba sin cubrir...                    */
            S->nsin--;                             /* ...deja de estarlo                            */
        }
        S->cub[i]++;                               /* la fila i tiene una columna mas que la cubre  */
    }
}

/* Quita la columna j de la solucion (se supone que estaba). */
void sol_quitar(const Instancia *I, Solucion *S, int j)
{
    int p, i, q, ult;                              /* auxiliares                                    */
    q   = S->pos[j];                               /* posicion que ocupa j en la lista              */
    ult = S->lista[S->ncols - 1];                  /* ultima columna de la lista                    */
    S->lista[q] = ult;                             /* la ultima pasa a ocupar el hueco que deja j   */
    S->pos[ult] = q;                               /* y se actualiza su posicion                    */
    S->ncols--;                                    /* hay una columna menos                         */
    S->pos[j] = -1;                                /* j ya no tiene posicion                        */
    S->en[j]  = 0;                                 /* j ya no esta elegida                          */
    S->z -= I->coste[j];                           /* el coste total disminuye en c(j)              */
    for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {  /* para cada fila i que cubria j...      */
        i = I->col_fil[p];
        S->cub[i]--;                               /* ...una columna menos la cubre                 */
        if (S->cub[i] == 0) {                      /* si se ha quedado sin ninguna...               */
            S->nsin++;                             /* ...hay una fila mas sin cubrir                */
        }
    }
}

/* Devuelve 1 si la columna j (que esta en S) es REDUNDANTE, es decir, si   */
/* todas sus filas estan cubiertas ademas por alguna otra columna elegida.  */
int sol_redundante(const Instancia *I, const Solucion *S, int j)
{
    int p;                                         /* posicion en la lista de filas de j            */
    for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {  /* para cada fila que cubre j...         */
        if (S->cub[I->col_fil[p]] < 2) {           /* ...si solo la cubre j (cub = 1)...            */
            return 0;                              /* ...j es imprescindible: no es redundante      */
        }
    }
    return 1;                                      /* todas sus filas tienen otra columna: sobra    */
}

/* Recalcula el coste sumando desde cero (elimina errores de redondeo que   */
/* se acumulan tras millones de sumas y restas sobre S->z).                 */
double sol_coste_exacto(const Instancia *I, const Solucion *S)
{
    int    t;                                      /* indice en la lista de columnas elegidas       */
    double z = 0.0;                                /* acumulador del coste                          */
    for (t = 0; t < S->ncols; t++) {               /* para cada columna elegida...                  */
        z += I->coste[S->lista[t]];                /* ...se suma su coste                           */
    }
    return z;
}

/* Comprobacion completa e independiente de una solucion: recuenta desde    */
/* cero la cobertura de cada fila. Devuelve 1 si es factible y si los datos */
/* guardados (cub, nsin, z) coinciden con los recalculados.                 */
int sol_comprobar(const Instancia *I, const Solucion *S)
{
    int  i, j, t, p, ok = 1;                       /* ok = 1 mientras no se detecte ningun fallo    */
    int *c = vector_int(I->m);                     /* cobertura recalculada (empieza a cero)        */
    for (t = 0; t < S->ncols; t++) {               /* para cada columna elegida...                  */
        j = S->lista[t];
        if (!S->en[j] || S->pos[j] != t) {         /* ...sus marcas deben ser coherentes            */
            ok = 0;
        }
        for (p = I->col_ini[j]; p < I->col_ini[j + 1]; p++) {
            c[I->col_fil[p]]++;                    /* ...y se cuentan las filas que cubre           */
        }
    }
    for (i = 0; i < I->m; i++) {                   /* para cada fila...                             */
        if (c[i] == 0 || c[i] != S->cub[i]) {      /* ...debe estar cubierta y coincidir el recuento */
            ok = 0;
        }
    }
    if (S->nsin != 0) {                            /* no puede quedar ninguna fila sin cubrir       */
        ok = 0;
    }
    if (fabs(sol_coste_exacto(I, S) - S->z) > 1e-6) {  /* el coste guardado debe ser el correcto    */
        ok = 0;
    }
    free(c);                                       /* libera el vector auxiliar                     */
    return ok;
}

/* Devuelve 1 si A y B contienen exactamente las mismas columnas. */
int sol_iguales(const Solucion *A, const Solucion *B)
{
    int t;                                         /* indice en la lista de A                       */
    if (A->ncols != B->ncols) {                    /* si tienen distinto numero de columnas...      */
        return 0;                                  /* ...no pueden ser iguales                      */
    }
    for (t = 0; t < A->ncols; t++) {               /* para cada columna de A...                     */
        if (!B->en[A->lista[t]]) {                 /* ...si no esta en B...                         */
            return 0;                              /* ...son distintas                              */
        }
    }
    return 1;                                      /* mismas columnas (y mismo numero): iguales     */
}

/* ========================================================================== */
/*  6. ELIMINACION DE COLUMNAS REDUNDANTES                                    */
/* ========================================================================== */
/*  Una columna redundante se puede quitar sin perder factibilidad, y eso     */
/*  siempre abarata la solucion. Si hay varias, se quita primero la MAS CARA  */
/*  (decision voraz: es la que mas ahorra), y se repite, porque al quitar una */
/*  otras pueden dejar de ser redundantes. Devuelve cuantas ha quitado.       */

int quitar_redundantes(const Instancia *I, Solucion *S)
{
    int t, j, peor, quitadas = 0;                  /* peor = redundante mas cara encontrada         */
    for (;;) {                                     /* bucle sin fin: se sale con "break"            */
        peor = -1;                                 /* todavia no se ha encontrado ninguna           */
        for (t = 0; t < S->ncols; t++) {           /* recorre las columnas elegidas                 */
            j = S->lista[t];
            if ((peor < 0 || I->coste[j] > I->coste[peor])   /* si es mas cara que la candidata...  */
                && sol_redundante(I, S, j)) {      /* ...y ademas es redundante...                  */
                peor = j;                          /* ...pasa a ser la candidata a salir            */
            }
        }
        if (peor < 0) {                            /* si no queda ninguna redundante...             */
            break;                                 /* ...se termina                                 */
        }
        sol_quitar(I, S, peor);                    /* quita la redundante mas cara                  */
        quitadas++;                                /* y la cuenta                                   */
    }
    return quitadas;
}

/* ========================================================================== */
/*  7. COMPLETAR UNA SOLUCION (construccion voraz / reparacion)               */
/* ========================================================================== */
/*  Mientras queden filas sin cubrir, se anade una columna. Cada candidata j  */
/*  (columna que cubriria gan[j] > 0 filas nuevas) recibe una PUNTUACION:     */
/*      criterio 1:  1 / c(j)            (la mas barata)                      */
/*      criterio 2:  gan[j]              (la que mas filas nuevas cubre)      */
/*      criterio 3:  gan[j] / c(j)       (filas nuevas por unidad de coste)   */
/*  Si se pasa un vector "memoria" (GRASP), la puntuacion final es            */
/*      lambda * (puntuacion voraz normalizada) + (1 - lambda) * memoria[j].  */
/*  Se forma la LISTA RESTRINGIDA DE CANDIDATOS (LRC) con las "lrc" columnas  */
/*  de mayor puntuacion y se elige UNA AL AZAR entre ellas. Con lrc = 1 el    */
/*  procedimiento es el voraz puro (determinista).                            */
/*  Las columnas con veto[j] = 1 no pueden entrar.                            */

/* Puntuacion voraz de una columna de coste c que cubriria g filas nuevas. */
static double puntuacion(int criterio, int g, double c)
{
    if (criterio == 1) {                           /* criterio 1: solo importa el coste             */
        return 1.0 / (c + 1e-12);                  /* mas barata = mayor puntuacion                 */
    }
    if (criterio == 2) {                           /* criterio 2: solo importa la cobertura         */
        return (double)g;                          /* mas filas nuevas = mayor puntuacion           */
    }
    return (double)g / (c + 1e-12);                /* criterio 3: filas nuevas por unidad de coste  */
}                                                  /* (el 1e-12 evita dividir entre cero)           */

void completar(Instancia *I, Solucion *S, int criterio, int lrc,
               const double *memoria, double lambda)
{
    int    *gan  = I->gan;                         /* gan[j]: filas nuevas que cubriria j (todo a 0) */
    int    *cand = I->cand;                        /* lista de columnas candidatas                  */
    int     nc = 0;                                /* numero de candidatas en la lista              */
    int     usar_veto = 1;                         /* 1 = se respetan los vetos                     */
    int     mej_col[MAX_LRC];                      /* columnas de la LRC, de mejor a peor           */
    double  mej_val[MAX_LRC];                      /* sus puntuaciones                              */
    int     nm, i, j, p, q, t, u, elegida;         /* indices y auxiliares                          */
    double  v, vmax;                               /* puntuacion de una columna y la maxima         */

    if (lrc < 1) lrc = 1;                          /* la LRC tiene como minimo 1 elemento           */
    if (lrc > MAX_LRC) lrc = MAX_LRC;              /* y como maximo MAX_LRC                         */

    /* --- recuento inicial: que columnas cubririan filas aun sin cubrir ---- */
    for (i = 0; i < I->m; i++) {                   /* para cada fila i...                           */
        if (S->cub[i] != 0) continue;              /* ...si ya esta cubierta no aporta nada         */
        for (p = I->fila_ini[i]; p < I->fila_ini[i + 1]; p++) {  /* para cada columna j que cubre i */
            j = I->fila_col[p];
            if (gan[j] == 0) {                     /* primera vez que aparece la columna j...       */
                cand[nc] = j;                      /* ...se apunta en la lista de candidatas        */
                nc++;
            }
            gan[j]++;                              /* j cubriria una fila nueva mas                 */
        }
    }

    /* --- bucle principal: una columna nueva en cada vuelta ---------------- */
    while (S->nsin > 0) {                          /* mientras quede alguna fila sin cubrir         */

        /* compactar la lista: se eliminan las candidatas que ya no aportan  */
        q = 0;                                     /* posicion de escritura                         */
        vmax = 0.0;                                /* mayor puntuacion voraz (para normalizar)      */
        for (t = 0; t < nc; t++) {                 /* recorre las candidatas                        */
            j = cand[t];
            if (gan[j] == 0) continue;             /* ya no cubre nada nuevo: se descarta           */
            cand[q] = j;                           /* se conserva                                   */
            q++;
            v = puntuacion(criterio, gan[j], I->coste[j]);
            if (v > vmax) vmax = v;                /* se guarda la mejor puntuacion voraz           */
        }
        nc = q;                                    /* nuevo tamano de la lista                      */

        /* formar la LRC: las "lrc" candidatas no vetadas de mayor puntuacion */
        nm = 0;                                    /* la LRC empieza vacia                          */
        for (t = 0; t < nc; t++) {                 /* recorre las candidatas                        */
            j = cand[t];
            if (usar_veto && I->veto[j]) continue; /* las vetadas no pueden entrar                  */
            v = puntuacion(criterio, gan[j], I->coste[j]);   /* puntuacion voraz                    */
            if (memoria != NULL) {                 /* si hay memoria (GRASP), se combina con ella:  */
                v = lambda * (v / vmax)            /*   lambda * [voraz normalizado a (0,1]]        */
                    + (1.0 - lambda) * memoria[j]; /*   + (1 - lambda) * [memoria en [0,1]]         */
            }
            if (nm == lrc && v <= mej_val[nm - 1]) continue;  /* peor que la ultima de la LRC llena */
            if (nm < lrc) nm++;                    /* si aun cabe, la LRC crece en un elemento      */
            u = nm - 1;                            /* hueco libre: la ultima posicion               */
            while (u > 0 && mej_val[u - 1] < v) {  /* se desplazan hacia abajo las peores que v...  */
                mej_val[u] = mej_val[u - 1];
                mej_col[u] = mej_col[u - 1];
                u--;
            }
            mej_val[u] = v;                        /* ...y se coloca j en su sitio (orden decreciente) */
            mej_col[u] = j;
        }

        if (nm == 0) {                             /* si no hay ninguna candidata permitida es porque */
            usar_veto = 0;                         /* los vetos lo impiden: se levantan los vetos   */
            continue;                              /* y se repite la vuelta                         */
        }

        elegida = mej_col[azar_entero(nm)];        /* eleccion al azar dentro de la LRC             */

        /* antes de insertarla se actualizan los contadores gan: las filas    */
        /* que la elegida va a cubrir dejan de ser "nuevas" para las demas    */
        for (p = I->col_ini[elegida]; p < I->col_ini[elegida + 1]; p++) {  /* filas de la elegida   */
            i = I->col_fil[p];
            if (S->cub[i] != 0) continue;          /* si la fila ya estaba cubierta no cambia nada  */
            for (q = I->fila_ini[i]; q < I->fila_ini[i + 1]; q++) {  /* columnas que cubren esa fila */
                gan[I->fila_col[q]]--;             /* cada una pierde una fila nueva                */
            }
        }
        sol_insertar(I, S, elegida);               /* por fin, la elegida entra en la solucion      */
    }
    /* Al terminar no queda ninguna fila sin cubrir, de modo que todos los   */
    /* contadores gan[] han vuelto a 0 por si solos y el vector queda limpio. */
}

/* ========================================================================== */
/*  8. PERTURBACION  Omega_k                                                  */
/* ========================================================================== */
/*  Genera una solucion "vecina a distancia k" de S:                          */
/*    1) quita k columnas elegidas al azar (la solucion deja de ser factible), */
/*    2) repara con "completar" (cociente + LRC), prohibiendo que vuelvan a   */
/*       entrar justo las que se acaban de quitar (si no, muchas veces se     */
/*       reconstruiria la misma solucion),                                    */
/*    3) elimina las columnas que hayan quedado redundantes.                  */
/*  Con k pequeno la vecina se parece mucho a S; con k grande, poco.          */

void perturbar(Instancia *I, Solucion *S, int k, int lrc)
{
    int quitadas[MAX_K];                           /* columnas que se han quitado                   */
    int t, j;                                      /* contador y columna                            */

    if (k > S->ncols - 1) k = S->ncols - 1;        /* no se pueden quitar todas: se deja al menos 1 */
    if (k > MAX_K) k = MAX_K;                      /* limite de seguridad del vector quitadas       */
    for (t = 0; t < k; t++) {                      /* se repite k veces:                            */
        j = S->lista[azar_entero(S->ncols)];       /*   se elige al azar una columna de la solucion */
        sol_quitar(I, S, j);                       /*   se quita                                    */
        I->veto[j] = 1;                            /*   se le prohibe volver a entrar al reparar    */
        quitadas[t] = j;                           /*   se apunta para poder levantar el veto       */
    }
    completar(I, S, 3, lrc, NULL, 1.0);            /* reparacion voraz aleatorizada (cociente)      */
    for (t = 0; t < k; t++) {                      /* se levantan los vetos                         */
        I->veto[quitadas[t]] = 0;
    }
    quitar_redundantes(I, S);                      /* limpieza final de columnas sobrantes          */
}
