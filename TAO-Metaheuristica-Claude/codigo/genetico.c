/* ==========================================================================
 *  genetico.c  --  Algoritmo Genetico
 * --------------------------------------------------------------------------
 *  Idea: en lugar de una sola solucion se mantiene una POBLACION de
 *  soluciones ("individuos") que evoluciona imitando la seleccion natural:
 *  los individuos se cruzan y dan hijos que heredan caracteristicas de sus
 *  padres, algunos hijos sufren mutaciones al azar y, al final de cada
 *  generacion, sobreviven con mas probabilidad los mas aptos.
 *
 *  Esquema de los apuntes ("Algoritmo general" de los Algoritmos Geneticos):
 *
 *     Construir una poblacion inicial de soluciones
 *     Guardar la mejor solucion x*
 *     Repetir {
 *         Seleccionar progenitores y aplicar el OPERADOR DE CRUCE
 *         Aplicar el OPERADOR DE MUTACION sobre las nuevas soluciones
 *         Actualizar la mejor solucion x* si se ha obtenido alguna mejor
 *         Aplicar el OPERADOR DE SELECCION
 *     } Mientras no se haya alcanzado el criterio de parada
 *
 *  Adaptacion al problema de cubrimiento:
 *    - REPRESENTACION: un individuo es el vector binario x (un "gen" por
 *      columna: 1 si la columna esta elegida). Como casi todos los genes
 *      valen 0, en memoria se guarda como el conjunto de columnas elegidas.
 *    - APTITUD: como se minimiza, es mas apto quien menos cuesta. Se usa
 *      aptitud = (coste del peor) - (coste propio) + un pequeno margen.
 *    - POBLACION INICIAL: el voraz puro mas soluciones del voraz aleatorizado
 *      (LRC de tamano 5) para que sean distintas entre si.
 *    - PROGENITORES: se baraja la poblacion y se empareja de dos en dos.
 *      Cada pareja tiene dos hijos complementarios.
 *    - CRUCE (parametro ga_cruce). Los genes en los que coinciden los padres
 *      se heredan tal cual; los demas se reparten asi:
 *        1 = un punto: hasta la columna de corte, del padre; despues, de la madre;
 *        2 = uniforme: cada gen, del padre o de la madre con probabilidad 1/2;
 *        3 = fusion:   cada gen, del progenitor mas barato con mas probabilidad.
 *    - MUTACION: con probabilidad ga_pmut se alteran ga_nmut genes del hijo.
 *    - REPARACION: un hijo puede no cubrir todas las filas; se completa con
 *      el voraz y se le quitan las columnas redundantes. Asi todos los
 *      individuos son siempre soluciones factibles.
 *    - SELECCION: de los 2*pob individuos (padres + hijos) sobrevive la
 *      mitad: el mejor siempre (elitismo) y el resto por sorteo con
 *      probabilidad proporcional a la aptitud (ruleta), sin repetidos.
 *    - Parada: limite de tiempo de CPU.
 * ========================================================================== */

#include "scp.h"                                   /* tipos y declaraciones comunes */

#define LRC_INICIAL 5                              /* tamano de la LRC para crear la poblacion inicial */

/* -------------------------------------------------------------------------- */
/*  Operador de CRUCE: genera los hijos h1 y h2 a partir de los padres a y b  */
/* -------------------------------------------------------------------------- */
static void cruzar(const Instancia *I, const Solucion *a, const Solucion *b,
                   Solucion *h1, Solucion *h2, int tipo)
{
    int    t, j, corte, de_a;                      /* indices; corte del cruce; de_a = 1 si h1 hereda de a */
    double pa;                                     /* probabilidad de heredar del padre a (fusion)  */

    sol_vaciar(I, h1);                             /* los hijos empiezan sin columnas               */
    sol_vaciar(I, h2);
    corte = 1 + azar_entero(I->n - 1);             /* punto de corte entre 1 y n-1 (cruce de un punto) */
    pa = b->z / (a->z + b->z);                     /* fusion: cuanto mas barato es a, mayor es pa   */

    for (t = 0; t < a->ncols; t++) {               /* genes que valen 1 en el padre a               */
        j = a->lista[t];
        if (b->en[j]) {                            /* tambien vale 1 en b: gen comun...             */
            sol_insertar(I, h1, j);                /* ...lo heredan los dos hijos                   */
            sol_insertar(I, h2, j);
            continue;
        }
        if (tipo == 1)      de_a = (j < corte);            /* un punto: la primera parte es de a    */
        else if (tipo == 2) de_a = (azar_01() < 0.5);      /* uniforme: a cara o cruz               */
        else                de_a = (azar_01() < pa);       /* fusion: sorteo ponderado por el coste */
        if (de_a) sol_insertar(I, h1, j);          /* h1 hereda este gen de a (un 1): columna dentro */
        else      sol_insertar(I, h2, j);          /* si no, lo hereda de a el otro hijo            */
    }
    for (t = 0; t < b->ncols; t++) {               /* genes que valen 1 solo en la madre b          */
        j = b->lista[t];
        if (a->en[j]) continue;                    /* los comunes ya estan repartidos               */
        if (tipo == 1)      de_a = (j < corte);
        else if (tipo == 2) de_a = (azar_01() < 0.5);
        else                de_a = (azar_01() < pa);
        if (de_a) sol_insertar(I, h2, j);          /* si h1 toma este gen de a (un 0), el 1 de b va a h2 */
        else      sol_insertar(I, h1, j);          /* y viceversa                                   */
    }
}

