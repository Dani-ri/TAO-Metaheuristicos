/* ALGORITMO CONSTRUCTIVO + BÚSQUEDA LOCAL

- Algoritmo constructivo: Construye una solución completa iterativamente,
de forma que en cada iteración se añade el subconjunto con mayor ratio
elementos_nuevos / coste 
En caso de empate se elige el primer subconjunto encontrado.

- Algoritmo de búsqueda local: A partir de la solcuión construida por el
algoritmo constructivo, se busca una solución mejor en su vecindario evaluando
posibles intercambios de subconjuntos.
Estrategias de selección:
1. First Improvement: Se selecciona el primer subconjunto que mejore la solución actual.
2. Best Improvement: Se selecciona el subconjunto que proporcione la mayor mejora en la solución actual.
Estrategias de intercambios permitidos:
1. 1-1
2. 1-1 y 1-2

*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include <limits.h>


typedef struct {
    int m;
    int n;
    int nnz;
    double *c;
    int *row_ptr;
    int *row_col_idx;
    int *col_ptr;
    int *col_row_idx;
} SCP;

typedef struct {
    int *sol;
    int nsol;
    double z;
} Solucion;

typedef enum {
    FIRST_IMPROVEMENT = 0,
    BEST_IMPROVEMENT = 1
} EstrategiaBL;

typedef enum {
    VECINDARIO_11 = 0,
    VECINDARIO_11_12 = 1
} TipoVecindario;

typedef struct {
    int iteraciones;
    int movimientos;
    long long evaluaciones;
} EstadisticasBL;

typedef struct {
    int eliminar;   /* 1 si el movimiento elimina A sin reemplazo */
    int a;
    int b;
    int c;
    double ahorro;
} Movimiento;

/* Algoritmo para la lectura de datos de los problemas SCP en formato disperso*/
SCP leer_scp(const char *ruta){
    SCP d;
    int i, j, k, t, p, *ncolj;
    double coste_leido;

    FILE *f = fopen(ruta, "r");
    if (f == NULL) {
        printf("Error: no se puede abrir el fichero %s\n", ruta);
        exit(1);
    }
    
    /*Paso 0*/
    fscanf(f, "%d %d", &d.m, &d.n);
    d.c = malloc(d.n * sizeof(double));
    ncolj = calloc(d.n, sizeof(int));
    d.row_ptr = malloc((d.m + 1) * sizeof(int));
    d.col_ptr = malloc((d.n + 1) * sizeof(int));
    for (j = 0; j < d.n; j++)
        fscanf(f, "%lf", &d.c[j]);

    /*Paso 1*/
    d.nnz = 0;
    for (i = 0; i < d.m; i++) {
        fscanf(f, "%d", &k);
        d.row_ptr[i] = k;
        d.nnz += k;
        for (t = 0; t < k; t++)
            fscanf(f, "%d", &j);
    }

    /*Paso 2*/
    d.row_col_idx = malloc(d.nnz * sizeof(int));
    d.col_row_idx = malloc(d.nnz * sizeof(int));

    /*Paso 3*/
    rewind(f);
    fscanf(f, "%d %d", &k, &t);
    for (j = 0; j < d.n; j++)
        fscanf(f, "%lf", &coste_leido);

    /*Paso 4*/
    p = 0;
    for (i = 0; i < d.m; i++) {
        int nind = d.row_ptr[i];
        d.row_ptr[i] = p;
        fscanf(f, "%d", &k);
        for (t = p; t < p + nind; t++) {
            fscanf(f, "%d", &j);
            d.row_col_idx[t] = j - 1;
            ncolj[j - 1]++;
        }
        p += nind;
    }
    d.row_ptr[d.m] = p;
    fclose(f);


    /*Paso 5*/
    p = 0;
    for (j = 0; j < d.n; j++) {
        d.col_ptr[j] = p;
        p += ncolj[j];
    }
    d.col_ptr[d.n] = p;
    memset(ncolj, 0, d.n * sizeof(int));
    for (i = 0; i < d.m; i++)
        for (t = d.row_ptr[i]; t < d.row_ptr[i + 1]; t++) {
            j = d.row_col_idx[t];
            d.col_row_idx[d.col_ptr[j] + ncolj[j]] = i;
            ncolj[j]++;
        }
    free(ncolj);
    return d;
}

