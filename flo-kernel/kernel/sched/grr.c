#include "sched.h"
#include <linux/limits.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
/*
 * grr scheduling class.
 */

#define PART_I_ONLY 0

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
		trace_printk("grr_rq is NULL\n");
		return;
	}

	list_for_each_entry(grr_se, queue, run_list) {
		p = grr_task_of(grr_se);
		trace_printk("[cpu %d] pid on this rq: %d group: %s\n",
			smp_processor_id(), p->pid, get_tg_str(p));
		i++;
	}
}

static struct task_struct *get_first_migrateable_task
				(struct rq *rq, int dst_cpu)
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

static atomic_t fg_cpu_mask;
static atomic_t bg_cpu_mask;

static void move_task(struct rq *src_rq, struct rq *dst_rq,
		struct task_struct *p, int dst_cpu)
{
	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, dst_cpu);
	activate_task(dst_rq, p, 0);
}


/*this function will move all the tasks on the source cpu
to the destination cpu
this is generally done while assigning a cpu to one
group*/
int move_cpu_group(int source_cpu, int dest_cpu)
{
	unsigned long flags = 0;
	struct rq *src_rq = cpu_rq(source_cpu);
	struct rq *dest_rq = cpu_rq(dest_cpu);
	struct task_struct *task_to_move;

	task_to_move = get_first_migrateable_task(src_rq, dest_cpu);

	local_irq_save(flags);
	double_rq_lock(src_rq, dest_rq);
	while (task_to_move != NULL) {
		move_task(src_rq, dest_rq, task_to_move, dest_cpu);
		task_to_move = get_first_migrateable_task(src_rq, dest_cpu);
	}
	double_rq_unlock(src_rq, dest_rq);
	local_irq_restore(flags);
	return 1;
}

static void task_move_group_grr(struct task_struct *p, int on_rq)
{
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

	tg_str = get_tg_str(p);
	len = strlen(tg_str);
	if (len <= 5)
		cpu_mask = atomic_read(&fg_cpu_mask);
	else
		cpu_mask = atomic_read(&bg_cpu_mask);
	if (PART_I_ONLY)
		cpu_mask = -1;
	rcu_read_lock();
	for_each_online_cpu(curr_cpu) {
		struct rq *rq = cpu_rq(curr_cpu);

		if (cpu_mask & 1<<curr_cpu) {
			if (rq->grr.grr_nr_running < min_q_len) {
				min_q_len = rq->grr.grr_nr_running;
				min_cpu = curr_cpu;
			}
		}
	}
	rcu_read_unlock();
	return min_cpu;
}



static void migrate_task(struct rq *dst_rq, struct rq *src_rq, int dst_cpu)
{
	unsigned long flags;
	struct task_struct *p = NULL;

	local_irq_save(flags);
	double_rq_lock(dst_rq, src_rq);

	p = get_first_migrateable_task(src_rq, dst_cpu);
	if (p) {
		move_task(src_rq, dst_rq, p,
				dst_cpu);
	} else {
		trace_printk("[MIGRATE_TASK] p is NULL\n");
	}
	double_rq_unlock(dst_rq, src_rq);
	local_irq_restore(flags);
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
	int i, heavy_cpu, light_cpu;
	unsigned long max_proc_on_run_q, min_proc_on_run_q;
	struct rq *heavily_loaded_rq, *lightly_loaded_rq;

	heavily_loaded_rq = lightly_loaded_rq =  NULL;
	max_proc_on_run_q = 0;
	min_proc_on_run_q = ULONG_MAX;
	heavy_cpu = light_cpu = 0;

	rcu_read_lock();
	for_each_possible_cpu(i) {
		if (cpu_mask & (1<<i)) {
			struct rq *this_rq = cpu_rq(i);
			struct grr_rq *grr_rq = &this_rq->grr;

			if (max_proc_on_run_q < grr_rq->grr_nr_running) {
				max_proc_on_run_q = grr_rq->grr_nr_running;
				heavily_loaded_rq = this_rq;
				heavy_cpu = i;
			}
			if (grr_rq->grr_nr_running < min_proc_on_run_q) {
				min_proc_on_run_q = grr_rq->grr_nr_running;
				lightly_loaded_rq = this_rq;
				light_cpu = i;
			}
		}
	}
	rcu_read_unlock();

	/*condition for rebalance go ahead*/
	if (light_cpu == heavy_cpu)
		return;
	if ((max_proc_on_run_q - min_proc_on_run_q) > 1)
		migrate_task(lightly_loaded_rq, heavily_loaded_rq, light_cpu);
}

void steal_from_another_cpu_grr(struct rq *this_rq)
{
	struct rq *rq = this_rq;
	struct grr_rq *grr_rq = &rq->grr;
	struct rq *stolen_rq = NULL;
	int cpu, i;
	int cpu_mask = -1;
	int local_fg_cpu_mask = atomic_read(&fg_cpu_mask);
	int local_bg_cpu_mask = atomic_read(&bg_cpu_mask);

	/* steal from another CPU */
	raw_spin_unlock(&this_rq->lock);

	rcu_read_lock();
	cpu = cpu_of(rq);
	if (local_fg_cpu_mask & (1<<cpu))
		cpu_mask = local_fg_cpu_mask;
	else
		cpu_mask = local_bg_cpu_mask;
	for_each_online_cpu(i) {
		if (cpu_mask & 1<<i) {
			if (i != cpu) {
				stolen_rq = cpu_rq(i);
				grr_rq = &stolen_rq->grr;
				if (grr_rq->grr_nr_running) {
					rcu_read_unlock();
					migrate_task(rq, stolen_rq, cpu);
					raw_spin_lock(&this_rq->lock);
					return;
				}
			}
		}
	}
	rcu_read_unlock();
	raw_spin_lock(&this_rq->lock);
}


