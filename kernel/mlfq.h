#ifndef MLFQ_H
#define MLFQ_H

/*
    p->times_chosen es uint, como quiero buscar el minimo el neutro del min
    es este numero.
*/
#define NPRIO            3
#define UINT32_MAX       4294967295           // Valor maximo de times_chosen
#define MIN_PRIORITY     0                    // Prioridad minima
#define NULL             ((void*)0)           // Para inicializar punteros
#define S_RESET          2000000              // Para la regla 5 MLFQ 
#define DEFAULT_INTERVAL 1000000

#endif // !MLFQ_H
