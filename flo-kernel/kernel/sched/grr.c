#include "sched.h"
#include <linux/limits.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
/*
 * grr scheduling class.
 *
 */

static atomic_t load_balance_time_slice;

static char group_path[PATH_MAX];


int ccc = 0;

static char *task_group_path(struct task_group *tg)
{
	/*
	 * May be NULL if the underlying cgroup isn't fully-created yet
	 */
	if (!tg->css.cgroup) {
		group_path[0] = '\0';
		return group_path;
	}
	cgroup_path(tg->css.cgroup, group_path, PATH_MAX);
	return group_path;
}

static char *get_tg_str(struct task_struct *p)
{
	return task_group_path(task_group(p));
}

static inline struct task_struct *grr_task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grre);
}

void printlist(struct rq *rq)
{
	struct task_struct *p;
	struct grr_rq *grr_rq = &rq->grr;
	struct sched_grr_entity *grr_se;
	struct list_head *queue = &grr_rq->queue;

	int i = 0;

	if (grr_rq == NULL) {
		printk("grr_rq is NULL\n");
		return;
	}

	list_for_each_entry(grr_se, queue, run_list) {
		p = grr_task_of(grr_se);
		trace_printk("[cpu %d] pid on this rq: %d group: %s\n",
			smp_processor_id(), p->pid, get_tg_str(p));
		i++;
	}
	//printk("[cpu %d]Size of queue: %d\n", smp_processor_id(), i);
}

static struct task_struct *find_first_movable_task(struct rq *rq, int dst_cpu)
{
	struct task_struct *p;
	struct sched_grr_entity *grr_se;
	struct list_head *curr;
	struct list_head *queue = &rq->grr.queue;

	if (!rq->grr.grr_nr_running)
		return NULL;

	list_for_each(curr, queue) {
		grr_se = list_entry(curr, struct sched_grr_entity, run_list);
		p = grr_task_of(grr_se);
		if (!task_running(rq, p) && cpumask_test_cpu(dst_cpu,
					tsk_cpus_allowed(p)))
			return p;
	}
	return NULL;
}

#ifdef CONFIG_SMP

static int fg_cpu_mask = 0;
static int bg_cpu_mask = 0;

static void move_task(struct rq *src_rq, struct rq *dst_rq,
		struct task_struct *p, int dst_cpu)
{
	trace_printk("Moving process %d from cpu %d to %d\n", p->pid,
			src_rq->cpu, dst_cpu);
	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, dst_cpu);
	activate_task(dst_rq, p, 0);
	trace_printk("Done Moving process %d from cpu %d to %d\n",
			p->pid, src_rq->cpu, dst_cpu);
}

static void task_move_group_grr(struct task_struct *p, int on_rq)
{
	trace_printk("task_move_group_grr: Task group is %s for pid %d\n",
			task_group_path(task_group(p)), p->pid);

	//printk("task_move_group_grr: Task group: %s, pid: %d, cpu: %d\n",
	//		task_group_path(task_group(p)), p->pid, task_cpu(p));
	/*
	if (strstr(p->comm, "chro")) {
		printk("FOUND BROWSER in task_move_group_grr: Task group: %s, pid: %d, cpu: %d\n",
				task_group_path(task_group(p)),
				p->pid, task_cpu(p));

		trace_printk("FOUND BROWSER: Task group: %s, pid: %d, cpu: %d\n",
				task_group_path(task_group(p)),
				p->pid, task_cpu(p));
	}
	*/
	set_task_rq(p, task_cpu(p));

}


static int
select_task_rq_grr(struct task_struct *p, int sd_flag, int flags)
{
	char *tg_str = NULL;
	int len = 0;
	int min_cpu = 0;
	unsigned long min_q_len = (unsigned long)-1;
	int curr_cpu = 0;
	int cpu_mask = 0;
	//printk(KERN_ERR "[cpu %d]select_task_rq_grr: called!\n",
	//		smp_processor_id());

	tg_str = get_tg_str(p);
	len = strlen(tg_str);
	if (len <= 5) {
		trace_printk("select_task_rq_grr: FG task: %s : %d\n", tg_str, p->pid);
		cpu_mask = fg_cpu_mask;
	} else {
		trace_printk("select_task_rq_grr: BG task: %s : %d\n", tg_str, p->pid);
		cpu_mask = bg_cpu_mask;
	}
	//for part 1 iv)
	//cpu_mask = -1;
	rcu_read_lock();
	for_each_online_cpu(curr_cpu) {
		struct rq *rq = cpu_rq(curr_cpu);
		trace_printk("No. of tasks on CPU %d = %d\n",
				curr_cpu, rq->grr.grr_nr_running);
		if (cpu_mask & 1<<curr_cpu) {
			if (rq->grr.grr_nr_running < min_q_len) {
				min_q_len = rq->grr.grr_nr_running;
				min_cpu = curr_cpu;
			}
		}
	}
	rcu_read_unlock();
	trace_printk("select_task_rq_grr: selected CPU: %d\n", min_cpu);
	return min_cpu;
}





