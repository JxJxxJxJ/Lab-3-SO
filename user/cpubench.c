#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#define CPU_MATRIX_SIZE 128
#define CPU_EXPERIMENT_LEN 256

// #define MEASURE_PERIOD 1000

// Multiplica dos matrices de tamaño CPU_MATRIX_SIZE x CPU_MATRIX_SIZE
// y devuelve la cantidad de operaciones realizadas / 1000
int 
cpu_ops_cycle() 
{
  int kops_matmul = CPU_MATRIX_SIZE * CPU_MATRIX_SIZE * CPU_MATRIX_SIZE / 1000;
  float A[CPU_MATRIX_SIZE][CPU_MATRIX_SIZE];
  float B[CPU_MATRIX_SIZE][CPU_MATRIX_SIZE];
  float C[CPU_MATRIX_SIZE][CPU_MATRIX_SIZE];

  // Inicializar matrices con valores arbitrarios
  for (int i = 0; i < CPU_MATRIX_SIZE; i++) {
    for (int j = 0; j < CPU_MATRIX_SIZE; j++) {
        A[i][j] = i + j;
        B[i][j] = i - j;
    }
  }

  // Multiplicar matrices N veces
  for (int n = 0; n < CPU_EXPERIMENT_LEN; n++) {
    for (int i = 0; i < CPU_MATRIX_SIZE; i++) {
      for (int j = 0; j < CPU_MATRIX_SIZE; j++) {
        C[i][j] = 0.0f;
        for (int k = 0; k < CPU_MATRIX_SIZE; k++) {
          C[i][j] += 2.0f * A[i][k] * B[k][j];
        }
      }
    }
  }

  return (kops_matmul * CPU_EXPERIMENT_LEN); 
  // ((128)^3 / 1000) * 256 = 536 870.912
}

void 
cpubench(int N, int pid) 
{
  uint64 start_tick, end_tick, elapsed_ticks, total_cpu_kops, metric;
  int *measurements = malloc(sizeof(int) * N);

  // Realizar N ciclos de medicion
  for(int i = 0; i < N; ++i) {
    start_tick = uptime();
    total_cpu_kops = cpu_ops_cycle();
    end_tick = uptime();

    elapsed_ticks = end_tick - start_tick;

    // metric mide la cantidad de operaciones por unidad de tiempo (ticks)
    metric = total_cpu_kops / elapsed_ticks;
    measurements[i] = metric;
    printf("%d;[cpubench];%d;%d;%d;%d\n",
           i,pid, metric, start_tick, elapsed_ticks);
  }
}

int
main(int argc, char *argv[])
{
  int N, pid;
  if (argc != 2) {
    printf("Uso: benchmark N\n");
    exit(1);
  }

  N = atoi(argv[1]);  // Número de repeticiones para los benchmarks
  pid = getpid();
  cpubench(N, pid);

  exit(0);
}
