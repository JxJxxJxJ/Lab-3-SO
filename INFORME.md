# Informe: "Planificador de procesos" - Grupo 1 - FaMAF (2024)

Los **integrantes** que trabajaron en conjunto durante este proyecto son:

* Juan Cruz Hermosilla Artico
* Gaspar Sáenz Valiente
* Fernando Cabrera Luque

## **Índice**
 1. [Primera parte: Estudiando el planificador de xv6-riscv](#primera-parte)
    * [¿Qué política de planificación utiliza xv6-riscv para elegir el próximo proceso a ejecutarse?](#preg-1)
    * [¿Cuáles son los estados en los que un proceso puede permanecer en xv6-riscv y qué los hace
cambiar de estado?](#preg-2)  
    * [¿Qué es un quantum? ¿Dónde se define en el código? ¿Cuánto dura un quantum en xv6-riscv?](#preg-3)
    * [¿En qué parte del código ocurre el cambio de contexto en xv6-riscv? ¿En qué funciones un
proceso deja de ser ejecutado? ¿En qué funciones se elige el nuevo proceso a ejecutar?](#preg-4)
    * [¿El cambio de contexto consume tiempo de un quantum?](#preg-5)
 2. [Segunda parte: Medir operaciones de cómputo y de entrada/salida](#segunda-parte)
    * [Experimento 1: ¿Cómo son planificados los programas iobound y cpubound?](#experimento-1)
    * [Experimento 2: ¿Qué sucede cuando cambiamos el largo del quantum?](#experimento-2)
 3. [Tercera parte: Implementación de MLFQ. MLFQ vs. RR](#tercera-parte)

<a name="primera-parte"></a>
## **Primera parte: Estudiando el planificador de xv6-riscv**

<a name="preg-1"></a>
### **1. ¿Qué política  de planificación utiliza `xv6-riscv` para elegir el próximo proceso a ejecutarse?**
La función `scheduler()` del módulo `proc.c` de XV6 puede darnos un indicio de la respuesta:
```c
// proc.c

// ...

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// ...
```
Este es el planificador de XV6 (no muy difícil de deducir tras leer el comentario por encima de la función). Analicemos un poco el código:  

El planificador recorre todos los procesos del sistema de manera secuencial, desde `proc[0]` hasta `proc[NPROC - 1]`, buscando a alguno que esté listo para ejecutarse (`RUNNABLE`). Una vez lo encuentra, cambia su estado a `RUNNING` (es decir, lo ejecuta) y realiza el debido cambio de contexto con la función `swtch()`. Una vez que el proceso termina su ejecución o cede el control nuevamente a la CPU, el planificador continúa con el siguiente en la lista. Previo a ello, se comenta que el planificador habilita las interrupciones para permitir que dispositivos, como el timer, puedan interrumpir al proceso en ejecución y devolver el control a la CPU. 

Esto nos da todos los ingredientes para poder afirmar que la política de planificación que utiliza XV6 es **Round-Robin** (RR), la cual permite a un proceso ejecutarse durante un período determinado de tiempo (time slice), denominado ***quantum*** o ***cuanto***, para luego repetir el procedimiento con otro proceso que se encuentre listo para ejecutar. 

El cuanto por defecto, como cada vez que se genera una interrupción por timer `scheduler()` reanuda su ejecución, puede decirse que es de `~10ms` (que es el intervalo de la interrupción por timer por defecto en XV6).


<a name="preg-2"></a>
### **2. ¿Cúales son los estados en los que un proceso puede permanecer en `xv6-riscv` y qué los hace cambiar de estado?**
El enunciado anterior parece habernos "spoileado" la respuesta al mostrarnos los estados `RUNNABLE` y `RUNNING`, pero aún hay más.   
Los estados en los que puede fluctuar un proceso dentro de XV6 están enumerados en el módulo `proc.h`:
```c
enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
```

* **UNUSED**: el proceso no está en uso. Este estado indica que el espacio reservado para el proceso no está asignado a ninguna tarea en este momento.  

* **USED**: el proceso ha sido asignado pero no está listo para ejecutarse. Esto significa que se ha reservado un espacio para el proceso en la *tabla de procesos*.  

* **SLEEPING**: el proceso está inactivo, esperando algún evento externo o condición para continuar  (por ejemplo, una operación de entrada/salida).  

* **RUNNABLE**: el proceso está listo para ser ejecutado, debe ser seleccionado por el planificador.  

* **RUNNING**: el proceso está siendo ejecutado por la CPU.  

* **ZOMBIE**: el proceso terminó de ejecutarse, pero su entrada en la tabla de procesos aún no ha sido limpiada (esperando que el padre llame a `wait()`).    

En cuanto a qué debe pasar para cambiar de un estado a otro:  

* **UNUSED -> USED**: creación de un proceso mediante `allocproc()`.  

* **USED -> RUNNABLE**: proceso listo para ejecutarse mediante `fork()`.  

* **RUNNABLE -> RUNNING**: el proceso es seleccionado por el planificador mediante `scheduler()`.  

* **RUNNING -> RUNNABLE**: cuando sucede una interrupción por timer el proceso cede el control de la CPU mediante `yield()`.  

* **RUNNING -> SLEEPING**: cuando sucede una interrupción por I/O se manda a dormir al proceso mediante `sleep()`.  

* **SLEEPING -> RUNNABLE**: se despierta al proceso dormido mediante `wakeup()` .  

* **RUNNING -> ZOMBIE**: el proceso finaliza su ejecución mediante `exit()`.  

* **ZOMBIE -> UNUSED**: el proceso padre limpia los recursos del proceso hijo finalizado mediante `wait()`.  

Una forma más ilustrativa de poder ver estas transiciones entre estados en XV6 se condensa en el siguiente diagrama de estados:
![Diagrama de estados de XV6](https://ucema.edu.ar/u/jmc/siop/U1/xv6_states.png)

**ACLARACIÓN**: *EMBRYO* es equivalente a *USED* en nuestra definición.


<a name="preg-3"></a>
### **3. ¿Qué es un *quantum*? ¿Dónde se define en el código? ¿Cuánto dura un *quantum* en `xv6-riscv`?**
Un **quantum** o **cuanto** es el intervalo de tiempo durante el cual un proceso puede ejecutarse en la CPU antes de que el sistema operativo le interrumpa para permitir que otro proceso se ejecute.  

En XV6, el cuanto está determinado indirectamente por la frecuencia en la que se realice una interrupción por timer la cual es de `~100ms`.

A continuación se explica cómo se generan interrupciones periódicas y por qué decimos que el cuanto esta ligado a éstas.

#### 3.1. ¿Cómo generar interrupciones periódicas?
En el módulo `start.c -> timerinit()` se declaran valores importantes que determinarán el comportamiento de las interrupciones por hardware en XV6:
```c
// arrange to receive timer interrupts.
// they will arrive in machine mode at
// at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
```

En particular nos llama poderosamente la atención la línea donde se define la variable `interval`:
```c
int interval = 1000000; // cycles; about 1/10th second in qemu.
``` 

Dicha variable especifica la cantidad de ciclos de CPU que deben ocurrir antes de que ocurra una interrupción por timer, los cuales son **1.000.000** en XV6. Como remarca el comentario, esto es aproximadamente *0.1 segundos* o *100 milisegundos* en QEMU.  

En `kernelvec.S` se configura el período de la interrupción por timer del cual se encargará el hardware (RISC-V) con los valores declarados en `start.c -> timerinit();` (como `interval = 1000000`). Esto se logra manipulando el CLINT.
```
"CLINT stands for Core-Local Interrupt Controller, a hardware component
                in the RISC-V architecture that provides a simple and efficient mechanism
                for generating interrupts within a core"

https://notes.yxy.ninja/Computer-Organisation/Instruction-Set-Architecture-(ISA)/RISCV/RISCV-CLINT
```

`MTIME` y `MTIMECMP` son direcciones de memoria específicas a RISC-V donde se almacenan los valores que se usan para manejar interrupciones por timer. Viven en una estructura llamada CLINT (Core-Local Interrupt Controller).

`MTIME` se actualiza por hardware constantemente, y cuando éste supera `MTIMECMP` se genera una interrupción de forma automática.

En `kernelvec.S` se va actualizando `MTIMECMP` a un valor cada vez más alto (`MTIMECMP_T+1 = MTIMECMP_t + Interval`) para generar interrupciones periódicas con un intervalo `interval`.

**Nota:** Sin embargo, esta duración del cuanto no es absoluta. El tiempo real puede variar dependiendo de cuanto tarda el procesador en actualizar el registro `MTIME`.

#### 3.2. ¿Por qué el quantum está ligado a estas interrupciones periódicas por hardware?

Estas interrupciones periódicas mediante el hardware (RISC-V) son manejadas a nivel de kernel por `trap.c -> void kerneltrap()` 
```c
// trap.c

// ...

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// ...

```

La función `devintr()` se encarga de identificar el tipo de interrupción y devuelve 2 cuando se trata de una interrupción por timer:
```c
// proc.c

// ...

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
// ...
}

// ...

```

Cuando `trap.c -> void kerneltrap()` identifica que se ha generado una interrupción por timer (es decir, `which_dev` es igual a 2), entra en el siguiente condicional donde el proceso actual "cede" (*yields*) la CPU:
```c
// trap.c

// ...

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  // ...

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) // AQUI
    yield();

  // ...
}

// ...
```

La función `yield()` establece el estado del proceso como `RUNNABLE` y llama a `sched()`:
```c
// proc.c

// ...

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// ...

```

Luego `sched()`, gracias a `swtch()`, cambia el contexto del proceso actual al contexto del planificador (apuntado por `mycpu()->context`). El cual como es ejecutado infinitamente, se logra "volverlo a correr", es decir "avanzar un cuanto". 
```c
// proc.c

// ...

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  // ... kernel panic checks
  // ...
  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// ...

```

En definitiva, el planificador cambia a un nuevo proceso cada vez que se genera una interrupción por timer, lo que, por defecto, ocurre cada `~100ms`. 
O que es lo mismo, XV6 tiene una planificación Round-Robin con un cuanto de `~100ms`.

<a name="preg-4"></a>
### **4. ¿En qué parte del código ocurre el cambio de contexto en `xv6-riscv`? ¿En qué funciones un proceso deja de ser ejecutado? ¿En qué funciones se elige el nuevo proceso a ejecutar?**

Como vimos previamente en el código de la función `scheduler()`, XV6 realiza un cambio de contexto a la hora de planificar o desplanificar algún proceso en el sistema. Puede verse reflejado en esta línea:  
```c
swtch(&c->context, &p->context);
```

Aquí el contexto de la CPU (`c->context`) se guarda, y el contexto del proceso `p` (`p->context`) se carga, lo que indica la ejecución del mismo.  
Por lo tanto, la función encargada de realizar los cambios de contexto en XV6 es `swtch()`, la cual se encuentra alojada en el módulo `swtch.S`.  
```assembly
# Context switch
#
#   void swtch(struct context *old, struct context *new);
# 
# Save current registers in old. Load from new.	

.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        
        ret
```

Nótese que es una función construida en lenguaje ensamblador que se compone de 28 instrucciones, 14 de almacenamiento o guardado (`sd`) correspondiente al contexto viejo (*old context*, como se refiere en el comentario) y otras 14 de carga (`ld`) del nuevo contexto (*new context*).  

En XV6, un proceso deja de ser ejecutado cuando este cede voluntariamente la CPU o cuando es interrumpido por eventos externos (interrupciones de hardware, como un cuanto agotado). Tomando como punto de apoyo al diagrama de estados de procesos que expusimos más arriba, ponemos el foco sobre aquellas transiciones donde un programa en ejecución deja de ejecutarse, es decir donde pasa de *RUNNING* hacia otro estado.  

* **RUNNING -> RUNNABLE**: cuando sucede una interrupción por timer el proceso cede el control de la CPU mediante `yield()`. Esta función se encuentra definida en el módulo `proc.c`:
```c
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}
```  

* **RUNNING -> SLEEPING**: si un proceso está esperando algún evento o recurso, se pone en estado `SLEEPING`, mediante `sleep()`, y deja de ser ejecutado. Dicha función también se aloja en el módulo `proc.c`:
```c
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}
```  

* **RUNNING -> ZOMBIE**: cuando un proceso termina su ejecución ya sea de manera voluntaria o forzada se cambia su estado a `ZOMBIE`, mediante `exit()`, y deja de ser ejecutado. Nuevamente, la función acompaña a las demás en el módulo `proc.c`:
```c
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
```

En cuanto a las funciones que participan en la elección del próximo proceso a ejecutar en XV6, son dos: `scheduler()` y  `sched()`.  

Hemos desarrollado a `scheduler()` en preguntas anteriores, pero haremos una descripción corta de la función. Básicamente es el planificador de XV6, el cual recorre toda la tabla de procesos buscando alguno con estado ``RUNNABLE` y lo ejecuta, dando el control al proceso mediante un cambio de contexto.  

Por otro lado, `sched()` se encarga de ceder el control nuevamente al planificador (también mediante un cambio de contexto), que luego elegirá otro proceso para ejecutar. El código es el siguiente (ubicado también en el módulo `proc.c`):
```c
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}
```

Nótese que `sched()` al ser una función que devuelve el control al planificador, puede suponerse que es utilizada cuando algún proceso termina su ejecución o es interrumpido. Por lo tanto, sería empleada en las transiciones de `RUNNING` hacia otros estados. Efectivamente sucede esto, siendo empleada en las funciones previamente señaladas: `yield()`, `sleep()` y `exit()`.

<a name="preg-5"></a>
### **5. ¿El cambio de contexto consume tiempo de un *quantum*?**
Si. Debido a que en nuestra implementación un cambio de contexto se genera cada vez que hago un timer-interrupt se genera el siguiente comportamiento:
Supongamos que tengo dos procesos: proceso `A` y proceso `B`, 
[1] <Se ejecuta el proceso A> [2] <Se genera un timer interrupt> <Se hace kerneltrap, yield, sched y se planifica otro proceso> [3] <Se ejecuta B> [4]  

Supongamos un quantum de 10m e imaginemos que el proceso `A` usó todo su quantum al comienzo.
Hay que tener en cuenta que los interrupts son periódicos, es decir se generan cada un intervalo fijo (el especificado en `start.c`).
Para analizar el ejemplo voy a llevar cuenta de dos registros importantes, MTIME y MTIMECMP que son los que determinan cuándo se hará un timer interrupt. 
* MTIME: lleva cuenta el tiempo actual (en ticks).
* MTIMECMP: cuando MTIME sea igual a este, el CPU generará una interrupción. 

Comienzo con un MTIME = 0 y MTIMECMP = 10.
[1] 
MTIME = 0
MTIMECMP = 10
[2] 
MTIME = 10 (el proceso A ha usado todo su quantum)
MTIMECMP = 10
Se genera un timer interrupt, ademas se setea el MTIMECMP en 20 (MTIME+interval=10+10) para establecer cuándo se hará la siguiente interrupción.
[3] 
MTIME = 11 (supongamos que hacer kerneltrap, yield, sched y planificar otro proceso consume 1ms)
MTIMECMP = 20
Ahora al elegirse y ejecutarse B, este tiene 9ms de tiempo de ejecución, no 10ms. 

Por lo tanto en esta implementación (usando un timer interrupt que determine cuándo cambiar de contexto) el quantum si es consumido por el tiempo usado entre la ejecución de procesos.

<a name="segunda-parte"></a>
## **Segunda parte: Medir operaciones de cómputo y de entrada/salida**
Nos parece oportuno introducir la **métrica** que empleamos a la hora de las mediciones experimentales. Debido a que la naturaleza de ambas operaciones (CPU e I/O) es diferente y no comparable, establecimos una métrica propia para cada tipo de procesos:

| Instrucción | Métrica | Código |
| :---: | :---: | :---: |
| `cpubench` | kilo-operaciones CPU por tick (**kops / ticks**) | `metric = (total_cpu_kops * scale) / elapsed_ticks`, donde `scale=1024` |
| `iobench` | operaciones I/O por tick (**iops / ticks**) | `metric = (total_iops * scale) / elapsed_ticks`, donde `scale=1024` |  

Esta elección permite comparar la carga de trabajo realizada por la CPU y el subsistema de I/O durante un intervalo de cuanto, considerándola razonable para estudiar el rendimiento en ambos casos.

Algo que quizás traiga confusión es la variable `scale` que figura en el código de la métrica. Si bien ambas instrucciones se encargan de medir operaciones por ticks, la _cantidad_ de operaciones que realiza cada una es muy diferente.  

`cpubench` trabaja sobre el conteo de operaciones de multiplicación de tres matrices, y las devuelve divididas en 1000 (kops). Esto da un indicio de que siempre se manejan números grandes con esta instrucción, y eso nos agrada de cierta forma.  

No obstante, las cosas se dificultan con `iobench`. La instrucción realiza `IO_EXPERIMENT_LEN` operaciones de lectura y `IO_EXPERIMENT_LEN` operaciones de escritura, es decir `2 * IO_EXPERIMENT_LEN` operaciones en total. En caso de que el valor de la variable sea muy pequeño y `elapsed_ticks` sea mayor a este, si se hiciera `total_iops / elapsed_ticks` resultaría en un valor aproximado a 0 (como 0.00425 por ejemplo) y XV6, al no soportar valores de punto flotante en su estructura, lo redondearía a 0 entero. Es por ello que `scale` resuelve este problema, incrementando notablemente el dividendo para evitar redondeos que no nos permitan llevar a cabo los experimentos de manera adecuada.

Luego incorporamos `scale` a `cpubench` en pos de mantener cierta uniformidad en ambas variables `metric`.  

Por otro lado, también se asignaron nombres particulares a los seis experimentos enunciados en la consigna a modo de facilitar el trabajo y análisis de datos. Los nombres claves de dichos experimentos son:

* `cpu`: `cpubench N &` (un solo proceso `cpubench` en ejecución)
* `cpu+cpu(2)`: `cpubench N &; cpubench N &; cpubench N &` (tres procesos `cpubench` ejecutándose en simultáneo)
* `cpu+io(3)`: `cpubench N &; iobench N &; iobench N &; iobench N &` (un proceso `cpubench` ejecutándose en paralelo junto a otros tres procesos `iobench`)
* `io`: `iobench N &` (un solo proceso `iobench` en ejecución)
* `io+io(2)`: `iobench N &; iobench N &; iobench N &` (tres procesos `iobench` ejecutándose en simultáneo)
* `io+cpu(3)`: `iobench N &; cpubench N &; cpubench N &; cpubench N &` (un proceso `iobench` ejecutándose en paralelo junto a otros tres procesos `cpubench`)

<a name="experimento-1"></a>
### Experimento 1: ¿Cómo son planificados los programas `iobound` y `cpubound`?

#### 1. Describa los parámetros de los programas `cpubench` e `iobench` para este experimento (o sea, los `define` al principio y el valor de `N`. Tener en cuenta que podrían cambiar en experimentos futuros, pero que si lo hacen los resultados ya no serán comparables).

Decidimos que cada programa realice 30 ciclos de medición (es decir, **N = 30**). Para escoger dicho parámetro primero establecimos un tiempo de ejecución de **1 minuto** para las instrucciones y observamos, con cronómetro mediante, la cantidad de ciclos que podía realizar nuestra CPU en dicha cantidad de tiempo. La medición inicial se realizó sobre el cuanto `Q=0.1 ms` y se trató de obtener la cantidad de ciclos que realizaban en un minuto las operaciones `cpubench` e `iobench`, obteniendo los siguientes resultados:
```
N_cpu = 31 en 1 min (0.1 ms)
N_io  = 15 en 1 min (0.1 ms)
```
Siendo `N_cpu=31` y `N_io=15` la cantidad de ciclos de ejecución que pueden realizar las operaciones `cpubench` e `iobench` en el intervalo de tiempo determinado. Posteriormente se modificó la duración del cuanto en dos ocasiones (primero `Q=1 ms` y luego `Q=10 ms`) para observar cuánto tardaba la CPU en realizar 31 ciclos `cpubench` y 15 ciclos `iobench` con un *time slice* diferente:
```
N_cpu = 31 en 11 s (1 ms)
N_io  = 15 en 24 s (1 ms)

N_cpu = 31 en 10 s (10 ms)
N_io  = 15 en 20 s (10 ms)
```
Luego nos ajustamos al peor caso (el más lento), es decir cuando el planificador tiene un cuanto `Q=0.1 ms`. En este peor caso 
```
cpubench 30 tarda ~1 minuto  (0.1 ms)
iobench  30 tarda ~2 minutos (0.1 ms)
```
Y los demás casos (`Q=1 ms`, `Q=0.1 ms`), si bien son más rápidos, siguen dándole tiempo al planificador de hacer cambios de contexto reiteradas veces. Por lo tanto, **N = 30** es una opción que nos convence para mantener a lo largo del experimento bajo diferentes cuantos.

Por otro lado, en `user/cpubench.c` no modificamos los parámetros `CPU_MATRIX_SIZE` ni `CPU_EXPERIMENT_LEN`, es decir los dejamos de la siguiente manera:
```c
#define CPU_MATRIX_SIZE 128
#define CPU_EXPERIMENT_LEN 256
```
Lo mismo hicimos con `user/iobench.c`. Tenemos:
```c
#define IO_OPSIZE 64
#define IO_EXPERIMENT_LEN 512
```
	 
#### 2. ¿Los procesos se ejecutan en paralelo? ¿En promedio, qué proceso o procesos se ejecutan primero? Hacer una **observación cualitativa**.

Al correr QEMU con `CPUS=1` estamos limitando a que no haya más de un núcleo ejecutando procesos de XV6 a la vez. Luego, no hay una ejecución paralela multinúcleo. 
Sin embargo, el núcleo de QEMU puede ir rotando entre procesos (pues QEMU tiene un planificador Round-Robin) dándole un cuanto de ejecución a cada uno hasta que todos terminan su ejecución por completo.  

Para evaluar si el planificador prioriza ejecutar procesos *CPU-bound* (como `cpubench`) o *I/O-bound* (como `iobench`) podemos ejecutarlo varias veces y analizar cuál proceso obtiene primero la CPU en base al `start_tick`, el cual indica el tick inicial en el que se empezó a ejecutar la instrucción; por ende, aquel proceso con el menor `start_tick` es aquel escogido primero por el planificador. Haremos uso de los experimentos **e** y **f**, pues emplean procesos tanto CPU-bound como I/O-bound en su ejecución.

---
**e) Caso: io+cpu(3)**
```
$ iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];9;4261364;[3380];129
0;[cpubench];7;4102357;[3379];134
0;[cpubench];10;4164514;[3381];132
0;[iobench];5;3382;[3378];310 
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];17;3926542;[6817];140
0;[cpubench];15;3765177;[6816];146
0;[cpubench];18;3844167;[6820];143
0;[iobench];13;3120;[6815];336 
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];23;3983449;[8099];138                     <-- Por que se ejecutan dos procesos en el mismo tick? } 
0;[cpubench];25;3983449;[8109];138                                                                              }
0;[cpubench];26;3954791;[8110];139                                                                              } -> los obviamos
0;[iobench];21;3084;[8099];340                           <-- Por que se ejecutan dos procesos en el mismo tick? } 
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];33;4469235;[10007];123
0;[cpubench];34;4362825;[10008];126
0;[cpubench];31;4294656;[10009];128
0;[iobench];29;3530;[10006];297
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];41;4362825;[10725];126
0;[cpubench];42;4328472;[10726];127
0;[cpubench];39;4228584;[10724];130
0;[iobench];37;3578;[10727];293
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];49;4362825;[12259];126                     <-- Por que se ejecutan dos procesos en el mismo tick? }
0;[cpubench];50;4294656;[12260];128                                                                              }
0;[cpubench];47;4164514;[12258];132                                                                              } -> los obviamos
0;[iobench];45;3518;[12258];298                           <-- Por que se ejecutan dos procesos en el mismo tick? } 
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];57;4469235;[13423];123
0;[cpubench];55;4261364;[13422];129
0;[cpubench];58;4469235;[13430];123
0;[iobench];53;3426;[13421];306
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];17;4362825;[901];126
0;[cpubench];18;4261364;[902];129
0;[cpubench];15;4228584;[903];130
0;[iobench];13;3506;[900];299
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];33;4505868;[2110];122
0;[cpubench];31;4261364;[2109];129
0;[cpubench];34;4328472;[2113];127
0;[iobench];29;3472;[2108];302
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];33;3926542;[4058];140
0;[cpubench];34;3898694;[4061];141
0;[cpubench];31;3739564;[4057];147
0;[iobench];29;3075;[4056];341
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];15;4698427;[1220];117
0;[cpubench];17;4543107;[1230];121
0;[cpubench];18;4543107;[1231];121
0;[iobench];13;3360;[1219];312
iobench 1&; cpubench 1&; cpubench 1&; cpubench 1&
$ 0;[cpubench];25;3983449;[3246];138
0;[cpubench];26;3871239;[3247];142
0;[cpubench];23;3739564;[3245];147
0;[iobench];21;3158;[3248];332
```
Donde `iobench` termina siendo planificado primero **8/10 veces**.

---

**f) Caso: cpu+io(3)**
```
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];21;9317219;[5809];59
0;[iobench];26;5041;[5812];208
0;[iobench];23;4702;[5811];223
0;[iobench];25;4002;[5811];262
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];29;9161932;[7049];60
0;[iobench];33;5065;[7050];207
0;[iobench];31;4519;[7050];232
0;[iobench];34;4599;[7058];228
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];37;9317219;[8219];59
0;[iobench];42;5729;[8224];183
0;[iobench];41;4922;[8220];213
0;[iobench];39;3785;[8220];277
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];45;9317219;[9293];59
0;[iobench];47;6721;[9294];156
0;[iobench];50;4578;[9295];229
0;[iobench];49;4032;[9292];260
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];53;10179925;[11087];54
0;[iobench];57;6678;[11089];157
0;[iobench];55;5957;[11089];176
0;[iobench];58;4161;[11090];252
$ cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];5;9317219;[323];59
0;[iobench];9;6678;[326];157
0;[iobench];10;5295;[327];198
0;[iobench];7;4405;[326];238
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];5;9477861;[165];58
0;[iobench];9;5165;[164];203
0;[iobench];10;4297;[166];244
0;[iobench];7;4211;[164];249
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];13;8084058;[1726];68
0;[iobench];15;5518;[1727];190
0;[iobench];18;4766;[1732];220
0;[iobench];17;4177;[1727];251
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];21;9011737;[2854];61
0;[iobench];26;5461;[2857];192
0;[iobench];25;4766;[2856];220
0;[iobench];23;4481;[2856];234
cpubench 1&; iobench 1&; iobench 1&; iobench 1&
$ 0;[cpubench];29;10571460;[3659];52
0;[iobench];34;6512;[3662];161
0;[iobench];31;5295;[3660];198
0;[iobench];33;5216;[3660];201

