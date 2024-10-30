# Laboratorio 3: "Planificador de procesos" - Grupo 1 - FaMAF (2024)

Los **integrantes** que desarrollaron este proyecto son: 

 - Juan Cruz Hermosilla Artico
 - Gaspar Sáenz Valiente
 - Fernando Cabrera Luque


## **Índice**
 1. [Introducción](#introduccion)
 2. [Dependencias](#dependencias)
 3. [Compilación y ejecución](#compilacion-y-ejecucion)
 4. [Informe](#informe)
 5. [Puntos extra](#puntos-extra)
 6. [Información de las branches](#branches)
 7. [Documentación empleada para el desarrollo del proyecto](#documentacion)


<a name="introduccion"></a>
## **Introducción**
Uno de los componentes fundamentales de cualquier sistema operativo es el planificador de procesos (*scheduler*), que es el responsable de decidir cuál de los procesos en espera de ejecución recibirá tiempo de CPU. Términos como Round-Robin, quantum, CPU-bound, I/O-bound, MLFQ o prioridad son bastante recurrentes en este tópico que estudiaremos en detalle. 

Este proyecto de laboratorio busca proporcionar una visión práctica y profunda sobre el diseño de políticas de planificación en sistemas operativos, y el impacto de estas decisiones en el rendimiento de procesos con distintas necesidades de recursos desde un punto de vista teórico y experimental.


<a name="dependencias"></a>
## **Dependencias**
Para poder compilar y ejecutar el proyecto es necesario tener instalado el emulador QEMU, el cual nos permite trabajar amenamente con XV6.

 - **QEMU (Ubuntu)**: `sudo apt-get install qemu-system-riscv64 gcc-riscv64-linux-gnu`
 - **Fuente monoespaciada con soporte de caracteres** unicode: [`JetBrains Mono`](https://www.jetbrains.com/es-es/lp/mono/)


<a name="compilacion-y-ejecucion"></a>
## **Compilación y ejecución**
Para compilar el proyecto, simplemente ejecutar:
``` sh
git clone https://<user>@bitbucket.org/sistop-famaf/so24lab3g01.git
cd so24lab3g01
make CPUS=1 qemu
```
Finalmente, para eliminar todos los archivos ejecutables debe utilizarse el comando:
``` sh
make clean
```
**Aclaración**: es de vital importancia colocar la flag `CPUS=1` puesto que todo el trabajo y las mediciones se realizaron lanzando la máquina virtual con un solo procesador.


<a name="informe"></a>
## **Informe**
Como actividad principal de este laboratorio se pidió realizar un informe que documente todo el trayecto del mismo desde un punto de vista teórico y experimental. Dicho informe compila desde preguntas teóricas sobre el planificador original de XV6 hasta la modificación del mismo, y el debido análisis del comportamiento que sufren los procesos en diferentes escenarios.

Toda la información documentada de este proyecto se encuentra aquí 👉 [INFORME.md](./INFORME.md)


<a name="puntos-extra"></a>
## **Puntos extra**

| Objetivos | Status |
| --- | --- |
| Reemplace la política de ascenso de  prioridad por la regla 5 de MLFQ de OSTEP: *priority boost* | :heavy_check_mark: |
| Modifique el planificador de manera que los distintos niveles de prioridad tengan distintas longitudes de *quantum* | :heavy_check_mark: |
| Modifique el planificador de manera que ponga a dormir el procesador cuando no hay procesos para planificar, utilizando la instrucción `hlt` | :heavy_check_mark: |
| Cuando xv6-riscv corre en una máquina virtual con 2 procesadores, la performance de los procesos varía significativamente según cuántos procesos haya corriendo simultáneamente. ¿Se sigue dando este fenómeno si el planificador tiene en cuenta la localidad de los procesos e intenta mantenerlos en el mismo procesador? | :x: |
| Llevar cuenta de cuánto tiempo de procesador se le ha asignado a cada proceso, con una *system call* para leer esta información desde el espacio de usuario. | :heavy_check_mark: |


<a name="branches"></a>
## **Información de las branches**
A medida que se iba avanzando en el proyecto se tomaron ciertas decisiones de implementación para mantener el orden y facilitar la recolección y análisis de datos. Una de esas decisiones fue la creación de diversas ramas a modo de facilitar el cambio entre diferentes cuantos y planificadores con solo un comando (`git checkout <rama>`). A su vez se optó por realizar una rama por cada consigna extra que se pudo resolver con el fin de no entreverar implementaciones.

Las doce ramas resultantes son las siguientes:

* `main`: rama principal del proyecto que contiene el informe completo y actualizado.
* `scheduler_vanilla_quantum_10ms`: corresponde al planificador Round-Robin original de XV6, con un cuanto de **100000**.
* `scheduler_vanilla_quantum_1ms`: corresponde al planificador Round-Robin original de XV6, con un cuanto de **10000**.
* `scheduler_vanilla_quantum_0.1ms`: corresponde al planificador Round-Robin original de XV6, con un cuanto de **1000**. 
* `mlfq`: implementación del planificador MLFQ correspondiente a la parte 3, el cual conjuga las reglas 3 y 4.
* `scheduler_mlfq_quantum_10ms`: planificador MLFQ implementado por el cual se sustituye al RR de XV6 en la parte 4, con un cuanto de **100000**. 
* `scheduler_mlfq_quantum_1ms`: planificador MLFQ implementado por el cual se sustituye al RR de XV6 en la parte 4, con un cuanto de **10000**.
* `scheduler_mlfq_quantum_0.1ms`: planificador MLFQ implementado por el cual se sustituye al RR de XV6 en la parte 4, con un cuanto de **1000**. 
* `mlfq_estrella`: contiene al **punto extra 1**.
* `mlfq_different_quantums_2_estrella`: contiene al **punto extra 2**.
* `mlfq_hlt_implementation`: contiene al **punto extra 3**.
* `mlfq_time-syscall_5_estrella`: contiene al **punto extra 5**.


<a name="documentacion"></a>
## **Documentación empleada para el desarrollo del proyecto**
- Remzi H. Arpaci-Dusseau y Andrea C. Arpaci-Dusseau (2023). [*Operating Systems: Three Easy Pieces*](https://pages.cs.wisc.edu/~remzi/OSTEP/). Hicimos un foco principal en los capítulos sobre **CPU Scheduling** y **Multilevel Feedback Queue** para tener una noción general del comportamiento del planificador Round-Robin y el MLFQ (capítulos 7 y 8 respectivamente). 
- Russ Cox, Frans Kaashoek y Robert Morris (2022). [*xv6: a simple, Unix-like teaching operating system*](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf). Donde el capítulo 7 (**Scheduling**) nos proporicionó una noción puntual y bastante teórica sobre el planificador original de XV6.
- Mythili Vutukuru. [*Process Management in xv6*](https://www.cse.iitb.ac.in/~mythili/os/notes/old-xv6/xv6-process.pdf). Pequeño artículo, cuyo aporte con los capítulos **Handling Interrupts** (capítulo 1) y **Process Scheduling** (capítulo 3) fue importante para la parte teórica.
- [*xv6-annotated*](https://github.com/palladian1/xv6-annotated). Repositorio GitHub en donde se hacen explicaciones sobre los procesos y el planificador de xv6 a nivel de código.