/*
 * This function will rebalance the various queues as per the policy 
 * Periodic load balancing should be implemented such that a single job
 * from the run queue with the highest total number of tasks should be 
 * moved to the run queue with the lowest total number of tasks. The job
 * that should be moved is the first eligible job in the run queue which
 *  can be moved without causing the imbalance to reverse. Jobs that are
 *  currently running are not eligible to be moved and some jobs may have
 * restrictions on which CPU they can be run on. Load balancing should be
 * attempted every 500ms for each CPU.
 */

static void rebal_group(int cpu_mask)
{
	int i,heavy_cpu,light_cpu;
	int cpu_mask = 0;

	//for debugging
	int cpus_checked = 0;

	unsigned long max_proc_on_run_q,min_proc_on_run_q;
	//struct grr_rq *heavily_loaded_grr_rq, *lightly_loaded_grr_rq;
	struct rq *heavily_loaded_rq, *lightly_loaded_rq;
	//struct sched_grr_entity *grr_se;
	struct task_struct *p;
	unsigned long flags;
	heavily_loaded_rq = lightly_loaded_rq =  NULL;
	//heavily_loaded_grr_rq = lightly_loaded_grr_rq = NULL;
	max_proc_on_run_q = 0;
	min_proc_on_run_q = ULONG_MAX;
	heavy_cpu = light_cpu = 0;

	rcu_read_lock();
	printk("\n~~~~\n[GRR_LOADBALANCER] Checking load for %d:\n", cpu_mask);
	for_each_possible_cpu(i){
		if (cpu_mask & 1<<curr_cpu) {
			cpus_checked++;
			struct rq *this_rq = cpu_rq(i);
			struct grr_rq *grr_rq = &this_rq->grr;
			if (max_proc_on_run_q < grr_rq->grr_nr_running) {
				max_proc_on_run_q = grr_rq->grr_nr_running;
				//heavily_loaded_grr_rq = grr_rq;
				heavily_loaded_rq = this_rq;
				heavy_cpu = i;
			}
			if (grr_rq->grr_nr_running < min_proc_on_run_q) {
				min_proc_on_run_q = grr_rq->grr_nr_running;
				//lightly_loaded_grr_rq = grr_rq;
				lightly_loaded_rq = this_rq;
				light_cpu = i;
			}
		}
	}
	rcu_read_unlock();

	printk("[GRR_LOADBALANCER] %s cpus checked for %d:\n", cpus_checked, cpu_mask);
	/*condition for rebalance go ahead*/
	printk("[GRR_LOADBALANCER]In the rebalance method\n[GRR_LOADBALANCER] cpu[%d] min_proc_on_run_q:[%lu] cpu[%d] max_proc_on_run_q[%lu]\n",
			light_cpu, min_proc_on_run_q, heavy_cpu, max_proc_on_run_q);
	if (light_cpu == heavy_cpu) {
		printk("[GRR_LOADBALANCER] Same CPUs id: %d\n", light_cpu);
		return;
	}
	if ((max_proc_on_run_q - min_proc_on_run_q) > 1){
		/*lock both run queues*/
		//printk("[GRR_LOADBALANCER] moving from cpu[%d] to cpu[%d]--1\n", heavy_cpu, light_cpu);
		local_irq_save(flags);
		double_rq_lock(lightly_loaded_rq, heavily_loaded_rq);
		
		p = find_first_movable_task(heavily_loaded_rq, light_cpu);
		if (p) {
			move_task(heavily_loaded_rq, lightly_loaded_rq, p,
					light_cpu);
			printk("[GRR_LOADBALANCER] moving pid %d from cpu[%d] to cpu[%d]--2\n", p->pid, heavy_cpu, light_cpu);

		} else {
			printk("[GRR_LOADBALANCER] p is NULL\n");
		}

		double_rq_unlock(lightly_loaded_rq,
				heavily_loaded_rq);
		local_irq_restore(flags);
	}
}



