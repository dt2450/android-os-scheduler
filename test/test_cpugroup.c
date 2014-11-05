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
        int ret;
        //FOREGROUND = 1
        //BACKGROUND = 2
        //set cpu group numcpu, group
        ret = syscall(__NR_sched_set_CPUgroup, 0, 1);
        printf("Ret: %d\n", ret);
        if(ret == -1)
                printf("error is: %s\n", strerror(errno));
	
        ret = syscall(__NR_sched_set_CPUgroup, 4, 1);
        printf("Ret: %d\n", ret);
        if(ret == -1)
                printf("error is: %s\n", strerror(errno));

	

	
        ret = syscall(__NR_sched_set_CPUgroup, 3, 2);
        printf("Ret: %d\n", ret);
        if(ret == -1)
                printf("error is: %s\n", strerror(errno));
        ret = syscall(__NR_sched_set_CPUgroup, 3, 1);
        printf("Ret: %d\n", ret);
        if(ret == -1)
                printf("error is: %s\n", strerror(errno));
        ret = syscall(__NR_sched_set_CPUgroup, 2, 5);
        printf("Ret: %d\n", ret);
        if(ret == -1)
                printf("error is: %s\n", strerror(errno));
	
	return ret;
}