static void liberar_scp(SCP *d)
{
    free(d->c);
    free(d->row_ptr);
    free(d->row_col_idx);
    free(d->col_ptr);
    free(d->col_row_idx);
    memset(d, 0, sizeof(*d));
}

static void liberar_solucion(Solucion *s)
{
    free(s->sol);
    s->sol = NULL;
    s->nsol = 0;
    s->z = 0.0;
}


/* Función para medir el tiempo cpu adaptada tanto a windows como a linux*/
#ifdef _WIN32
#include <windows.h>
double tiempo_cpu(void)
{
    FILETIME creacion, fin, sistema, usuario;
    double t_sistema, t_usuario;

    GetProcessTimes(GetCurrentProcess(), &creacion, &fin, &sistema, &usuario);
    t_sistema = (sistema.dwHighDateTime * 4294967296.0 + sistema.dwLowDateTime) * 1e-7;
    t_usuario = (usuario.dwHighDateTime * 4294967296.0 + usuario.dwLowDateTime) * 1e-7;
    return t_sistema + t_usuario;
}
#else
static double tiempo_cpu(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}
#endif

static int comparar_indices(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

/* -------------------------------------------------------------------------
 * Algoritmo constructivo: Greedy*variables (mayor ratio nummero de filas no cubiertas/coste)
 *
 * cnt[j] = numero de elementos aun no cubiertos que cubre la columna j.
 * El ratio que se maximiza es cnt[j] / coste[j].
 * Los subconjuntos vacios (cnt=0 desde el inicio) nunca se seleccionan.
 * ------------------------------------------------------------------------- */

static Solucion constructivo(const SCP *d)
{
    int i, j, t;
    int ncubiertos = 0;
    char *cubierto = (char *)calloc((size_t)d->m, sizeof(char));
    char *seleccionado = (char *)calloc((size_t)d->n, sizeof(char));
    int *cnt = (int *)malloc((size_t)d->n * sizeof(int));
    Solucion s;

    s.sol = (int *)malloc((size_t)d->n * sizeof(int));
    s.nsol = 0;
    s.z = 0.0;

    for (j = 0; j < d->n; j++)
        cnt[j] = d->col_ptr[j + 1] - d->col_ptr[j];

    while (ncubiertos < d->m) {
        int mejor = -1;
        double mejor_ratio = -DBL_MAX;

        for (j = 0; j < d->n; j++) {
            double ratio;

            if (seleccionado[j] || cnt[j] <= 0)
                continue;

            ratio = (double)cnt[j] / d->c[j];

            if (ratio > mejor_ratio) {
                mejor = j;
                mejor_ratio = ratio;
            }
        }

        if (mejor < 0) {
            fprintf(stderr,
                    "ERROR: la instancia no se puede cubrir. "
                    "Quedan %d elementos sin cubrir.\n",
                    d->m - ncubiertos);
            free(cubierto);
            free(seleccionado);
            free(cnt);
            liberar_solucion(&s);
            s.sol = NULL;
            return s;
        }

        seleccionado[mejor] = 1;
        s.sol[s.nsol++] = mejor;
        s.z += d->c[mejor];

        /* Actualizar la cobertura de los elementos y comprobar qué elementos nuevos aporta cada columna. */
        for (t = d->col_ptr[mejor]; t < d->col_ptr[mejor + 1]; t++) {
            i = d->col_row_idx[t];

            if (!cubierto[i]) {
                cubierto[i] = 1;
                ncubiertos++;

                for (int r = d->row_ptr[i]; r < d->row_ptr[i + 1]; r++) {
                    int col = d->row_col_idx[r];
                    cnt[col]--;
                }
            }
        }
    }

    free(cubierto);
    free(seleccionado);
    free(cnt);
    return s;
}

/* -------------------------------------------------------------------------
 * Búsqueda local.
 *
 * seleccionada[j]: indica si la columna j pertenece a la solucion.
 * cobertura[i]: numero de columnas seleccionadas que cubren el elemento i.
 *
 * Un elemento critico para eliminar A es aquel con cobertura[i] == 1.
 * Para que A pueda sustituirse, el reemplazo debe cubrir todos esos elementos.
 * ------------------------------------------------------------------------- */

static void actualizar_contadores(const SCP *d, int j, int delta, int *cobertura)
{
    int t;
    for (t = d->col_ptr[j]; t < d->col_ptr[j + 1]; t++) {
        int i = d->col_row_idx[t];
        cobertura[i] += delta;
    }
}

/* Guarda en filas_criticas los elementos que solo cubre A (cobertura == 1) y devuelve cuantos hay. */
static int construir_criticos(const SCP *d, int a, const int *cobertura,
                              int *filas_criticas)
{
    int t, ncrit = 0;

    for (t = d->col_ptr[a]; t < d->col_ptr[a + 1]; t++) {
        int i = d->col_row_idx[t];
        if (cobertura[i] == 1)
            filas_criticas[ncrit++] = i;
    }
    return ncrit;
}

/* Comprueba si una columna o la union de dos columnas cubre los criticos. 
cubre_j es la fila j de la matriz densa: cubre_j[i] = 1 si j cubre i. */
static int columna_cubre_criticos(const char *cubre_j,
                                  const int *filas_criticas, int ncrit)
{
    int q;
    for (q = 0; q < ncrit; q++)
        if (!cubre_j[filas_criticas[q]])
            return 0;
    return 1;
}

static int pareja_cubre_criticos(const char *cubre_b, const char *cubre_c,
                                 const int *filas_criticas, int ncrit)
{
    int q;
    for (q = 0; q < ncrit; q++) {
        int i = filas_criticas[q];
        if (!cubre_b[i] && !cubre_c[i])
            return 0;
    }
    return 1;
}

/* Matriz densa n x m: cubre[j * m + i] = 1 si la columna j cubre el elemento i. */
static char *crear_matriz_cobertura(const SCP *d)
{
    int j, t;
    char *cubre = (char *)calloc((size_t)d->n * d->m, sizeof(char));

    for (j = 0; j < d->n; j++)
        for (t = d->col_ptr[j]; t < d->col_ptr[j + 1]; t++)
            cubre[(size_t)j * d->m + d->col_row_idx[t]] = 1;

    return cubre;
}

/* Aplica el movimiento A -> B (y opcionalmente C). */
static void aplicar_movimiento(const SCP *d, int a, int b, int c, char *seleccionada, int *cobertura, double *coste_actual)
{
    actualizar_contadores(d, a, -1, cobertura);
    seleccionada[a] = 0;
    *coste_actual -= d->c[a];

    if (b >= 0) {
        actualizar_contadores(d, b, +1, cobertura);
        seleccionada[b] = 1;
        *coste_actual += d->c[b];
    }

    if (c >= 0) {
        actualizar_contadores(d, c, +1, cobertura);
        seleccionada[c] = 1;
        *coste_actual += d->c[c];
    }
}

static Solucion reconstruir_solucion(const SCP *d, const char *seleccionada, double coste)
{
    int j;
    Solucion s;
    s.sol = (int *)malloc((size_t)d->n * sizeof(int));
    s.nsol = 0;
    s.z = coste;

    for (j = 0; j < d->n; j++)
        if (seleccionada[j])
            s.sol[s.nsol++] = j;

    return s;
}

/* Busca el mejor movimiento para una columna A.
   Para 1-2 B debe dejar algun critico sin cubrir y C se busca entre las columnas que cubren uno de esos criticos. */
static int buscar_movimiento_para_a(
    const SCP *d, int a, const char *seleccionada, const int *cobertura,
    const char *cubre, const int *filas_criticas, int ncrit,
    TipoVecindario vecindario, EstrategiaBL estrategia,
    Movimiento *mejor, long long *evaluaciones)
{
    int j, t, x;
    int encontrado = 0;
    double mejor_ahorro = 0;
    int *frecuencia = NULL;
    int *orden_candidatos = (int *)malloc((size_t)d->n * sizeof(int));

    for (x = 0; x < d->n; x++) orden_candidatos[x] = x;

    mejor->eliminar = 0;
    mejor->a = a;
    mejor->b = -1;
    mejor->c = -1;
    mejor->ahorro = 0.0;

    /* Si A no tiene elementos criticos, se puede eliminar directamente. */
    if (ncrit == 0) {
        mejor->eliminar = 1;
        mejor->ahorro = d->c[a];
        free(orden_candidatos);
        return 1;
    }

    /* Primero: movimientos 1-1. */
    for (x = 0; x < d->n; x++) {
        double ahorro;
        j = orden_candidatos[x];
        const char *cj;

        if (seleccionada[j] || j == a)
            continue;

        (*evaluaciones)++;
        ahorro = d->c[a] - d->c[j];
        if (ahorro <= 0)
            continue;

        cj = cubre + (size_t)j * d->m;
        if (!columna_cubre_criticos(cj, filas_criticas, ncrit))
            continue;

        if (estrategia == FIRST_IMPROVEMENT) {
            mejor->eliminar = 0;
            mejor->a = a;
            mejor->b = j;
            mejor->c = -1;
            mejor->ahorro = ahorro;
            free(orden_candidatos);
            return 1;
        }

        if (ahorro > mejor_ahorro) {
            mejor_ahorro = ahorro;
            mejor->eliminar = 0;
            mejor->a = a;
            mejor->b = j;
            mejor->c = -1;
            mejor->ahorro = ahorro;
            encontrado = 1;
        }
    }

    if (vecindario == VECINDARIO_11 || d->n < 3) {
        free(orden_candidatos);
        return encontrado;
    }

    /* Frecuencia de candidatos para cada elemento critico: cuantas columnas
       no seleccionadas y mas baratas que A lo cubren.
       Se elige, para cada B, el critico no cubierto por B con menos opciones C. */
    frecuencia = (int *)calloc((size_t)d->m, sizeof(int));

    {
        int q;
        for (q = 0; q < ncrit; q++) {
            int i = filas_criticas[q];
            for (t = d->row_ptr[i]; t < d->row_ptr[i + 1]; t++) {
                int k = d->row_col_idx[t];
                if (!seleccionada[k] && d->c[k] < d->c[a])
                    frecuencia[i]++;
            }
        }
    }

    /* Movimientos 1-2: A se sustituye por B+C. */
    for (x = 0; x < d->n; x++) {
        int fila_guia = -1;
        j = orden_candidatos[x];
        int min_opciones = INT_MAX;
        double restante;
        const char *cj;

        if (seleccionada[j] || j == a || d->c[j] >= d->c[a])
            continue;

        cj = cubre + (size_t)j * d->m;

        /* Si B ya cubre todos los criticos, el movimiento 1-1 es suficiente. */
        if (columna_cubre_criticos(cj, filas_criticas, ncrit))
            continue;

        /* Elegir un elemento critico que B no cubre y que tenga pocas opciones C. */
        {
            int q;
            for (q = 0; q < ncrit; q++) {
                int opciones;
                t = filas_criticas[q];
                if (cj[t]) continue;

                opciones = frecuencia[t];
            if (opciones < min_opciones) {
                min_opciones = opciones;
                    fila_guia = t;
                }
            }
        }

        if (fila_guia < 0 || min_opciones == 0)
            continue;

        restante = d->c[a] - d->c[j];

        /* Todo C valido debe cubrir fila_guia. En First Improvement se
           recorre los candidatos C en el orden original. */
        {
            int inicio = d->row_ptr[fila_guia];
            int grado = d->row_ptr[fila_guia + 1] - inicio;
            int desplazamiento = 0; /* Recorrer candidatos en el orden original. */
            int q;

            for (q = 0; q < grado; q++) {
                int pos = inicio + (desplazamiento + q) % grado;
                int k = d->row_col_idx[pos];
                double ahorro;
                const char *ck;

                if (k == j || seleccionada[k] || k == a)
                    continue;
                if (d->c[k] >= restante)
                    continue;

                (*evaluaciones)++;
                ck = cubre + (size_t)k * d->m;

                if (!pareja_cubre_criticos(cj, ck, filas_criticas, ncrit))
                    continue;

                ahorro = d->c[a] - d->c[j] - d->c[k];
                if (ahorro <= 0)
                    continue;

                if (estrategia == FIRST_IMPROVEMENT) {
                    mejor->eliminar = 0;
                    mejor->a = a;
                    mejor->b = j;
                    mejor->c = k;
                    mejor->ahorro = ahorro;
                    free(frecuencia);
                    free(orden_candidatos);
                    return 1;
                }

                if (ahorro > mejor_ahorro) {
                    mejor_ahorro = ahorro;
                    mejor->eliminar = 0;
                    mejor->a = a;
                    mejor->b = j;
                    mejor->c = k;
                    mejor->ahorro = ahorro;
                    encontrado = 1;
                }
            }
        }
    }

    free(frecuencia);
    free(orden_candidatos);
    (void)cobertura;
    return encontrado;
}

static Solucion busqueda_local(const SCP *d, const Solucion *inicial,
                               EstrategiaBL estrategia, TipoVecindario vecindario,
                               EstadisticasBL *stats)
{
    int j, t;
    char *seleccionada = (char *)calloc((size_t)d->n, sizeof(char));
    int *cobertura = (int *)calloc((size_t)d->m, sizeof(int));
    char *cubre = crear_matriz_cobertura(d);
    int *filas_criticas = (int *)malloc((size_t)d->m * sizeof(int));
    int *orden = (int *)malloc((size_t)d->n * sizeof(int));
    double coste_actual = inicial->z;
    int cambio;

    stats->iteraciones = 0;
    stats->movimientos = 0;
    stats->evaluaciones = 0;

    for (j = 0; j < inicial->nsol; j++)
        seleccionada[inicial->sol[j]] = 1;

    for (j = 0; j < d->n; j++)
        orden[j] = j;

    for (j = 0; j < d->n; j++) {
        if (seleccionada[j])
            actualizar_contadores(d, j, +1, cobertura);
    }

    do {
        int a;
        Movimiento movimiento;
        cambio = 0;
        stats->iteraciones++;

        if (estrategia == FIRST_IMPROVEMENT) {
            /* First Improvement: aceptar el primer movimiento que mejore. */
            for (j = 0; j < d->n && !cambio; j++) {
                int ncrit;
                a = orden[j];
                if (!seleccionada[a]) continue;

                ncrit = construir_criticos(d, a, cobertura, filas_criticas);

                if (buscar_movimiento_para_a(
                        d, a, seleccionada, cobertura, cubre, filas_criticas,
                        ncrit, vecindario, estrategia, &movimiento,
                        &stats->evaluaciones)) {
                    aplicar_movimiento(d, movimiento.a, movimiento.b,
                                       movimiento.c, seleccionada, cobertura,
                                       &coste_actual);
                    stats->movimientos++;
                    cambio = 1;
                }
            }
        } else {
            /* Best Improvement: evaluar todos los movimientos y elegir el mejor. */
            Movimiento mejor_global;
            int hay_mejor = 0;
            mejor_global.ahorro = 0;

            for (j = 0; j < d->n; j++) {
                int ncrit;
                a = j;
                if (!seleccionada[a]) continue;

                ncrit = construir_criticos(d, a, cobertura, filas_criticas);

                if (buscar_movimiento_para_a(
                        d, a, seleccionada, cobertura, cubre, filas_criticas,
                        ncrit, vecindario, estrategia, &movimiento,
                        &stats->evaluaciones)) {
                    if (!hay_mejor || movimiento.ahorro > mejor_global.ahorro) {
                        mejor_global = movimiento;
                        hay_mejor = 1;
                    }
                }
            }

            if (hay_mejor) {
                aplicar_movimiento(d, mejor_global.a, mejor_global.b,
                                   mejor_global.c, seleccionada, cobertura,
                                   &coste_actual);
                stats->movimientos++;
                cambio = 1;
            }
        }

    } while (cambio);

    /* Verificacion final de factibilidad. */
    for (t = 0; t < d->m; t++) {
        if (cobertura[t] <= 0) {
            fprintf(stderr, "ERROR: La busqueda local dejo un elemento sin cubrir.\n");
            exit(EXIT_FAILURE);
        }
    }

    {
        Solucion resultado = reconstruir_solucion(d, seleccionada, coste_actual);
        free(seleccionada);
        free(cobertura);
        free(cubre);
        free(filas_criticas);
        free(orden);
        return resultado;
    }
}

/* -------------------------------------------------------------------------
 * Validacion y escritura de resultados
 * ------------------------------------------------------------------------- */

static int es_factible(const SCP *d, const Solucion *s)
{
    int j, t;
    int *cobertura = (int *)calloc((size_t)d->m, sizeof(int));
    int factible = 1;

    if (!cobertura) return 0;

    for (j = 0; j < s->nsol; j++) {
        int col = s->sol[j];
        for (t = d->col_ptr[col]; t < d->col_ptr[col + 1]; t++)
            cobertura[d->col_row_idx[t]]++;
    }

    for (j = 0; j < d->m; j++) {
        if (cobertura[j] == 0) {
            factible = 0;
            break;
        }
    }

    free(cobertura);
    return factible;
}

static const char *nombre_estrategia(EstrategiaBL e)
{
    return e == FIRST_IMPROVEMENT ? "First" : "Best";
}

static const char *nombre_vecindario(TipoVecindario v)
{
    return v == VECINDARIO_11 ? "1-1" : "1-1_y_1-2";
}

static void escribir_solucion(FILE *f, const char *nombre, const Solucion *s)
{
    int k;
    int *copia = (int *)malloc((size_t)s->nsol * sizeof(int));

    if (!copia) return;
    memcpy(copia, s->sol, (size_t)s->nsol * sizeof(int));
    qsort(copia, (size_t)s->nsol, sizeof(int), comparar_indices);

    fprintf(f, "\n============================================================\n");
    fprintf(f, "%s\n", nombre);
    fprintf(f, "Subconjuntos seleccionados: %d\n", s->nsol);
    fprintf(f, "Coste: %.6f\n", s->z);
    fprintf(f, "Indices (base 1):\n");
    for (k = 0; k < s->nsol; k++) {
        fprintf(f, "%d", copia[k] + 1);
        if ((k + 1) % 15 == 0 || k == s->nsol - 1)
            fprintf(f, "\n");
        else
            fprintf(f, " ");
    }
    free(copia);
}

/* -------------------------------------------------------------------------
 * Programa principal: compara 4 configuraciones BL.
 * ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    const char *ruta = (argc >= 2) ? argv[1] : "datos/SCP04.dat";
    SCP d;
    FILE *fres;
    FILE *fsol;
    int estrategia, vecindario, config;
    double inicio_total, fin_total;
    double costes[4] = {0.0};
    double tiempos[4] = {0.0};
    const char *nombres_config[4] = {
        "First_11", "First_112", "Best_11", "Best_112"
    };

    printf("============================================================\n");
    printf(" PRACTICA 2 - SCP: Greedy + Busqueda local\n");
    printf("============================================================\n");
    printf("Fichero: %s\n", ruta);

    inicio_total = tiempo_cpu();
    d = leer_scp(ruta);

    printf("Instancia leida: %d elementos, %d subconjuntos, %d unos.\n",
           d.m, d.n, d.nnz);

    fres = fopen("resultados/resultados.txt", "w");
    if (!fres) {
        fprintf(stderr, "No se puede crear resultados/resultados.txt.\n");
        liberar_scp(&d);
        return EXIT_FAILURE;
    }

    fsol = fopen("soluciones/soluciones.txt", "w");
    if (!fsol) {
        fprintf(stderr, "No se puede crear soluciones/soluciones.txt.\n");
        fclose(fres);
        liberar_scp(&d);
        return EXIT_FAILURE;
    }

    fprintf(fres, "RESULTADOS DE LA PRACTICA SCP\n");
    fprintf(fres, "Instancia: %s\n", ruta);
    fprintf(fsol, "SOLUCIONES DE LA PRACTICA SCP\n");
    fprintf(fsol, "Instancia: %s\n", ruta);

        for (estrategia = 0; estrategia < 2; estrategia++) {
            for (vecindario = 0; vecindario < 2; vecindario++) {
                Solucion inicial, final;
                EstadisticasBL stats;
                double t0, t1, t2;
                double tiempo_greedy, tiempo_bl;
                int factible;
                char nombre[160];

                config = estrategia * 2 + vecindario;

                t0 = tiempo_cpu();
                inicial = constructivo(&d);
                t1 = tiempo_cpu();

                if (!inicial.sol) {
                    fprintf(stderr, "No se pudo construir una solucion factible.\n");
                    fclose(fres);
                    fclose(fsol);
                    liberar_scp(&d);
                    return EXIT_FAILURE;
                }

                final = busqueda_local(&d, &inicial,
                                       (EstrategiaBL)estrategia,
                                       (TipoVecindario)vecindario,
                                       &stats);
                t2 = tiempo_cpu();

                tiempo_greedy = t1 - t0;
                tiempo_bl = t2 - t1;
                factible = es_factible(&d, &final);

                costes[config] = final.z;
                tiempos[config] = tiempo_greedy + tiempo_bl;

                printf("%-5s %-10s | Greedy %9.4f -> Final %9.4f | "
                       "factible %d\n",
                       nombre_estrategia((EstrategiaBL)estrategia),
                       nombre_vecindario((TipoVecindario)vecindario),
                       inicial.z, final.z,
                       factible);

                snprintf(nombre, sizeof(nombre),
                         "Greedy=Primer_candidato | BL=%s | Vecindario=%s",
                         nombre_estrategia((EstrategiaBL)estrategia),
                         nombre_vecindario((TipoVecindario)vecindario));
                escribir_solucion(fsol, nombre, &final);
                fprintf(fsol, "Coste inicial Greedy: %.6f\n", inicial.z);
                fprintf(fsol, "Tiempo Greedy: %.6f segundos\n", tiempo_greedy);
                fprintf(fsol, "Tiempo busqueda local: %.6f segundos\n", tiempo_bl);
                fprintf(fsol, "Iteraciones: %d\n", stats.iteraciones);
                fprintf(fsol, "Movimientos: %d\n", stats.movimientos);
                fprintf(fsol, "Evaluaciones: %lld\n", stats.evaluaciones);
                fprintf(fsol, "Solucion factible: %s\n", factible ? "Si" : "No");

                liberar_solucion(&inicial);
                liberar_solucion(&final);
            }
        }

    fin_total = tiempo_cpu();

    /* Resumen TABLA. */
    fprintf(fres, "\n\nVALOR OBJETIVO DE LOS ALGORITMOS\n");
    fprintf(fres, "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    fprintf(fres, " Archivo  ");
    for (config = 0; config < 4; config++)
        fprintf(fres, "%-16s", nombres_config[config]);
    fprintf(fres, "\n--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    fprintf(fres, " SCP04    ");
    for (config = 0; config < 4; config++)
        fprintf(fres, "%-16.4f", costes[config]);
    fprintf(fres, "\n--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\n");

    fprintf(fres, "TIEMPOS DE CALCULO (segundos)\n");
    fprintf(fres, "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    fprintf(fres, " Archivo  ");
    for (config = 0; config < 4; config++)
        fprintf(fres, "%-16s", nombres_config[config]);
    fprintf(fres, "\n--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    fprintf(fres, " SCP04    ");
    for (config = 0; config < 4; config++)
        fprintf(fres, "%-16.6f", tiempos[config]);
    fprintf(fres, "\n--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");
    {
        const char *nombres_estrategia[2] = {"First", "Best"};
        const char *nombres_vecindario[2] = {"1-1", "1-1+1-2"};
        double coste_e[2] = {0}, tiempo_e[2] = {0};
        double coste_v[2] = {0}, tiempo_v[2] = {0};
        int e_idx, v_idx, indice;

        for (e_idx = 0; e_idx < 2; e_idx++)
            for (v_idx = 0; v_idx < 2; v_idx++) {
                indice = e_idx * 2 + v_idx;
                coste_e[e_idx] += costes[indice] / 2.0;
                tiempo_e[e_idx] += tiempos[indice] / 2.0;
                coste_v[v_idx] += costes[indice] / 2.0;
                tiempo_v[v_idx] += tiempos[indice] / 2.0;
            }

        fprintf(fres, "\nPROMEDIOS POR ESTRATEGIA DE BUSQUEDA LOCAL (2 configuraciones por estrategia)\n");
        fprintf(fres, "%-16s %18s %20s\n", "Estrategia", "Coste medio", "Tiempo medio (s)");
        for (e_idx = 0; e_idx < 2; e_idx++)
            fprintf(fres, "%-16s %18.6f %20.6f\n",
                    nombres_estrategia[e_idx], coste_e[e_idx], tiempo_e[e_idx]);

        fprintf(fres, "\nPROMEDIOS POR VECINDARIO (2 configuraciones por vecindario)\n");
        fprintf(fres, "%-16s %18s %20s\n", "Vecindario", "Coste medio", "Tiempo medio (s)");
        for (v_idx = 0; v_idx < 2; v_idx++)
            fprintf(fres, "%-16s %18.6f %20.6f\n",
                    nombres_vecindario[v_idx], coste_v[v_idx], tiempo_v[v_idx]);

    }

    fprintf(fres, "tiempo total = %9.3f\n", fin_total - inicio_total);

    fclose(fres);
    fclose(fsol);
    liberar_scp(&d);

    printf("\n============================================================\n");
    printf("Experimentos terminados.\n");
    printf("Tiempo total CPU: %.3f segundos\n", fin_total - inicio_total);
    printf("============================================================\n");

    return EXIT_SUCCESS;
}