static void rebalance(struct softirq_action *h)
{
	rebal_group(fg_cpu_mask);
	rebal_group(bg_cpu_mask);
}

__init void init_sched_grr_class(void)
{
	fg_cpu_mask = 0x3;
	bg_cpu_mask = 0xC;

	atomic_set(&load_balance_time_slice,GRR_LOAD_BALANCE_TIMESLICE);
        open_softirq(SCHED_GRR_SOFTIRQ, rebalance);

}
#else

__init void init_sched_grr_class(void)
{
	return;
}

#endif /* CONFIG_SMP */

void init_grr_rq(struct grr_rq *grr_rq)
{
	INIT_LIST_HEAD(&grr_rq->queue);
	grr_rq->grr_time = 0;
	grr_rq->grr_throttled = 0;
	grr_rq->grr_runtime = 0;
	grr_rq->grr_nr_running = 0;
	raw_spin_lock_init(&grr_rq->grr_runtime_lock);
}


static inline struct rq *rq_of_grr_rq(struct grr_rq *grr_rq)
{
	return container_of(grr_rq, struct rq, grr);
}

static inline struct grr_rq *grr_rq_of_se(struct sched_grr_entity *grr_se)
{
	struct task_struct *p = grr_task_of(grr_se);
	struct rq *rq = task_rq(p);

	return &rq->grr;
}

static inline int on_grr_rq(struct sched_grr_entity *grr_se)
{
	return !list_empty(&grr_se->run_list);
}

static inline u64 sched_grr_runtime(struct grr_rq *grr_rq)
{
	return grr_rq->grr_runtime;
}

typedef struct grr_rq *grr_rq_iter_t;

#define for_each_grr_rq(grr_rq, iter, rq) \
	for ((void) iter, grr_rq = &rq->grr; grr_rq; grr_rq = NULL)

static inline void sched_grr_rq_enqueue(struct grr_rq *grr_rq)
{
	if (grr_rq->grr_nr_running)
		resched_task(rq_of_grr_rq(grr_rq)->curr);
}

static struct sched_grr_entity *pick_next_grr_entity(struct rq *rq,
						   struct grr_rq *grr_rq)
{
	struct list_head *queue = &grr_rq->queue;
	struct sched_grr_entity * grr_se = list_entry(queue->next, struct
			sched_grr_entity, run_list);
	//set_next_entity done below
	grr_rq->curr = grr_se;
	return grr_se;
}

/*
 * Idle tasks are unconditionally rescheduled:
 */
//TODO: To be implemented?
static void check_preempt_curr_grr(struct rq *rq, struct task_struct *p,
		int flags)
{
	//printk(KERN_ERR "[cpu %d]check_preempt_curr_grr: called!\n",
	//smp_processor_id());
	//TODO: we don't have priority based scheduling
	//resched_task(rq->curr);
}
/*This function will pick the task of the head of the queue
and make this start running this task
NOTE: put_prev_task is always called before this function- since
put prev task will pick the running task and put it at the end of the queue
we are gauranteed to have a non-running task at the begining of the queue.
*/
static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	struct task_struct *p;
	struct grr_rq *grr_rq = &rq->grr;
	struct sched_grr_entity *grr_se;

	//if (++ccc%1000 == 0)
	//printk(KERN_ERR "[cpu %d]pick_next_task_grr: 1. called!\n",
	//smp_processor_id());

	if (!grr_rq->grr_nr_running)
		return NULL;

	//printk(KERN_ERR "[cpu %d]pick_next_task_grr: 2. called!\n",
	//smp_processor_id());
	grr_se = pick_next_grr_entity(rq, grr_rq);
	BUG_ON(!grr_se);

	p = grr_task_of(grr_se);
	p->grre.exec_start = rq->clock_task;
	return p;
}