/* -------------------------------------------------------------------------- */
/*  Operador de MUTACION: altera nmut genes del individuo h                   */
/* -------------------------------------------------------------------------- */
/*  Solo ~0,5 % de los genes valen 1. Si se eligiera el gen al azar entre los */
/*  n, casi siempre se encenderia un gen y casi nunca se apagaria ninguno.    */
/*  Por eso, la mitad de las veces se apaga un gen encendido (se quita una    */
/*  columna de la solucion) y la otra mitad se enciende uno apagado.          */
static void mutar(const Instancia *I, Solucion *h, int nmut)
{
    int t, j;                                      /* contador y columna                            */
    for (t = 0; t < nmut; t++) {                   /* se alteran nmut genes                         */
        if (azar_01() < 0.5 && h->ncols > 1) {     /* cara: apagar un gen encendido                 */
            j = h->lista[azar_entero(h->ncols)];   /* columna al azar de la solucion                */
            sol_quitar(I, h, j);
        } else {                                   /* cruz: encender un gen apagado                 */
            j = azar_entero(I->n);                 /* columna al azar                               */
            if (!h->en[j]) sol_insertar(I, h, j);  /* (si ya estaba encendido no se hace nada)      */
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Algoritmo genetico                                                        */
/* -------------------------------------------------------------------------- */
Resultado genetico(Instancia *I, Solucion *mejor, Parametros *P)
{
    int       np = P->ga_pob;                      /* tamano de la poblacion                        */
    Solucion *ind;                                 /* ind[0..np-1] = poblacion; ind[np..2np-1] = hijos */
    Solucion  tmp;                                 /* para intercambiar dos individuos              */
    double   *apt;                                 /* aptitud de cada uno de los 2*np individuos    */
    int      *orden;                               /* permutacion al azar para formar las parejas   */
    Resultado R;                                   /* estadisticas                                  */
    int       i, t, e, u, total, elegido;          /* auxiliares                                    */
    double    zmin, zmax, suma, r, acum;           /* costes extremos y variables de la ruleta      */

    if (np < 4) np = 4;                            /* poblacion minima de 4 individuos              */
    if (np % 2 == 1) np++;                         /* tamano par para poder formar parejas          */
    total = 2 * np;                                /* padres + hijos                                */
    ind = (Solucion *)calloc((size_t)total, sizeof(Solucion));   /* reserva del vector de individuos */
    if (ind == NULL) {
        fprintf(stderr, "ERROR: memoria insuficiente\n");
        exit(1);
    }
    for (i = 0; i < total; i++) {                  /* memoria para cada individuo                   */
        sol_crear(I, &ind[i]);
    }
    apt   = vector_double(total);
    orden = vector_int(np);
    R.iter = 0;
    R.t_mejor = 0.0;

    /* ---------------- poblacion inicial ----------------------------------- */
    for (i = 0; i < np; i++) {
        sol_vaciar(I, &ind[i]);                    /* cada individuo se construye desde cero        */
        completar(I, &ind[i], 3,                   /* criterio del cociente                         */
                  i == 0 ? 1 : LRC_INICIAL,        /* el primero es el voraz puro; el resto, aleatorizados */
                  NULL, 1.0);
        quitar_redundantes(I, &ind[i]);
        if (i == 0 || ind[i].z < mejor->z - EPS) { /* "Guardar la mejor solucion x*"                */
            sol_copiar(I, mejor, &ind[i]);
        }
    }
    R.t_mejor = tiempo_usado(P);
    anotar_mejora(P, mejor->z);

    /* ---------------- bucle de generaciones ------------------------------- */
    while (!tiempo_agotado(P)) {
        R.iter++;                                  /* una generacion mas                            */

        /* --- seleccionar progenitores: barajar y emparejar ---------------- */
        for (i = 0; i < np; i++) orden[i] = i;     /* orden = 0, 1, 2, ..., np-1                    */
        for (i = np - 1; i > 0; i--) {             /* barajado de Fisher-Yates                      */
            u = azar_entero(i + 1);                /* posicion al azar entre 0 e i                  */
            t = orden[i];  orden[i] = orden[u];  orden[u] = t;   /* intercambio                     */
        }

        /* --- cruce, mutacion y reparacion --------------------------------- */
        for (i = 0; i < np; i += 2) {              /* pareja formada por orden[i] y orden[i+1]      */
            cruzar(I, &ind[orden[i]], &ind[orden[i + 1]],        /* los padres...                   */
                   &ind[np + i], &ind[np + i + 1], P->ga_cruce); /* ...y sus dos hijos              */
            for (e = np + i; e <= np + i + 1; e++) {             /* para cada uno de los dos hijos  */
                if (azar_01() < P->ga_pmut) {      /* con probabilidad ga_pmut...                   */
                    mutar(I, &ind[e], P->ga_nmut); /* ...el hijo muta                               */
                }
                completar(I, &ind[e], 3, 1, NULL, 1.0);          /* reparacion: cubrir lo que falte */
                quitar_redundantes(I, &ind[e]);                  /* y quitar lo que sobre           */
                if (P->ga_bl > 0) {                              /* mejora local opcional del hijo  */
                    busqueda_local_vnd(I, &ind[e], P->ga_bl);
                }
                if (ind[e].z < mejor->z - EPS) {   /* "Actualizar la mejor solucion x*"             */
                    sol_copiar(I, mejor, &ind[e]);
                    R.t_mejor = tiempo_usado(P);
                    anotar_mejora(P, mejor->z);
                }
            }
        }

        /* --- operador de seleccion: sobrevive la mitad -------------------- */
        zmin = ind[0].z;                           /* se buscan el mejor y el peor coste            */
        zmax = ind[0].z;
        for (i = 1; i < total; i++) {
            if (ind[i].z < zmin) zmin = ind[i].z;
            if (ind[i].z > zmax) zmax = ind[i].z;
        }
        for (i = 0; i < total; i++) {              /* aptitud: mayor cuanto menor es el coste       */
            apt[i] = (zmax - ind[i].z) + 0.05 * (zmax - zmin) + 1e-6;   /* (el margen evita aptitud 0) */
        }
        for (i = 1; i < total; i++) {              /* los individuos repetidos no pueden sobrevivir: */
            for (e = 0; e < i; e++) {              /* se compara cada uno con los anteriores        */
                if (apt[e] > 0.0 && fabs(ind[e].z - ind[i].z) < 1e-7
                    && sol_iguales(&ind[e], &ind[i])) {
                    apt[i] = 0.0;                  /* copia de otro: aptitud nula                   */
                    break;
                }
            }
        }
        elegido = 0;                               /* elitismo: se busca el mejor individuo         */
        for (i = 1; i < total; i++) {
            if (ind[i].z < ind[elegido].z) elegido = i;
        }
        for (t = 0; t < np; t++) {                 /* se eligen los np supervivientes uno a uno     */
            if (t > 0) {                           /* (el primero, t = 0, es el mejor: elitismo)    */
                suma = 0.0;                        /* suma de aptitudes de los que aun no han sido elegidos */
                for (i = t; i < total; i++) suma += apt[i];
                r = azar_01() * suma;              /* la "bola" de la ruleta cae en un punto al azar */
                acum = 0.0;
                elegido = total - 1;               /* por seguridad, el ultimo                      */
                for (i = t; i < total; i++) {      /* se busca a que individuo corresponde ese punto */
                    acum += apt[i];
                    if (r < acum) {
                        elegido = i;
                        break;
                    }
                }
            }
            tmp = ind[t];  ind[t] = ind[elegido];  ind[elegido] = tmp;   /* el elegido pasa a la posicion t */
            r = apt[t];  apt[t] = apt[elegido];  apt[elegido] = r;       /* y su aptitud con el             */
        }
        /* ahora ind[0..np-1] es la nueva poblacion; el resto se sobrescribira */
    }

    mejor->z = sol_coste_exacto(I, mejor);         /* coste final sin errores de redondeo           */
    for (i = 0; i < total; i++) {                  /* se devuelve toda la memoria                   */
        sol_liberar(&ind[i]);
    }
    free(ind);
    free(apt);
    free(orden);
    return R;
}
