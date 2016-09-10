#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char *argv[])
{
    cpu_set_t *cpusetp;
    size_t size;
    int num_cpus, cpu;

    num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%d CPUs online\n", num_cpus);

    cpusetp = CPU_ALLOC(num_cpus);
    if (cpusetp == NULL) {
        perror("CPU_ALLOC");
        exit(EXIT_FAILURE);
    }
    size = CPU_ALLOC_SIZE(num_cpus);
    CPU_ZERO_S(size, cpusetp);
    for (cpu = 0; cpu < num_cpus; cpu += 2)
        CPU_SET_S(cpu, size, cpusetp);
    printf("CPU_COUNT() of set:    %d\n", CPU_COUNT_S(size, cpusetp));
    sched_setaffinity(0, num_cpus, cpusetp);

    CPU_FREE(cpusetp);
    exit(EXIT_SUCCESS);
}
