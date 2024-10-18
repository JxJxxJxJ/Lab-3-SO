# Informe: "Planificador de procesos" - Grupo 1 - FaMAF (2024)

Los **integrantes** que trabajaron en conjunto durante este proyecto son:

* Juan Cruz Hermosilla Artico
* Gaspar Sáenz Valiente
* Fernando Cabrera Luque

## **Índice**
 1. [Primera parte: Estudiando el planificador de xv6-riscv](#primera-parte)
    * [Pregunta 1](#preg-1)
    * [Pregunta 2](#preg-2)  
    * [Pregunta 3](#preg-3)
    * [Pregunta 4](#preg-4)
    * [Pregunta 5](#preg-5)
 2. [Segunda parte: Medir operaciones de cómputo y de entrada/salida](#segunda-parte)
 3. [Tercera parte: Asignar prioridad a los procesos](#tercera-parte)
 4. [Cuarta parte: Implementar MLFQ](#cuarta-parte)

<a name="primera-parte"></a>
## **Primera parte: Estudiando el planificador de xv6-riscv**

<a name="preg-1"></a>
### **1. ¿Qué política  de planificación utiliza `xv6-riscv` para elegir el próximo proceso a ejecutarse?**
La función `scheduler()` del módulo `proc.c` de xv6 puede darnos un indicio de la respuesta:
```c
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
```
Este es el planificador de xv6 (no muy difícil de deducir tras leer el comentario por encima de la función). Analicemos un poco el código:  
El planificador recorre todos los procesos del sistema de manera secuencial, desde `proc[0]` hasta `proc[NPROC - 1]`, buscando a alguno que esté listo para ejecutarse (`RUNNABLE`). Una vez lo encuentra, cambia su estado a `RUNNING` (es decir, lo ejecuta) y realiza el debido cambio de contexto con la función `swtch()`. Una vez que el proceso termina su ejecución o cede el control nuevamente a la CPU, el planificador continúa con el siguiente en la lista. Previo a ello, se comenta que el planificador habilita las interrupciones para permitir que dispositivos, como el temporizador, puedan interrumpir al proceso en ejecución y devolver el control a la CPU.  
Esto nos da todos los ingredientes para poder afirmar que la política de planificación que utiliza xv6 es **Round-Robin** (RR), la cual permite a un proceso ejecutarse durante un período determinado de tiempo (time slice), denominado *quantum*, para luego repetir el procedimiento con otro proceso que se encuentre listo para ejecutar. 


<a name="preg-2"></a>
### **2. ¿Cúales son los estados en los que un proceso puede permanecer en `xv6-riscv` y qué los hace cambiar de estado?**
El enunciado anterior parece habernos "spoileado" la respuesta al mostrarnos los estados `RUNNABLE` y `RUNNING`, pero aún hay más.   
Los estados en los que puede fluctuar un proceso dentro de xv6 están enumerados en el módulo `proc.h`:
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

Una forma más ilustrativa de poder ver estas transiciones entre estados en xv6 se condensa en el siguiente diagrama de estados:
![Diagrama de estados de procesos en xv6](https://ucema.edu.ar/u/jmc/siop/U1/xv6_states.png)  

**ACLARACIÓN**: *EMBRYO* es equivalente a *USED* en nuestra definición.



<a name="preg-3"></a>
### **3. ¿Qué es un *quantum*? ¿Dónde se define en el código? ¿Cuánto dura un *quantum* en `xv6-riscv`?**
Un **quantum** es el intervalo de tiempo durante el cual un proceso puede ejecutarse en la CPU antes de que el sistema operativo le interrumpa para permitir que otro proceso se ejecute.  
En xv6, el quantum está relacionado con el uso del timer interrupt (como dijimos previamente en la respuesta a la primera pregunta). El temporizador genera interrupciones periódicas que se utilizan para realizar el cambio entre procesos y permitir que el sistema planifique el próximo proceso a ejecutar.  
En el módulo `start.c` se habilitan las interrupciones, donde haremos foco en la función `timerinit()`:
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
Dicha variable especifica la cantidad de ciclos de CPU que deben ocurrir antes de que ocurra una interrupción de temporizador, los cuales son **1.000.000** en xv6. Como remarca el comentario, esto es aproximadamente *0.1 segundos* o *100 milisegundos* en QEMU.  
Sin embargo, esta duración del quantum no es absoluta. El tiempo puede variar dependiendo del procesador y la velocidad con la que este realiza las instrucciones.

<a name="preg-4"></a>
### **4. ¿En qué parte del código ocurre el cambio de contexto en `xv6-riscv`? ¿En qué funciones un proceso deja de ser ejecutado? ¿En qué funciones se elige el nuevo proceso a ejecutar?**
Como vimos previamente en el código de la función `scheduler()`, xv6 realiza un cambio de contexto a la hora de planificar o desplanificar algún proceso en el sistema. Puede verse reflejado en esta línea:  
```c
swtch(&c->context, &p->context);
```
Aquí el contexto de la CPU (`c->context`) se guarda, y el contexto del proceso `p` (`p->context`) se carga, lo que indica la ejecución del mismo.  
Por lo tanto, la función encargada de realizar los cambios de contexto en xv6 es `swtch()`, la cual se encuentra alojada en el módulo `swtch.S`.  
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
En xv6, un proceso deja de ser ejecutado cuando este cede voluntariamente la CPU o cuando es interrumpido por eventos externos (interrupciones de hardware, como un quantum agotado). Tomando como punto de apoyo al diagrama de estados de procesos que expusimos más arriba, ponemos el foco sobre aquellas transiciones donde un programa en ejecución deja de ejecutarse, es decir donde pasa de *RUNNING* hacia otro estado.  
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

* **RUNNING -> SLEEPING**: si un proceso está esperando algún evento o recurso, se pone en estado SLEEPING, mediante `sleep()`, y deja de ser ejecutado. Dicha función también se aloja en el módulo `proc.c`:
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

* **RUNNING -> ZOMBIE**: cuando un proceso termina su ejecución ya sea de manera voluntaria o forzada se cambia su estado a ZOMBIE, mediante `exit()`, y deja de ser ejecutado. Nuevamente, la función acompaña a las demás en el módulo `proc.c`:
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
En cuanto a las funciones que participan en la elección del próximo proceso a ejecutar en xv6, son dos: `scheduler()` y  `sched()`.  
Hemos desarrollado a `scheduler()` en preguntas anteriores, pero haremos una descripción corta de la función. Básicamente es el planificador de xv6, el cual recorre toda la tabla de procesos buscando alguno con estado *RUNNABLE* y lo ejecuta, dando el control al proceso mediante un cambio de contexto.  
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
Nótese que `sched()` al ser una función que devuelve el control al planificador, puede suponerse que es utilizada cuando algún proceso termina su ejecución o es interrumpido. Por lo tanto, sería empleada en las transiciones de *RUNNING* hacia otros estados. Efectivamente sucede esto, siendo empleada en las funciones previamente señaladas: `yield()`, `sleep()` y `exit()`.

<a name="preg-5"></a>
### **5. ¿El cambio de contexto consume tiempo de un *quantum*?**
Por ahora no se encontró mucha información al respecto. Esperamos que al trabajar con las mediciones se pueda esclarecer un poco la respuesta a esto.

<a name="segunda-parte"></a>
## **Segunda parte: Medir operaciones de cómputo y de entrada/salida**


<a name="tercera-parte"></a>
## **Tercera parte: Asignar prioridad a los procesos**


<a name="cuarta-parte"></a>
## **Cuarta parte: Implementar MLFQ**
