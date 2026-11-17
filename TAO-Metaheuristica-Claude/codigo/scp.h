/* ==========================================================================
 *  scp.h  --  Fichero de cabecera comun a todo el programa
 * --------------------------------------------------------------------------
 *  Problema de Cubrimiento de Conjuntos (Set Covering Problem, SCP):
 *
 *        min  z = c'x      sujeto a   Ax >= 1,   x binaria
 *
 *  - Hay m FILAS (restricciones): cada fila es un elemento que hay que cubrir.
 *  - Hay n COLUMNAS (variables): cada columna j cuesta c(j) y cubre unas
 *    cuantas filas (las filas i con A(i,j) = 1).
 *  - Una SOLUCION es un conjunto de columnas. Es FACTIBLE si todas las filas
 *    quedan cubiertas por, al menos, una de las columnas elegidas.
 *  - Se busca la solucion factible de menor coste total.
 *
 *  Un fichero ".h" (cabecera) no contiene codigo ejecutable: solo DECLARA
 *  los tipos de datos y las funciones para que todos los ficheros ".c" los
 *  conozcan y puedan utilizarlos.
 * ========================================================================== */

#ifndef SCP_H                 /* si todavia no se ha leido esta cabecera...           */
#define SCP_H                 /* ...se marca como leida (evita incluirla dos veces)   */

#include <stdio.h>            /* entrada y salida: printf, fscanf, fopen...           */
#include <stdlib.h>           /* memoria dinamica (calloc, free), exit, atoi, atof    */
#include <string.h>           /* manejo de memoria y cadenas: memcpy, strcmp...       */
#include <math.h>             /* funciones matematicas: exp, log                      */
#include <time.h>             /* reloj del procesador: clock                          */

/* -------------------------------------------------------------------------- */
/*  CONSTANTES                                                                */
/* -------------------------------------------------------------------------- */

#define EPS      1e-9         /* tolerancia para comparar numeros reales: "a mejora a
                                 b" solo si a < b - EPS (evita falsas mejoras debidas
                                 al redondeo de los numeros en coma flotante)         */
#define MAX_LRC  64           /* tamano maximo permitido de la lista restringida de
                                 candidatos (LRC) de las construcciones aleatorizadas */
#define MAX_EXP  64           /* numero maximo de columnas que puede expulsar un solo
                                 movimiento de insercion                              */
#define MAX_K    512          /* numero maximo de columnas que se pueden quitar de
                                 una vez en una perturbacion                          */

/* -------------------------------------------------------------------------- */
/*  TIPO Instancia: los datos del problema leidos del fichero SCPXX.dat       */
/* -------------------------------------------------------------------------- */
/*  La matriz A es muy dispersa (solo ~1,5 % de unos), asi que no se guarda   */
/*  solo como tabla: se guardan dos listas compactas, una "por filas" y otra  */
/*  "por columnas", que permiten recorrer directamente los unos.              */
/*  IMPORTANTE: en el fichero las filas y columnas se numeran desde 1, pero   */
/*  dentro del programa se numeran desde 0 (como es habitual en C).           */

typedef struct {
    int     m;                /* numero de filas (elementos que hay que cubrir)       */
    int     n;                /* numero de columnas (variables de decision)           */
    int     nnz;              /* numero total de unos de la matriz A                  */
    double *coste;            /* coste[j] = coste c(j) de la columna j                */

    int    *fila_ini;         /* las columnas que cubren la fila i estan guardadas en */
    int    *fila_col;         /* fila_col[p] para p = fila_ini[i] .. fila_ini[i+1]-1  */

    int    *col_ini;          /* las filas cubiertas por la columna j estan guardadas */
    int    *col_fil;          /* en col_fil[p] para p = col_ini[j] .. col_ini[j+1]-1  */

    unsigned char *a;         /* matriz A completa guardada fila a fila: el elemento
                                 A(i,j) esta en a[i*n + j]. Sirve para responder al
                                 instante a la pregunta "cubre la columna j la fila i?" */

    /* --- Vectores auxiliares ("de trabajo") ------------------------------- */
    /* Se reservan una sola vez al leer la instancia para no pedir memoria    */
    /* continuamente. Cada funcion los deja a cero al terminar.               */
    int    *gan;              /* [n] gan[j] = n. de filas AUN SIN CUBRIR que cubriria j */
    int    *cand;             /* [n] lista de columnas candidatas a entrar            */
    unsigned char *veto;      /* [n] veto[j] = 1 si la columna j tiene prohibido entrar */
    int    *duena;            /* [m] si la fila i esta cubierta UNA sola vez,
                                     duena[i] = la unica columna que la cubre         */
    int    *nunic;            /* [n] nunic[j] = n. de filas que solo cubre la columna j */
    int    *cnt;              /* [n] contador temporal (siempre vuelve a quedar a 0)  */
    int    *lis1;             /* [n] lista temporal de columnas                       */
    int    *lis2;             /* [n] otra lista temporal de columnas                  */
} Instancia;