```
Donde `cpubench` termina siendo planificado primero **8/10 veces**.

Luego podríamos hipotetizar que el `<programa1>` es elegido por el planificador en la mayoría de casos de la forma:
```
<programa1> &; <programa2> &; <programa3> &; <programa 4> &
```
Es decir, el planificador escogería primero a aquel proceso ubicado al inicio de la línea de ejecución y no haría algún favoritismo en particular por un proceso CPU-bound o por uno I/O-bound.

#### 3. ¿Cambia el rendimiento de los procesos I/O-bound con respecto a la cantidad y tipo de procesos que se estén ejecutando en paralelo? ¿Por qué?
Hacemos foco en los experimentos donde intervienen procesos I/O-bound: `io`, `io+io(2)` y `io+cpu(3)`.

| Media de IOPS (io) frente a cada experimento | Media de ticks ocurridos frente a cada experimento |
| :---: | :---: |
| ![Media de IOPS, Q=10ms](https://i.imgur.com/lYG6Hvx.png) | ![Media de ticks IO, Q=10ms](https://i.imgur.com/I0f7WqV.png) |

Podemos notar dos tendencias que destacan: 

* Una para el escenario en donde solo intervienen procesos I/O-bound (en `io` e `io+io(2)`);
* Otra donde se ejecuta un proceso I/O-bound junto a otros tres procesos CPU-bound, es decir el caso `io+cpu(3)`.  

En el primer caso, las métricas de IOPS/tick y el tiempo promedio transcurrido (evaluado en ticks) no parecen variar demasiado una respecto de la otra, ya que los procesos I/O-bound no utilizan completamente su cuanto. Esto se debe a que la CPU siempre está disponible cuando los procesos lo requieren, lo que minimiza el *overhead* o sobrecarga; tener el CPU siempre disponible es comparable a ejecutarse solo.  

En el segundo caso, donde coexisten un proceso I/O-bound y tres CPU-bound, el proceso I/O-bound devuelve el CPU, pero los procesos CPU-bound utilizan su cuanto completo antes de cederlo, a diferencia del comportamiento observado en el primer caso donde lo devolvían casi al instante. Esto incrementa el tiempo que el proceso I/O-bound necesita para completar un ciclo de medición, ya que ahora debe esperar a que los otros procesos agoten su cuanto de CPU, lo cual no ocurría anteriormente.

#### 4. ¿Cambia el rendimiento de los procesos CPU-bound con respecto a la cantidad y tipo de procesos que se estén ejecutando en paralelo? ¿Por qué?
Ahora nos interesa analizar a aquellos experimentos donde intervienen procesos CPU-bound, por lo tanto nos enfocamos en `cpu`, `cpu+cpu(2)` y `cpu+io(3)`:

| Media de KOPS (cpu) frente a cada experimento | Media de ticks ocurridos frente a cada experimento |
| :---: | :---: |
| ![Media de KOPS, Q=10ms](https://i.imgur.com/wBCnmY6.png) | ![Media de ticks CPU, Q=10ms](https://i.imgur.com/B08zoje.png) |

Veamos en detalle lo que sucede en cada experimento:

* **1 proceso CPU-bound**: Este proceso utiliza su cuanto de CPU de forma completa, sin interrupciones ni competencia. Cada tick es dedicado exclusivamente a este proceso, lo que maximiza su uso del CPU.
* **3 procesos CPU-bound**: En este escenario, el CPU alterna entre tres procesos. Aunque cada proceso sigue recibiendo el mismo tiempo de ejecución por cuanto (10 ms), cada proceso debe esperar más entre uno de sus cuantums y el siguiente, ya que el CPU está ocupado ejecutando los otros dos procesos. Como resultado, cada proceso CPU-bound tiene menos oportunidades de ejecutarse en un intervalo de tiempo determinado, lo que reduce el número de operaciones por tick desde una perspectiva global.
* **1 proceso CPU-bound, 3 procesos I/O-bound**: Cuando se introduce un proceso I/O-bound, este libera el CPU antes de agotar todo su cuanto, lo que permite que los procesos CPU-bound accedan al CPU con mayor frecuencia. Por este motivo, el rendimiento en el experimento `cpu+io(3)` es superior al de `cpu(3)`, ya que los procesos CPU-bound obtienen más tiempo de CPU debido a que los procesos I/O-bound no utilizan su cuanto completamente.

#### 5. ¿Es adecuado comparar la cantidad de operaciones de CPU con la cantidad de operaciones I/O-bound?
Consideramos que no es adecuado comparar directamente estas métricas, ya que en un ciclo de medición:

- KOPS mide la cantidad de sumas y multiplicaciones en el producto de matrices (observable en la función `cpu_ops_cycle()`), y
- IOPS mide la cantidad de lecturas y escrituras sobre un archivo temporal (en la función `io_ops()`).  

Estas son operaciones esencialmente diferentes.
Si bien es posible compararlas conceptualmente en términos de "operaciones" dentro de su respectivo contexto, la comparación solo tiene sentido al observar su relación con el tiempo de ejecución relativo. Más allá de eso, no son equivalentes ni comparables directamente.  

<a name="experimento-2"></a>
### Experimento 2: ¿Qué sucede cuando cambiamos el largo del quantum?

#### 1. ¿Fue necesario modificar las métricas para que los resultados fueran comparables? ¿Por qué?  
Desde el comienzo la prioridad fue hallar una métrica que se ajuste a los distintos experimentos. El criterio primigenio que se elaboró fue uno que analizaba la cantidad de operaciones por tick, y estaba definido como:
```c
metric = total_kops / elapsed_ticks; // contabiliza las operaciones CPU-bound
metric = total_iops / elapsed_ticks; // contabiliza las operaciones I/O-bound
```
Si bien no tenía problema alguno con las operaciones de `cpubench` (por la gran cantidad de las mismas) sí lo generaba con las operaciones `iobench`. [Como ya se explicó previamente](#segunda-parte), debido a la gran cantidad de ticks en los tests I/O-bound, el ratio de operaciones/tick se aproximaba a valores cercanos a 0 (pues la cantidad de instrucciones del numerador era inferior a la cantidad de ticks en la ejecución contabilizado en el denominador), lo que imprimía una salida llena de ceros para `iops/tick`. Esto es por la limitación de tener que trabajar con valores de tipo `uint64` en XV6 debido a la ausencia de tipos `double` o `float` en su estructura, empujándonos a tener que movernos entre divisiones enteras. 

Para evitar este inconveniente, incorporamos una variable `scale` que amplifica la cantidad de operaciones en las mediciones. Posteriormente *desescalamos* los datos manualmente en las hojas de cálculo, para así obtener las `kops/tick` y las `iops/tick` de los procesos `cpubench` e `iobench` respectivamente.

#### 2. ¿Qué cambios se observan con respecto al experimento anterior? ¿Qué comportamientos se mantienen iguales?
Se llevaron a cabo los seis experimentos previos sobre diferentes cuantos en XV6. Tomaremos como muestra aquellos datos obtenidos por un procesador **11th Gen Intel(R) Core(TM) i7-1165G7**, los cuales fueron plasmados en una hoja de cálculo de Google Sheets (si se desea ver el espacio de trabajo ingrese [aquí](https://docs.google.com/spreadsheets/d/1JNGoDfsYe019ozLIFg6F-UHQfh_fU4ePKNBiQvK92Hg/edit?gid=677797133#gid=677797133)).    

Analizaremos por separado a aquellos experimentos donde se utilizan operaciones `cpubench` de aquellos con `iobench` debido a la discordancia de ambas métricas que se manifestó previamente. En el caso de experimentos mixtos tomaremos como objeto de observación al proceso que es minoría (por ejemplo, en el caso `cpu+io(3)` se analizará cómo se comportó ese único proceso `cpubench` que se ejecutó en un contexto de compartición de CPU con una mayoría de procesos I/O-bound).  

En cuanto a kilo-operaciones por tick en los procesos `cpubench`, y operaciones por tick (a secas) para los procesos `iobench` logró obtenerse la siguiente información:

| Experimentos `cpubench` | Experimentos `iobench` |
| :---: | :---: |
| ![Gráfica de CPU, Qs distintos](https://i.imgur.com/tnOBMbW.png) | ![Gráfica de IO, Qs distintos](https://i.imgur.com/9qW7k5B.png) |

Nótese que todos los experimentos, tanto los CPU-bound como los I/O-bound, muestran una caída significativa en sus operaciones a medida que se reduce el cuanto. De hecho, pareciera tener una tendencia a caer exponencialmente al ir particionando por 10 dicho cuanto, dando a la gráfica una forma decreciente si nos enfocamos solamente en cada experimento de manera particular.  

Esto se debe a que, al reducir el cuanto del planificador, este realiza muchos más cambios de contexto, interrumpiendo al proceso más frecuentemente y por lo tanto reduciendo la cantidad de operaciones que puede realizar por tick. En resumen, un cuanto más grande como el inicial (100.000) promueve un mejor rendimiento ya que los procesos en general tienen más tiempo de realizar operaciones sin ser interrumpidos; por otro lado un cuanto más pequeño introduce muchos más cambios de contexto en la ecuación, dejando poco tiempo efectivo para que las operaciones se lleven a cabo.  

Analicemos más en detalle cómo se comportan las operaciones CPU-bound a medida que se diezma el cuanto:

| Cuanto = 100.000 | Cuanto = 10.000 | Cuanto = 1.000 |
| :---: | :---: | :---: |
| ![Gráfica de CPU, Q=100000](https://i.imgur.com/F6OQmUD.png) | ![Gráfica de CPU, Q=10000](https://i.imgur.com/xdPTYVw.png) | ![Gráfica de CPU, Q=1000](https://i.imgur.com/XfJTfCb.png) |

A partir de ahora nos referiremos a los cuantos con la letra **Q** a modo de agilizar el análisis. Cuando Q = 100.000 y Q = 10.000 la gráfica presenta cierta forma de U.  

* `cpu`: corresponde al mejor rendimiento pues solo un proceso CPU se ejecuta, teniendo acceso directo a la CPU hasta que finalice su cuanto.
* `cpu+cpu(2)`: tres procesos CPU ejecutándose simultáneamente indican que existe cierta competencia por la CPU entre ellos, lo que explica la baja de rendimiento comparado a `cpu`.
* `cpu+io(3)`: dado que los procesos I/O-bound tienden a ceder la CPU mientras esperan operaciones de entrada/salida, cpubench tiene más oportunidades de utilizar la CPU comparado a `cpu+cpu(2)`, donde todos los procesos eran CPU-bound y competían por la CPU. Es razonable pensar en una mejora de rendimiento comparado con el segundo experimento, pero un poco peor que `cpu`.  

No obstante, esa forma de U muta hacia una gráfica decreciente cuando Q = 1000. Esto se debe a la excesiva cantidad de cambios de contexto, impactando negativamente en todos los experimentos. Si a eso se le suma también la competencia por la CPU que se realiza cuando se ejecutan varios progresos en simultáneo, solo llevan a que el rendimiento caiga en picada.  

Por otro lado, la reducción del cuanto afecta a las operaciones I/O-bound de la siguiente manera:

| Cuanto = 100.000 | Cuanto = 10.000 | Cuanto = 1.000 |
| :---: | :---: | :---: |
| ![Gráfica de IO, Q=100000](https://i.imgur.com/rHWYi95.png) | ![Gráfica de IO, Q=10000](https://i.imgur.com/niMf7L9.png) | ![Gráfica de IO, Q=1000](https://i.imgur.com/XAKY0UU.png) |  

Puede observarse que cuando Q = 100000, la gráfica presenta cierta forma de U invertida.  

* `io`: un solo proceso I/O-bound en ejecución, el cual se encuentra limitado por su naturaleza de pasar tiempo en espera de operaciones I/O. 
* `io+io(2)`: como la naturaleza de los procesos I/O-bound corresponde a liberar la CPU mientras esperan, permite que los otros procesos aprovechen el tiempo de CPU disponible. Esta conducta "generosa" en un contexto de compartición de la CPU aumenta el rendimiento en comparación con `io`.
* `io+cpu(3)`: la naturaleza "vil" de los procesos CPU-bound es acaparar el mayor tiempo de CPU para ellos, reduciendo el tiempo disponible para `iobench`. Esto explica porque es el peor rendimiento de los tres experimentos.  

Sin embargo, cuando Q = 10000 y Q = 1000 la gráfica tiende a tornarse decreciente. Nuevamente los cada vez más frecuentes cambios de contexto sumado a la competencia por la CPU conforman un escenario en donde el rendimiento se va desmejorando exponencialmente.  

Otra gráfica que nos parece prudente presentar es la cantidad de ticks ocurridos en cada experimento a medida que el cuanto varía:

![Gráfica de ticks](https://i.imgur.com/gcfh6wW.png)

Un patrón que se repite a lo largo de todos los experimentos es el aumento exponencial de ticks a medida que el cuanto se va diezmando. Un cuanto más pequeño genera un mayor número de ticks debido al incremento de los cambios de contexto e interrupciones, incrementando el tiempo que tardan los procesos en poder completarse.  

Tanto los procesos CPU-bound como los I/O-bound se ven notablemente afectados, sin embargo pareciera que los CPU-bound son más sensibles a estos numerosos cambios de contexto, principalmente en experimentos donde se ejecutan varios de ellos en simultáneo. Como los procesos I/O-bound tienden a liberar la CPU más rápidamente, generan una cantidad de ticks menor en general (aunque no son la excepción a la baja de rendimiento en un planificador con un cuanto pequeño).

#### 3. ¿Con un quantum más pequeño, se ven beneficiados los procesos I/O-bound o los procesos CPU-bound?
Todos los procesos se ven perjudicados de manera similar en términos de operaciones/tick, debido a que la cantidad de ticks aumenta significativamente. Además, al tener un cuanto tan pequeño, el número de cambios de contexto incrementa exponencialmente, lo que provoca que en nuestras mediciones una gran cantidad de ticks ocurridos sea consecuencia del elevado número de cambios de contexto. Los procesos CPU-bound intentan aprovechar al máximo su cuanto, aunque sea reducido, mientras que los procesos I/O-bound frecuentemente no lo utilizan por completo, pero aún incurren en el costo asociado al cambio de contexto.


<a name="tercera-parte"></a>
## **Tercera parte: Implementación de MLFQ; MLFQ vs. RR**
Con el nuevo planificador MLFQ implementado, el cual prioriza los procesos I/O-bound y penaliza de cierta forma a los procesos CPU-bound (desplazándolos a niveles de prioridad más bajos para que otros procesos puedan acceder a la CPU), se procedió a realizar las mismas mediciones de la parte 2. 

Mantenemos el criterio de analizar por separado a aquellas operaciones donde hacemos el foco en un proceso `cpubench` de aquellas que lo hacen sobre uno `iobench`, y en caso de experimentos mixtos tomamos como ejemplar al proceso solitario. Se explorará también el comportamiento de los experimentos bajo diferentes cuantos, contrastando ambos esquemas de planificación en escenarios similares.

---

![MLFQ: Gráfica de CPU](https://i.imgur.com/7CbmUU9.jpeg)

Un patrón que se mantiene en ambos planificadores es que la reducción del cuanto disminuye drásticamente las kilo-operaciones por tick, que como dedujimos previamente se debe a los cada vez más frecuentes cambios de contexto y, en algunos casos, a la competencia por la CPU entre procesos.

Con un cuanto de 100000, el planificador MLFQ muestra un rendimiento significativamente mayor al del Round-Robin. Esto es porque bajo el paradigma del RR, los procesos CPU-bound acaparan la CPU para ellos durante su tiempo asignado y no la comparten (la naturaleza "egoísta" que es inherente a estos procesos, como definimos en la parte 2). El MLFQ evita esto con su sistema de prioridades, permitiendo a los procesos CPU-bound alcanzar niveles de prioridad bajos y obtener más tiempo de CPU en sus propias colas, aumentando así la cantidad de operaciones realizadas por tick.

Al diezmar el cuanto a 10000 comienzan a verse ciertos cambios interesantes. La brecha de rendimiento ya no es abrumadoramente notable a favor del MLFQ, si no que se empareja (incluso favoreciendo al planificador Round-Robin en ciertos experimentos). Consideramos que la aparición de cambios de contexto más frecuentes propios de un cuanto más pequeño sumado a la sobrecarga de la administración de las colas de prioridad del MLFQ son los causantes de este emparejamiento.

Finalmente, con un cuanto extremadamente pequeño, la situación da un giro de 180 grados. Si bien la excesiva cantidad de cambios de contexto afecta a ambos planificadores, el RR se ve menos perjudicado pues no debe realizar cargas extra como la gestión de múltiples colas de prioridad.

Podemos concluir que el MLFQ demuestra un rendimiento superior en cuantos grandes como el de 100000. El ajuste dinámico de prioridades de los procesos CPU-bound permite que puedan aprovechar más tiempo de CPU en colas de menor prioridad y así realizar una mayor cantidad de operaciones por tick. Por otro lado, en cuantos extremadamente bajos con cambios de contexto muy frecuentes, el planificador Round-Robin tiene un desempeño superior al no tener que lidiar con la sobrecarga del MLFQ en la gestión de colas.

---

![MLFQ: Gráfica de I/O](https://i.imgur.com/TO8sG5F.jpeg)

Nuevamente, la reducción del cuanto es igual a menos operaciones por tick analizando cada experimento en particular. 

Con un cuanto de 100000 puede notarse una notable diferencia de rendimiento a favor del MLFQ. El planificador Round-Robin no favorece a los procesos I/O-bound por su naturaleza de esperar a los procesos CPU-bound, impidiendo que puedan acceder frecuentemente a la CPU. La implementación del MLFQ opta por darle una mayor prioridad de los procesos I/O-bound y dándoles más frecuencia de ejecución a diferencia de los CPU-bound que van descendiendo a colas de menor prioridad.

El "favoritismo" por los procesos I/O-bound se ve afectado a medida que se reduce el cuanto. Un cuanto de 10000 genera cambios de contexto más seguido y, sumado a la carga de gestionar las colas de prioridad, produce que el rendimiento del MLFQ se aplaste un poco, igualándolo con el obtenido con el planificador Round-Robin.

De manera similar a lo analizado en los procesos CPU-bound, un cuanto de 1000 reduce drásticamente las operaciones por tick. Este declive se nota mucho más en el MLFQ, pues aparte de la abrumadora cantidad de cambios de contexto debe sobrellevar la sobrecarga u *overhead* del sistema de administración de prioridades por colas. Así la prioridad por los procesos I/O-bound es incluso más perjudicial para el rendimiento del planificador MLFQ.

En conclusión, en los experimentos I/O-bound el MLFQ supera ampliamente a Round-Robin en IOPS. La estructura de colas de MLFQ permite que los procesos I/O-bound se ejecuten rápidamente en las colas de alta prioridad, maximizando las operaciones por tick al reducir el tiempo de espera. Un cuanto demasiado pequeño afecta drásticamente a las IOPS debido a los excesivos cambios de contexto, apenas favoreciendo al planificador RR pues no tiene una estructura de colas que administrar.

---

Por último, analicemos la cantidad de ticks ocurridos en cada experimento a través de diferentes cuantos y contrastando ambos esquemas de planificación:

![MLFQ: Gráfica de ticks](https://i.imgur.com/rMXlNPV.jpeg)

En un planificador MLFQ con un cuanto elevado los experimentos realizan mucho menos ticks en general. Esto se debe a la optimización de uso de la CPU por medio de priorizar procesos I/O-bound y permitir que los procesos CPU-bound bajen a colas de menor prioridad. En RR no existe una diferenciación en el manejo de los procesos, por lo que las operaciones requieren de más ticks para completarse.

Un cuanto intermedio aumenta los ticks en todos los experimentos debido a la aparición de cambios de contexto más frecuentes, lo que genera que la brecha de ticks entre planificadores se estabilice y no sea tan desigual. Esto indica cierta tendencia a que en medida que el cuanto se disminuye, el rendimiento del planificador MLFQ va descendiendo y el del Round-Robin "en aumento" al compararlos entre sí. 

Cuando trabajamos sobre un cuanto diminuto, MLFQ es menos efectivo en esta configuración debido a su sobrecarga de gestión de prioridades, mientras que Round-Robin mantiene una cantidad de ticks más controlada debido a su simplicidad.

---

A lo largo del análisis de las KOPS inherentes a los procesos CPU-bound, IOPS de los procesos I/O bound y la cantidad de ticks de cada experimento, notamos ciertos patrones y tendencias que nos parece prudente informar a modo de conclusión:
> Como observación general, a medida que el cuanto se disminuye la cantidad de operaciones en general también baja, y por ende aumenta la cantidad de ticks ocurridos en cada experimento. Esto sucede en ambos planificadores en particular, aunque los rendimientos entre ellos al compararlos entre sí son diferentes.

> Con un quantum elevado, el MLFQ es más eficiente tanto en términos de KOPS como de IOPS debido a su estructura de colas de prioridad. Este planificador, al diferenciar entre los tipos de proceso mediante su estructura de prioridades, permite que los procesos CPU-bound realicen más operaciones (KOPS) cuando bajan de prioridad, mientras que los procesos I/O-bound son priorizados en las colas superiores, logrando así una mayor eficiencia en IOPS y una menor carga de ticks.

> Con un quantum intermedio, el rendimiento de RR y MLFQ se va emparejando en términos de KOPS e IOPS, así como también la cantidad de ticks. Si bien ambos tienen que lidiar con la aparición más reiterada de cambios de contexto, el MLFQ debe arrastrar también la sobrecarga de la gestión de sus colas de prioridad (y el Round-Robin no), generando que esa brecha tan distante en un cuanto mayor se vaya achicando.

> Cuando el quantum es muy bajo, el MLFQ pierde eficiencia debido al *overhead* generado por su gestión de múltiples colas y prioridades, resultando en una cantidad masiva de ticks que impacta negativamente tanto en KOPS como en IOPS. En este caso, Round-Robin se comporta de manera más estable, ya que su falta de gestión de prioridades reduce el overhead, lo cual le permite manejar mejor la alta frecuencia de cambios de contexto.

#### ¿Se puede producir *starvation* en el nuevo planificador?
La inanición (o *starvation* en inglés) ocurre cuando ciertos procesos no reciben tiempo de CPU debido a que siempre hay otros procesos de mayor prioridad ejecutándose.

Con esta implementación de MLFQ se podría decir que sí se produce inanición. Por ejemplo, en un posible caso de planificación de un solo proceso I/O de gran tamaño o incluso de múltiples procesos I/O en seguidilla, estos ocuparían constantemente la mayor prioridad de colas, relegando a la cola de baja prioridad a aquellos procesos CPU por tiempos prolongados. Estos morirían de hambre puesto que el "favoritismo" que tiene este planificador por los procesos I/O-bound impediría que los procesos de colas de baja prioridad puedan adquirir la CPU normalmente.

Una forma de erradicar la inanición es mediante la aplicación de un **aumento de prioridad** o ***priority boost*** en el planificador, donde periódicamente este eleva la prioridad de todos los procesos a su nivel máximo (lo cual fue implementado y puede visualizarse en la branch `mlfq_estrella`).
