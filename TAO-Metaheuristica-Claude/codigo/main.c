/* ==========================================================================
 *  main.c  --  Programa principal
 * --------------------------------------------------------------------------
 *  USO DESDE LA TERMINAL:
 *
 *      ./scp  FICHERO  ALGORITMO  [parametro=valor ...]
 *
 *  ALGORITMO puede ser:
 *      voraz     algoritmo constructivo voraz
 *      local     voraz + busqueda local
 *      temple    Temple Simulado
 *      tabu      Busqueda Tabu
 *      grasp     GRASP
 *      genetico  Algoritmo Genetico
 *      vns       Busqueda en Entorno Variable (GVNS)
 *
 *  EJEMPLOS:
 *      ./scp datos/SCP01.dat voraz
 *      ./scp datos/SCP01.dat temple tiempo=10 semilla=3
 *      ./scp datos/SCP01.dat vns jb=8 ver=1
 *
 *  Si se ejecuta SIN argumentos (por ejemplo, con el boton "Run" de un
 *  editor), el programa pregunta por teclado el fichero y el algoritmo.
 *
 *  La ultima linea que escribe empieza por "RESULTADO;" y contiene, separados
 *  por punto y coma: fichero; algoritmo; semilla; coste; n. de columnas;
 *  segundos hasta la mejor solucion; segundos totales; iteraciones; y un 1
 *  si la solucion ha superado la comprobacion de factibilidad.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

/* Rellena la estructura de parametros con los valores POR DEFECTO, que son */
/* los elegidos en la calibracion (vease la memoria).                       */
static void parametros_por_defecto(Parametros *P)
{
    P->tiempo      = 5.0;                          /* 5 segundos de CPU por ejecucion               */
    P->semilla     = 1;                            /* semilla aleatoria                             */
    P->traza       = 0;                            /* sin traza                                     */
    P->ver         = 0;                            /* sin listar la solucion                        */
    P->t_ini       = 0.0;                          /* se fija al arrancar el algoritmo              */

    P->criterio    = 3;                            /* voraz por cociente cobertura/coste            */
    P->entornos    = 3;                            /* busqueda local: Delta_1, Delta_2 y Delta_3    */

    P->sa_k        = 2;                            /* temple: columnas que se quitan por vecina     */
    P->sa_lrc      = 2;                            /* temple: tamano de la LRC al reparar           */
    P->sa_p0       = 0.1;                          /* temple: probabilidad inicial de aceptacion    */
    P->sa_alfa     = 0.99;                         /* temple: factor de enfriamiento                */
    P->sa_L        = 1000;                         /* temple: vecinas por temperatura               */

    P->ts_ten_ent  = 20;                           /* tabu: duracion de la prohibicion de entrar    */
    P->ts_ten_sal  = 1;                            /* tabu: duracion de la prohibicion de salir     */
    P->ts_beta     = 0.01;                         /* tabu: peso de la penalizacion por frecuencia  */
    P->ts_reinicio = 1000000;                      /* tabu: iter. sin mejora antes de reiniciar (1000000 = nunca) */

    P->gr_lrc      = 10;                           /* GRASP: tamano de la LRC                       */
    P->gr_lam0     = 1.0;                          /* GRASP: lambda inicial (solo voraz)            */
    P->gr_lamf     = 0.2;                          /* GRASP: lambda final                           */
    P->gr_elite    = 5;                            /* GRASP: tamano del conjunto elite              */
    P->gr_entornos = 3;                            /* GRASP: entornos de su busqueda local          */

    P->ga_pob      = 20;                           /* genetico: tamano de la poblacion              */
    P->ga_cruce    = 1;                            /* genetico: cruce de un punto                   */
    P->ga_pmut     = 1.0;                          /* genetico: probabilidad de mutacion            */
    P->ga_nmut     = 5;                            /* genetico: genes alterados por mutacion        */
    P->ga_bl       = 2;                            /* genetico: mejora local de los hijos (2 entornos) */

    P->vns_jb      = 10;                           /* VNS: numero de entornos de perturbacion       */
    P->vns_ja      = 3;                            /* VNS: numero de entornos del VND               */
    P->vns_lrc     = 3;                            /* VNS: tamano de la LRC al reparar              */
}

