
# Authors:
- Devashi Tandon
- Pratyush Parimal
- Lawrence Candes

# Files changed:

HEADER FILES:
- include/linux/sched.h
- include/linux/init_task.h
- include/linux/interrupt.h

C FILES:
- kernel/sched/grr.c
- kernel/sched/fair.c
- kernel/sched/rt.c
- kernel/sched/core.c
- kernel/sched/debug.c
- kernel/sched/sched.h

TEST FILES:

- test.c: starts a process, using GRR as default scheduler, and runs in an infinite loop.

- test_cpugroup.c: used for setting numCPU for a task_group (calls the homework syscall). Right now defaults to setting the number of CPUs specified to Foreground tasks.

# Benchmark apps used and scores:

                 Score (2 FG CPU)	      Score (3 FG CPU)
- Geekbench      591, 1095              580, 1372
- Pi(GGEMULATOR) 58.008                 44.670
- Quadrant       4088                   4203

# Analysis of General System Performance:

We switched between 1-2 and 3 CPUs for Foreground tasks.
There was a noticeable difference when we had just 1 core assigned to Foreground tasks.
As soon as we added 2 CPUs the UI became much smoother and responsive.

While running GeekBench3 we switched to just 1 CPU core the system began to stutter
and became very slow and did not respond very well. Running games on 1 CPU was not too bad.
However after running the game for a certain time period we did observe stuttering.

As soon as we enable the second core for Foreground tasks the entire android experience improved.
The geekbench3 values do reflect the real world scenario. When we have 1 core running it shows a score
of ~500 points when there are 2 cores the score is ~1100 and when we have 3 cores the score is ~1370.

We observed the same with pi benchmark and quadrant.

# Methods used to test the scheduler:

- Started the browser with multiple tabs, then set it to background.
- Downloaded and installed large apps like games and antivirus (to see background activity)
- Started own test processes (listed above) to see if enqueuing / dequeuing / balancing was happening properly.

# Visualizing scheduler load:

The code put in debug.c by us now prints statistics for the GRR scheduler, eg. the no. of tasks, their PIDs, names and group information.
To download the information from the emulator/device, use the command:

"adb pull /proc/sched_debug"

The file sched_debug contains the statistics for our GRR scheduler.
