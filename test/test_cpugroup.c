#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <errno.h>

#define __NR_sched_set_CPUgroup         378
int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("usage is ./test_cpugroup <numCPU> <group - 0/1>\n");
		return -1;
	}
	int num_cpu = atoi(argv[1]);
	int group = 1;
	int ret;

	printf("numcpu passed [%d], group passed [%d]\n", num_cpu, group);
	/*set cpu group numcpu, group*/
	ret = syscall(__NR_sched_set_CPUgroup, num_cpu, group);
	printf("Ret: %d\n", ret);
	if (ret == -1)
		printf("error is: %s\n", strerror(errno));
	return ret;
}