/* Interpreta un argumento de la forma "nombre=valor" y guarda el valor en  */
/* el parametro correspondiente. Devuelve 0 si el nombre no se reconoce.    */
static int leer_parametro(Parametros *P, const char *arg)
{
    char        nombre[64];                        /* parte anterior al signo '='                   */
    const char *igual = strchr(arg, '=');          /* busca el signo '=' dentro del argumento       */
    const char *valor;                             /* parte posterior al signo '='                  */
    size_t      lon;                               /* longitud del nombre                           */

    if (igual == NULL) return 0;                   /* sin '=' no es un parametro valido             */
    lon = (size_t)(igual - arg);                   /* numero de letras del nombre                   */
    if (lon == 0 || lon >= sizeof(nombre)) return 0;   /* nombre vacio o demasiado largo            */
    memcpy(nombre, arg, lon);                      /* copia el nombre...                            */
    nombre[lon] = '\0';                            /* ...y lo termina con el caracter nulo          */
    valor = igual + 1;                             /* el valor empieza justo despues del '='        */

    /* atoi convierte texto en entero; atof convierte texto en numero real  */
    if      (strcmp(nombre, "tiempo")   == 0) P->tiempo      = atof(valor);
    else if (strcmp(nombre, "semilla")  == 0) P->semilla     = (unsigned long)atol(valor);
    else if (strcmp(nombre, "traza")    == 0) P->traza       = atoi(valor);
    else if (strcmp(nombre, "ver")      == 0) P->ver         = atoi(valor);
    else if (strcmp(nombre, "criterio") == 0) P->criterio    = atoi(valor);
    else if (strcmp(nombre, "entornos") == 0) P->entornos    = atoi(valor);
    else if (strcmp(nombre, "k")        == 0) P->sa_k        = atoi(valor);
    else if (strcmp(nombre, "lrc")      == 0) { P->sa_lrc = P->gr_lrc = P->vns_lrc = atoi(valor); }
    else if (strcmp(nombre, "p0")       == 0) P->sa_p0       = atof(valor);
    else if (strcmp(nombre, "alfa")     == 0) P->sa_alfa     = atof(valor);
    else if (strcmp(nombre, "L")        == 0) P->sa_L        = atoi(valor);
    else if (strcmp(nombre, "tent")     == 0) P->ts_ten_ent  = atoi(valor);
    else if (strcmp(nombre, "tsal")     == 0) P->ts_ten_sal  = atoi(valor);
    else if (strcmp(nombre, "beta")     == 0) P->ts_beta     = atof(valor);
    else if (strcmp(nombre, "reinicio") == 0) P->ts_reinicio = atoi(valor);
    else if (strcmp(nombre, "lam0")     == 0) P->gr_lam0     = atof(valor);
    else if (strcmp(nombre, "lamf")     == 0) P->gr_lamf     = atof(valor);
    else if (strcmp(nombre, "elite")    == 0) P->gr_elite    = atoi(valor);
    else if (strcmp(nombre, "grent")    == 0) P->gr_entornos = atoi(valor);
    else if (strcmp(nombre, "pob")      == 0) P->ga_pob      = atoi(valor);
    else if (strcmp(nombre, "cruce")    == 0) P->ga_cruce    = atoi(valor);
    else if (strcmp(nombre, "pmut")     == 0) P->ga_pmut     = atof(valor);
    else if (strcmp(nombre, "nmut")     == 0) P->ga_nmut     = atoi(valor);
    else if (strcmp(nombre, "bl")       == 0) P->ga_bl       = atoi(valor);
    else if (strcmp(nombre, "jb")       == 0) P->vns_jb      = atoi(valor);
    else if (strcmp(nombre, "ja")       == 0) P->vns_ja      = atoi(valor);
    else return 0;                                 /* nombre desconocido                            */
    return 1;                                      /* parametro reconocido y guardado               */
}

/* Escribe en pantalla las instrucciones de uso. */
static void ayuda(void)
{
    printf("Uso: scp FICHERO ALGORITMO [parametro=valor ...]\n");
    printf("  ALGORITMO: voraz | local | temple | tabu | grasp | genetico | vns\n");
    printf("  Generales: tiempo=5 semilla=1 traza=0 ver=0\n");
    printf("  voraz/local: criterio=3 entornos=2\n");
    printf("  temple:   k= lrc= p0= alfa= L=\n");
    printf("  tabu:     tent= tsal= beta= reinicio=\n");
    printf("  grasp:    lrc= lam0= lamf= elite= grent=\n");
    printf("  genetico: pob= cruce= pmut= nmut= bl=\n");
    printf("  vns:      jb= ja= lrc=\n");
}

