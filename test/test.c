#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <errno.h>

#define __NR_sched_set_CPUgroup		378

int main(int argc, char **argv)
{
	int ret;

	ret = syscall(__NR_sched_set_CPUgroup, 111, 222);
	printf("Ret: %d\n", ret);
	if(ret == -1)
		printf("error is: %s\n", strerror(errno));
	while(1) {
		printf("Running now..\n");
		printf("%d\n", syscall(__NR_sched_getscheduler, getpid()));
		sched_yield();
	}
	return 0;
}