/*
 */
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	//printk(KERN_ERR "[cpu %d]enqueue_task_grr: called!!\n",
	//		smp_processor_id());
	//trace_printk("Task group is %s for pid %d\n",
	//		task_group_path(task_group(p)), p->pid);

	//printlist(rq);

	if (flags & ENQUEUE_WAKEUP)
		grr_se->timeout = 0;

	list_add_tail(&grr_se->run_list, &grr_rq->queue);
	grr_se->on_rq = 1;

	//TODO: ???
#if 0
	if (!task_current(rq, p) && p->rt.nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
#endif

	grr_rq->grr_nr_running++;
	//printk("[GRR_ENQUEUE] grr_nr_running=[%d]\n",grr_rq->grr_nr_running);
	inc_nr_running(rq);
	//printlist(rq);
}

static void __dequeue_entity(struct sched_grr_entity *grr_se)
{
	//printk(KERN_ERR "[cpu %d]__dequeue_entity: Deleting from list\n",
	//		smp_processor_id());
	list_del(&grr_se->run_list);
}

static inline struct task_struct *task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grre);
}

static inline struct grr_rq *grr_rq_of(struct sched_grr_entity *grr_se)
{
	struct task_struct *p = task_of(grr_se);
	struct rq *rq = task_rq(p);

	return &rq->grr;
}

/*
 * It is not legal to sleep in the idle task - print a warning
 * message if some code attempts to do it:
 */
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	//printk(KERN_ERR "[cpu %d]dequeue_task_grr: called!!\n",
	//		smp_processor_id());
	//printlist(rq);
	//printk("dequeue_task_grr: grr_rq:%x, grr_rq->grr_nr_running:%d, grr_se:%x, grr_rq->curr:%x\n", smp_processor_id(), grr_rq, grr_rq->grr_nr_running, grr_se, grr_rq->curr);
	//if ((grr_rq && grr_rq->grr_nr_running) && (grr_se != grr_rq->curr)) {
	if (grr_rq && grr_rq->grr_nr_running) {
		__dequeue_entity(grr_se);
		//TODO: To verify
		grr_rq->grr_nr_running--;
		dec_nr_running(rq);
	}
	//printk("[GRR_DEQUEUE] grr_nr_running=[%d]\n",grr_rq->grr_nr_running);
	grr_se->on_rq = 0;
	//printlist(rq);
}

static void yield_task_grr(struct rq *rq)
{
	//printk(KERN_ERR "[cpu %d]yield_task_grr: called!!\n",
	//smp_processor_id());
}

static void put_prev_entity(struct grr_rq *grr_rq,
		struct sched_grr_entity *prev)
{
	if (prev->on_rq) {
		list_del(&prev->run_list);
		list_add_tail(&prev->run_list, &grr_rq->queue);
	}
	grr_rq->curr = NULL;
}

static void put_prev_task_grr(struct rq *rq, struct task_struct *prev)
{
	struct sched_grr_entity *grr_se = &prev->grre;
	struct grr_rq *grr_rq = grr_rq_of(grr_se);

	//printk(KERN_ERR "[cpu %d]put_prev_task_grr: called!!\n",
	//smp_processor_id());

	put_prev_entity(grr_rq, grr_se);
}

/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void
requeue_grr_entity(struct grr_rq *grr_rq, struct sched_grr_entity *grr_se,
		int head)
{
	if (on_grr_rq(grr_se)) {
		struct list_head *queue = &grr_rq->queue;

		if (head)
			list_move(&grr_se->run_list, queue);
		else
			list_move_tail(&grr_se->run_list, queue);
	}
}

static void requeue_task_grr(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	//printk(KERN_ERR "[cpu %d]requeue_task_grr: Requeuing pid %d\n",
	//		smp_processor_id(), p->pid);

	requeue_grr_entity(grr_rq, grr_se, head);
}

static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_grr_entity *grr_se = &p->grre;
//	if (p->grre.time_slice < 0) {
//		printk(KERN_ERR "[cpu %d]task_tick_grr: p->grre.time_slice is garbage: %d, resetting to %d\n", smp_processor_id(), p->grre.time_slice, GRR_TIMESLICE);
//		p->grre.time_slice = GRR_TIMESLICE;
//	} else {
//		printk(KERN_ERR "[cpu %d]task_tick_grr: p->grre.time_slice is valid: %d\n", smp_processor_id(), p->grre.time_slice);
//	}

	//if (++ddd%300 == 0)
