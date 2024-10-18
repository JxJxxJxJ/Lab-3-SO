#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#define IO_OPSIZE 64
#define IO_EXPERIMENT_LEN 512

static char data[IO_OPSIZE];

int
io_ops()
{
    int rfd, wfd;

    int pid = getpid();

    // Crear un path unico de archivo
    char path[] = "12iops";
    path[0] = '0' + (pid / 10);
    path[1] = '0' + (pid % 10);

    // <uptime start>
    wfd = open(path, O_CREATE | O_WRONLY);
    for(int i = 0; i < IO_EXPERIMENT_LEN; ++i){
      write(wfd, data, IO_OPSIZE);
    }
    close(wfd);
    // <uptime start>
    // uint64 reading_time =; 

    rfd = open(path, O_RDONLY);
    for(int i = 0; i < IO_EXPERIMENT_LEN; ++i){
      read(rfd, data, IO_OPSIZE);
    }

    close(rfd);

    return 2 * IO_EXPERIMENT_LEN;
}

void
iobench(int N, int pid)
{
  memset(data, 'a', sizeof(data));
  uint64 start_tick, end_tick, elapsed_ticks, metric;
  uint64 total_iops/* , total_kiops */;

  int *measurements = malloc(sizeof(int) * N);

  for (int i = 0; i < N; i++){
    start_tick = uptime();
    total_iops = io_ops();
    end_tick = uptime();

    elapsed_ticks = end_tick - start_tick;
    // total_kiops = total_iops / 1000;

    metric = total_iops / elapsed_ticks;
    measurements[i] = metric;
    printf("%d;[iobench];%d;%d;%d;%d\n",
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
  iobench(N, pid);

  exit(0);
}