/* Macro que consulta la matriz A: vale 1 si la columna j cubre la fila i.    */
#define CUBRE(I, i, j)  ((I)->a[(size_t)(i) * (size_t)(I)->n + (size_t)(j)])

/* -------------------------------------------------------------------------- */
/*  TIPO Solucion: un conjunto de columnas y la informacion asociada          */
/* -------------------------------------------------------------------------- */
/*  Ademas del conjunto de columnas se mantiene SIEMPRE actualizado:          */
/*   - cuantas veces esta cubierta cada fila (cub),                           */
/*   - cuantas filas quedan sin cubrir (nsin),                                */
/*   - el coste total (z).                                                    */
/*  Asi, anadir o quitar una columna cuesta muy poco (solo se tocan las filas */
/*  de esa columna) y nunca hay que recalcular la solucion entera.            */

typedef struct {
    unsigned char *en;        /* [n] en[j] = 1 si la columna j esta en la solucion    */
    int    *cub;              /* [m] cub[i] = n. de columnas elegidas que cubren i    */
    int    *lista;            /* [n] lista[0..ncols-1] = columnas elegidas            */
    int    *pos;              /* [n] pos[j] = posicion de j dentro de lista (-1 si no) */
    int     ncols;            /* numero de columnas elegidas                          */
    int     nsin;             /* numero de filas sin cubrir (0 <=> solucion factible) */
    double  z;                /* coste total = suma de los costes de las elegidas     */
} Solucion;

/* -------------------------------------------------------------------------- */
/*  TIPO Parametros: todos los parametros configurables del programa          */
/* -------------------------------------------------------------------------- */
/*  Los valores por defecto (fijados en main.c) son los que resultaron        */
/*  mejores en la calibracion descrita en la memoria.                         */

typedef struct {
    /* --- generales -------------------------------------------------------- */
    double tiempo;            /* limite de tiempo de CPU en segundos (criterio de parada) */
    unsigned long semilla;    /* semilla del generador de numeros aleatorios          */
    int    traza;             /* 1 = escribir cada mejora encontrada (para graficas)  */
    int    ver;               /* 1 = escribir las columnas de la solucion final       */
    double t_ini;             /* instante en que empezo el algoritmo (uso interno)    */

    /* --- constructivo y busqueda local ------------------------------------ */
    int    criterio;          /* criterio voraz: 1 = coste, 2 = cobertura, 3 = cociente */
    int    entornos;          /* n. de estructuras de entornos de la busqueda local   */

    /* --- temple simulado -------------------------------------------------- */
    int    sa_k;              /* columnas que se quitan para generar una vecina       */
    int    sa_lrc;            /* tamano de la LRC al reparar la vecina                */
    double sa_p0;             /* probabilidad inicial de aceptar un empeoramiento     */
    double sa_alfa;           /* factor de enfriamiento: T <- alfa * T                */
    int    sa_L;              /* N(T): numero de vecinas generadas en cada temperatura */

    /* --- busqueda tabu ---------------------------------------------------- */
    int    ts_ten_ent;        /* duracion tabu de ENTRADA (para columnas que salen)   */
    int    ts_ten_sal;        /* duracion tabu de SALIDA (para columnas que entran)   */
    double ts_beta;           /* peso de la penalizacion por frecuencia               */
    int    ts_reinicio;       /* iteraciones sin mejorar antes de volver a s*         */

    /* --- GRASP ------------------------------------------------------------ */
    int    gr_lrc;            /* tamano de la LRC de la construccion                  */
    double gr_lam0;           /* valor inicial de lambda (peso del criterio voraz)    */
    double gr_lamf;           /* valor final de lambda                                */
    int    gr_elite;          /* tamano del conjunto elite (memoria)                  */
    int    gr_entornos;       /* n. de entornos de la busqueda local de GRASP         */

    /* --- algoritmo genetico ----------------------------------------------- */
    int    ga_pob;            /* tamano de la poblacion                               */
    int    ga_cruce;          /* tipo de cruce: 1 = un punto, 2 = uniforme, 3 = fusion */
    double ga_pmut;           /* probabilidad de que un hijo sufra una mutacion       */
    int    ga_nmut;           /* numero de genes que altera una mutacion              */
    int    ga_bl;             /* n. de entornos de la mejora local de los hijos (0 = no) */

    /* --- busqueda en entorno variable (VNS) ------------------------------- */
    int    vns_jb;            /* jB: numero de entornos de perturbacion Omega_1..Omega_jB */
    int    vns_ja;            /* jA: numero de entornos Delta_1..Delta_jA del VND     */
    int    vns_lrc;           /* tamano de la LRC al reparar tras la perturbacion     */
} Parametros;