//	printk(KERN_ERR "[cpu %d]task_tick_grr: called!! pid = %d pol = %d, slice = %d\n",
//		 smp_processor_id(), p->pid, p->policy, p->grre.time_slice);

//	if (p->grre.time_slice == 1)
//		printk(KERN_ERR "[cpu %d]\n\n\n\n\n\n\ntask_tick_grr: BECAME ONE, NEXT TIME ZERO\n\n\n\n\n\n\n", smp_processor_id());

	if (p->policy != SCHED_GRR)
		return;

	if (++ccc%100)
		printlist(rq);

	if (!(--p->grre.time_slice)) {
		//	printk(KERN_ERR "[cpu %d]+", smp_processor_id());
		p->grre.time_slice = GRR_TIMESLICE;
		if (grr_se->run_list.prev != grr_se->run_list.next) {
			//printk(KERN_ERR "[cpu %d]tick: Requeuing task: %d\n",
			//		smp_processor_id(), p->pid);
			requeue_task_grr(rq, p, 0);
			set_tsk_need_resched(p);
		}
	}

	#ifdef CONFIG_SMP
	atomic_dec(&load_balance_time_slice);
	
	if(!atomic_read(&load_balance_time_slice)){
		atomic_set(&load_balance_time_slice, GRR_LOAD_BALANCE_TIMESLICE);
		raise_softirq(SCHED_GRR_SOFTIRQ);				
	}
//	printk(KERN_ERR "[cpu %d]Done..\n", smp_processor_id());
	#endif /* SMP */

}

static void set_curr_task_grr(struct rq *rq)
{
	struct task_struct *p = rq->curr;
	struct sched_grr_entity *grr_se = &rq->curr->grre;
	struct grr_rq *grr_rq = grr_rq_of(grr_se);
	//TODO: Try using rq->curr

	//printk(KERN_ERR "[cpu %d]set_curr_task_grr: called!!\n",
	//		smp_processor_id());
	p->grre.exec_start = rq->clock_task;
	grr_rq->curr = grr_se;
	//printk("[cpu %d]set_curr_task_grr: grr_rq: %x, grr_rq->curr: %x, rq->curr: %x\n", smp_processor_id(), grr_rq, grr_rq->curr, rq->curr);
}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	//printk(KERN_ERR "[cpu %d]switched_to_grr: called!!\n",
	//		smp_processor_id());
	if (!p->grre.on_rq)
		return;

	if (rq->curr == p)
		resched_task(rq->curr);
	else
		check_preempt_curr(rq, p, 0);
}

static void
prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	//printk(KERN_ERR "[cpu %d]prio_changed_grr: called!!\n",
	//		smp_processor_id());
}

//TODO: Do we have to implement this?
static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	//printk(KERN_ERR "[cpu %d]get_rr_interval_grr: called!!\n",
	//		smp_processor_id());
	if (task->policy == SCHED_GRR)
                return GRR_TIMESLICE;
        else
                return 0;
}

void print_grr_stats(struct seq_file *m, int cpu)
{
	rcu_read_lock();
	print_grr_rq(m, cpu);
	rcu_read_unlock();
}


/*
 * Simple, special scheduling class for the per-CPU idle tasks:
 */
const struct sched_class grr_sched_class = {
	.next			= &fair_sched_class,
	//.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task             = yield_task_grr,

	.check_preempt_curr	= check_preempt_curr_grr,

	.pick_next_task		= pick_next_task_grr,
	.put_prev_task		= put_prev_task_grr,

#ifdef CONFIG_SMP
	.task_move_group	= task_move_group_grr,
	.select_task_rq		= select_task_rq_grr,
/*
	.set_cpus_allowed       = set_cpus_allowed_rt,
        .rq_online              = rq_online_rt,
        .rq_offline             = rq_offline_rt,
        .pre_schedule           = pre_schedule_rt,
        .post_schedule          = post_schedule_rt,
        .task_woken             = task_woken_rt,
        .switched_from          = switched_from_rt,
*/
#endif

	.set_curr_task          = set_curr_task_grr,
	.task_tick		= task_tick_grr,

	.get_rr_interval	= get_rr_interval_grr,

	.prio_changed		= prio_changed_grr,
	.switched_to		= switched_to_grr,
};
