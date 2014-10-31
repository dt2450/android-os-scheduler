#include "sched.h"

/*
 * grr scheduling class.
 *
 */

#ifdef CONFIG_SMP
//TODO: to be implemented
static int
select_task_rq_grr(struct task_struct *p, int sd_flag, int flags)
{
	printk(KERN_ERR "select_task_rq_grr: called!\n");
	return task_cpu(p); /* IDLE tasks as never migrated */
}
#endif /* CONFIG_SMP */

void init_grr_rq(struct grr_rq *grr_rq)
{
	INIT_LIST_HEAD(&grr_rq->queue);
	grr_rq->grr_time = 0;
	grr_rq->grr_throttled = 0;
	grr_rq->grr_runtime = 0;
	raw_spin_lock_init(&grr_rq->grr_runtime_lock);
}

static inline struct task_struct *grr_task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grre);
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
	return list_entry(queue->next, struct sched_grr_entity, run_list);
}

/*
 * Idle tasks are unconditionally rescheduled:
 */

static void check_preempt_curr_grr(struct rq *rq, struct task_struct *p,
		int flags)
{
	printk(KERN_ERR "check_preempt_curr_grr: called!\n");
	//TODO: if time slice has expired then
	resched_task(rq->grrt);
}

static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	struct task_struct *p;
	struct grr_rq *grr_rq = &rq->grr;
	struct sched_grr_entity *grr_se;

	printk(KERN_ERR "pick_next_task_grr: called!\n");

	if (!grr_rq->grr_nr_running)
		return NULL;

	grr_se = pick_next_grr_entity(rq, grr_rq);
	BUG_ON(!grr_se);

	p = grr_task_of(grr_se);
	p->grre.exec_start = rq->clock_task;
	return p;
}

//TODO: To be implemented from here ----
/*
 */
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;
	struct grr_rq *grr_rq = grr_rq_of_se(grr_se);

	printk(KERN_ERR "enqueue_task_grr: called!!\n");

	if (flags & ENQUEUE_WAKEUP)
		grr_se->timeout = 0;

	list_add_tail(&grr_se->run_list, &grr_rq->queue);
	//TODO: ???
#if 0
	if (!task_current(rq, p) && p->rt.nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
#endif

	inc_nr_running(rq);
}

/*
 * It is not legal to sleep in the idle task - print a warning
 * message if some code attempts to do it:
 */
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_grr_entity *grr_se = &p->grre;

	printk(KERN_ERR "dequeue_task_grr: called!!\n");
	//TODO: Update the stats
	//update_curr_rt(rq);
	//TODO: How to dequeue task?? Do we insert at end of queue?
	//dequeue_rt_entity(rt_se);

	dec_nr_running(rq);
}

static void yield_task_grr(struct rq *rq)
{
	printk(KERN_ERR "yield_task_grr: called!!\n");
}

static void put_prev_task_grr(struct rq *rq, struct task_struct *prev)
{
	printk(KERN_ERR "put_prev_task_grr: called!!\n");
}

static void task_tick_grr(struct rq *rq, struct task_struct *curr, int queued)
{
	printk(KERN_ERR "task_tick_grr: called!!\n");
}

static void set_curr_task_grr(struct rq *rq)
{
	printk(KERN_ERR "set_curr_task_grr: called!!\n");
}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	printk(KERN_ERR "switched_to_grr: called!!\n");
	BUG();
}

static void
prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	printk(KERN_ERR "prio_changed_grr: called!!\n");
	BUG();
}

static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	printk(KERN_ERR "get_rr_interval_grr: called!!\n");
	return 0;
}

/*
 * Simple, special scheduling class for the per-CPU idle tasks:
 */
const struct sched_class grr_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task             = yield_task_grr,

	.check_preempt_curr	= check_preempt_curr_grr,

	.pick_next_task		= pick_next_task_grr,
	.put_prev_task		= put_prev_task_grr,

#ifdef CONFIG_SMP
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