/* -------------------------------------------------------------------------- */
/*  TIPO Resultado: lo que devuelve cada algoritmo ademas de la solucion      */
/* -------------------------------------------------------------------------- */

typedef struct {
    double t_mejor;           /* segundos de CPU en que se encontro la mejor solucion */
    long   iter;              /* numero de iteraciones realizadas                     */
} Resultado;

/* ========================================================================== */
/*  DECLARACION DE FUNCIONES (el codigo de cada una esta en el .c indicado)   */
/* ========================================================================== */

/* --- base.c: herramientas comunes ----------------------------------------- */
void   azar_semilla(unsigned long semilla);              /* inicia el generador aleatorio      */
double azar_01(void);                                   /* numero real uniforme en [0,1)      */
int    azar_entero(int k);                              /* entero uniforme en {0,...,k-1}     */
double reloj(void);                                     /* segundos de CPU consumidos         */
int    tiempo_agotado(const Parametros *P);             /* 1 si se supero el limite de tiempo */
double tiempo_usado(const Parametros *P);               /* segundos desde que empezo el alg.  */
int   *vector_int(int n);                               /* reserva un vector de n enteros     */
double *vector_double(int n);                           /* reserva un vector de n reales      */
unsigned char *vector_byte(size_t n);                   /* reserva un vector de n bytes       */

int    leer_instancia(const char *nombre, Instancia *I);/* lee un fichero SCPXX.dat           */
void   liberar_instancia(Instancia *I);                 /* devuelve la memoria de la instancia */

void   sol_crear(const Instancia *I, Solucion *S);      /* reserva memoria para una solucion  */
void   sol_liberar(Solucion *S);                        /* devuelve la memoria de la solucion */
void   sol_vaciar(const Instancia *I, Solucion *S);     /* deja la solucion sin columnas      */
void   sol_copiar(const Instancia *I, Solucion *dst, const Solucion *src);  /* dst <- src     */
void   sol_insertar(const Instancia *I, Solucion *S, int j);  /* anade la columna j           */
void   sol_quitar(const Instancia *I, Solucion *S, int j);    /* quita la columna j           */
int    sol_redundante(const Instancia *I, const Solucion *S, int j); /* 1 si j sobra          */
double sol_coste_exacto(const Instancia *I, const Solucion *S);      /* recalcula el coste    */
int    sol_comprobar(const Instancia *I, const Solucion *S);         /* 1 si todo es correcto */
int    sol_iguales(const Solucion *A, const Solucion *B);            /* 1 si mismas columnas  */

int    quitar_redundantes(const Instancia *I, Solucion *S);   /* elimina columnas sobrantes   */
void   completar(Instancia *I, Solucion *S, int criterio, int lrc,
                 const double *memoria, double lambda);       /* cubre las filas sin cubrir   */
void   perturbar(Instancia *I, Solucion *S, int k, int lrc);  /* quita k columnas y repara    */
void   anotar_mejora(const Parametros *P, double z);          /* escribe la traza (si procede) */

/* --- constructivo.c -------------------------------------------------------- */
void   constructivo_voraz(Instancia *I, Solucion *S, int criterio);

/* --- busqueda_local.c ------------------------------------------------------ */
void   calcular_unicas(Instancia *I, const Solucion *S);      /* prepara duena[] y nunic[]    */
double valor_insercion(Instancia *I, Solucion *S, int k, const unsigned char *fija,
                       int *expulsadas, int *nexp);          /* valor de insertar k          */
int    descenso(Instancia *I, Solucion *S, int entorno);      /* busqueda local en un entorno */
void   busqueda_local_vnd(Instancia *I, Solucion *S, int jA); /* descenso por entornos (VND)  */

/* --- metaheuristicas (un fichero .c para cada una) ------------------------- */
Resultado temple_simulado(Instancia *I, Solucion *mejor, Parametros *P);   /* temple.c   */
Resultado busqueda_tabu(Instancia *I, Solucion *mejor, Parametros *P);     /* tabu.c     */
Resultado grasp(Instancia *I, Solucion *mejor, Parametros *P);             /* grasp.c    */
Resultado genetico(Instancia *I, Solucion *mejor, Parametros *P);          /* genetico.c */
Resultado vns(Instancia *I, Solucion *mejor, Parametros *P);               /* vns.c      */

#endif                        /* fin de la proteccion contra la doble inclusion       */