/* Funcion principal: por aqui empieza la ejecucion del programa.           */
/* argc = numero de argumentos; argv = los argumentos como textos.          */
int main(int argc, char **argv)
{
    Instancia  I;                                  /* datos del problema                            */
    Solucion   S;                                  /* mejor solucion encontrada                     */
    Parametros P;                                  /* parametros de los algoritmos                  */
    Resultado  R;                                  /* estadisticas de la ejecucion                  */
    char       fichero[512];                       /* nombre del fichero de datos                   */
    char       algoritmo[64];                      /* nombre del algoritmo elegido                  */
    double     t_total;                            /* tiempo total consumido                        */
    int        a, t, ok;                           /* contadores y resultado de la comprobacion     */

    parametros_por_defecto(&P);                    /* se cargan los parametros calibrados           */

    if (argc >= 3) {                               /* modo normal: fichero y algoritmo en la orden  */
        strncpy(fichero, argv[1], sizeof(fichero) - 1);      /* copia el nombre del fichero         */
        fichero[sizeof(fichero) - 1] = '\0';                 /* asegura el final de la cadena       */
        strncpy(algoritmo, argv[2], sizeof(algoritmo) - 1);  /* copia el nombre del algoritmo       */
        algoritmo[sizeof(algoritmo) - 1] = '\0';
        for (a = 3; a < argc; a++) {               /* el resto de argumentos son parametros         */
            if (!leer_parametro(&P, argv[a])) {    /* si alguno no se reconoce...                   */
                fprintf(stderr, "ERROR: parametro desconocido: %s\n", argv[a]);
                ayuda();                           /* ...se muestra la ayuda...                     */
                return 1;                          /* ...y se termina con codigo de error           */
            }
        }
    } else {                                       /* modo interactivo: se pregunta por teclado     */
        ayuda();
        printf("\nFichero de datos (por ejemplo datos/SCP01.dat): ");
        if (scanf("%511s", fichero) != 1) return 1;          /* lee una palabra (maximo 511 letras) */
        printf("Algoritmo: ");
        if (scanf("%63s", algoritmo) != 1) return 1;         /* lee el nombre del algoritmo         */
    }

    if (!leer_instancia(fichero, &I)) {            /* lectura de los datos del problema             */
        return 1;                                  /* si falla, el error ya se ha mostrado          */
    }
    sol_crear(&I, &S);                             /* memoria para la solucion                      */
    azar_semilla(P.semilla);                       /* se inicia el generador aleatorio              */
    R.iter = 0;                                    /* valores iniciales de las estadisticas         */
    R.t_mejor = 0.0;
    P.t_ini = reloj();                             /* EMPIEZA A CONTAR EL TIEMPO                    */

    if (strcmp(algoritmo, "voraz") == 0) {         /* --- constructivo voraz ---                    */
        constructivo_voraz(&I, &S, P.criterio);
        R.t_mejor = tiempo_usado(&P);
    } else if (strcmp(algoritmo, "local") == 0) {  /* --- voraz + busqueda local ---                */
        constructivo_voraz(&I, &S, P.criterio);
        busqueda_local_vnd(&I, &S, P.entornos);
        R.t_mejor = tiempo_usado(&P);
    } else if (strcmp(algoritmo, "temple") == 0) { /* --- temple simulado ---                       */
        R = temple_simulado(&I, &S, &P);
    } else if (strcmp(algoritmo, "tabu") == 0) {   /* --- busqueda tabu ---                         */
        R = busqueda_tabu(&I, &S, &P);
    } else if (strcmp(algoritmo, "grasp") == 0) {  /* --- GRASP ---                                 */
        R = grasp(&I, &S, &P);
    } else if (strcmp(algoritmo, "genetico") == 0) {   /* --- algoritmo genetico ---                */
        R = genetico(&I, &S, &P);
    } else if (strcmp(algoritmo, "vns") == 0) {    /* --- busqueda en entorno variable ---          */
        R = vns(&I, &S, &P);
    } else {                                       /* nombre de algoritmo desconocido               */
        fprintf(stderr, "ERROR: algoritmo desconocido: %s\n", algoritmo);
        ayuda();
        return 1;
    }

    t_total = tiempo_usado(&P);                    /* tiempo total de CPU del algoritmo             */
    S.z = sol_coste_exacto(&I, &S);                /* coste recalculado desde cero                  */
    ok = sol_comprobar(&I, &S);                    /* comprobacion independiente de la factibilidad */

    printf("Instancia : %s  (m = %d filas, n = %d columnas)\n", fichero, I.m, I.n);
    printf("Algoritmo : %s   (semilla %lu, limite %.1f s)\n", algoritmo, P.semilla, P.tiempo);
    printf("Coste     : %.4f\n", S.z);
    printf("Columnas  : %d\n", S.ncols);
    printf("Factible  : %s\n", ok ? "SI" : "NO");
    if (P.ver) {                                   /* si se pidio, se listan las columnas elegidas  */
        printf("Solucion  :");
        for (t = 0; t < S.ncols; t++) {
            printf(" %d", S.lista[t] + 1);         /* +1 para numerar desde 1 como en el fichero    */
        }
        printf("\n");
    }
    printf("RESULTADO;%s;%s;%lu;%.4f;%d;%.3f;%.3f;%ld;%d\n",   /* linea resumen para los experimentos */
           fichero, algoritmo, P.semilla, S.z, S.ncols, R.t_mejor, t_total, R.iter, ok);

    sol_liberar(&S);                               /* se devuelve la memoria                        */
    liberar_instancia(&I);
    return ok ? 0 : 2;                             /* 0 = todo correcto                             */
}