static void rebalance(struct softirq_action *h)
{
	if (PART_I_ONLY) {
		/* treat all CPUs as same */
		rebal_group(-1);
	} else {
		rebal_group(atomic_read(&fg_cpu_mask));
		rebal_group(atomic_read(&bg_cpu_mask));
	}
}

void get_cpu_masks(int *fg_mask, int *bg_mask)
{
	*fg_mask = atomic_read(&fg_cpu_mask);
	*bg_mask = atomic_read(&bg_cpu_mask);
}

void set_cpu_masks(int fg_mask, int bg_mask)
{
	atomic_set(&fg_cpu_mask, fg_mask);
	atomic_set(&bg_cpu_mask, bg_mask);
}

__init void init_sched_grr_class(void)
{
	int num_cpus = nr_cpu_ids;
	int fg_cpus = num_cpus/2;
	int bg_cpus = num_cpus - fg_cpus;
	int int_bg_cpu_mask = 0;

	if (PART_I_ONLY) {
		atomic_set(&fg_cpu_mask, -1);
		atomic_set(&bg_cpu_mask, -1);
	} else {
		atomic_set(&fg_cpu_mask, ((1<<fg_cpus)-1));
		int_bg_cpu_mask = ((1<<bg_cpus)-1);
		atomic_set(&bg_cpu_mask, (int_bg_cpu_mask << fg_cpus));
	}

	atomic_set(&load_balance_time_slice, GRR_LOAD_BALANCE_TIMESLICE);
	open_softirq(SCHED_GRR_SOFTIRQ, rebalance);
}
#else

__init void init_sched_grr_class(void)
{
	/*Do Nothing*/
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
	struct sched_grr_entity *grr_se = list_entry(queue->next, struct
			sched_grr_entity, run_list);
	/* set_next_entity done below */
	grr_rq->curr = grr_se;
	return grr_se;
}

/*
 * Idle tasks are unconditionally rescheduled:
 */
static void check_preempt_curr_grr(struct rq *rq, struct task_struct *p,
		int flags)
{
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

	if (!grr_rq->grr_nr_running)
		return NULL;

	grr_se = pick_next_grr_entity(rq, grr_rq);
	BUG_ON(!grr_se);

	p = grr_task_of(grr_se);
	return p;
}

static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	if (flags & ENQUEUE_WAKEUP)
		grr_se->timeout = 0;

	list_add_tail(&grr_se->run_list, &grr_rq->queue);
	grr_se->on_rq = 1;

	grr_rq->grr_nr_running++;
	inc_nr_running(rq);
}

static void __dequeue_entity(struct sched_grr_entity *grr_se)
{
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

static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	if (grr_rq && grr_rq->grr_nr_running) {
		__dequeue_entity(grr_se);
		grr_rq->grr_nr_running--;
		dec_nr_running(rq);
	}
	grr_se->on_rq = 0;
}

static void yield_task_grr(struct rq *rq)
{
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

	put_prev_entity(grr_rq, grr_se);
}

static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_grr_entity *grr_se = &p->grre;

	if (p->policy != SCHED_GRR)
		return;

	if (!(--p->grre.time_slice)) {
		p->grre.time_slice = GRR_TIMESLICE;
		if (grr_se->run_list.prev != grr_se->run_list.next)
			set_tsk_need_resched(p);
	}

#ifdef CONFIG_SMP
	atomic_dec(&load_balance_time_slice);

	if (!atomic_read(&load_balance_time_slice)) {
		atomic_set(&load_balance_time_slice,
				GRR_LOAD_BALANCE_TIMESLICE);
		raise_softirq(SCHED_GRR_SOFTIRQ);
	}
#endif /* SMP */

}

static void set_curr_task_grr(struct rq *rq)
{
	struct sched_grr_entity *grr_se = &rq->curr->grre;
	struct grr_rq *grr_rq = grr_rq_of(grr_se);

	grr_rq->curr = grr_se;
}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
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
}

static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	if (task->policy == SCHED_GRR)
		return GRR_TIMESLICE;
	return 0;
}

void print_grr_stats(struct seq_file *m, int cpu)
{
	rcu_read_lock();
	print_grr_rq(m, cpu);
	rcu_read_unlock();
}

/*
 * Simple, Group-based Round-Robin scheduling class for tasks:
 */
const struct sched_class grr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task             = yield_task_grr,

	.check_preempt_curr	= check_preempt_curr_grr,

	.pick_next_task		= pick_next_task_grr,
	.put_prev_task		= put_prev_task_grr,

#ifdef CONFIG_SMP
	.task_move_group	= task_move_group_grr,
	.select_task_rq		= select_task_rq_grr,
#endif

	.set_curr_task          = set_curr_task_grr,
	.task_tick		= task_tick_grr,

	.get_rr_interval	= get_rr_interval_grr,

	.prio_changed		= prio_changed_grr,
	.switched_to		= switched_to_grr,
};
