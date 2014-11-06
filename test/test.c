#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <errno.h>

#define SCHED_GRR 6
struct sched_param {
	int sched_priority;
};


int main(int argc, char **argv)
{
	int ret;
	/*set cpu group numcpu, group*/
	struct sched_param param;

	param.sched_priority = 0;
	ret = syscall(__NR_sched_setscheduler, getpid(), SCHED_GRR,
			(const struct sched_param *)&param);
	/*set scheduler here*/
	printf("Ret: %d\n", ret);
	if (ret == -1)
		printf("error is: %s\n", strerror(errno));
	while (1) {
		printf("Running now.. pid %d\n", getpid());
		printf("%ld\n", syscall(__NR_sched_getscheduler, getpid()));
	}
	return 0;

}
