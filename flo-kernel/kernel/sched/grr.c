#include "sched.h"

/*
 * grr scheduling class.
 *
 */

int ccc = 0;
int ddd = 0;

static inline struct task_struct *grr_task_of(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grre);
}

void printlist(struct grr_rq *grr_rq)
{
	struct list_head *p = NULL;
	struct task_struct *t = NULL;
	struct sched_grr_entity *grr_se = NULL;
	struct grr_rq *temp_grr_rq = NULL;
	int i = 0;
	
	if (grr_rq == NULL) {
		printk("grr_rq is NULL\n");
		return;
	}

	printk("Contents of queue:");
	list_for_each(p, &grr_rq->queue) {
		i++;
		/*
		temp_grr_rq = list_entry(p, struct grr_rq, queue);
		printk("temp_grr_rq: %x\n", temp_grr_rq);
		grr_se = temp_grr_rq->curr;
		printk("grr_se=%x\n", grr_se);
		t = grr_task_of(grr_se);
		printk("t=%x\n", t);
		printk(" %d", t->pid);
		*/
	}
	printk("Size of queue: %d\n", i);
}

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
	//printk(KERN_ERR "check_preempt_curr_grr: called!\n");
	//TODO: we don't have priority based scheduling
	//resched_task(rq->curr);
}

static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	struct task_struct *p;
	struct grr_rq *grr_rq = &rq->grr;
	struct sched_grr_entity *grr_se;

	//if (++ccc%1000 == 0)
	//printk(KERN_ERR "pick_next_task_grr: 1. called!\n");

	if (!grr_rq->grr_nr_running)
		return NULL;

	//printk(KERN_ERR "pick_next_task_grr: 2. called!\n");
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

	printk(KERN_ERR "enqueue_task_grr: called!!\n");
	printlist(grr_rq);

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
	inc_nr_running(rq);
	printlist(grr_rq);
}

static void __dequeue_entity(struct sched_grr_entity *grr_se)
{
	printk(KERN_ERR "__dequeue_entity: Deleting from list\n");
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

	printk(KERN_ERR "dequeue_task_grr: called!!\n");
	printlist(grr_rq);
	printk("dequeue_task_grr: grr_rq:%x, grr_rq->grr_nr_running:%d, grr_se:%x, grr_rq->curr:%x\n", grr_rq, grr_rq->grr_nr_running, grr_se, grr_rq->curr);
	//if ((grr_rq && grr_rq->grr_nr_running) && (grr_se != grr_rq->curr)) {
	if (grr_rq && grr_rq->grr_nr_running) {
		__dequeue_entity(grr_se);
		//TODO: To verify
		grr_rq->grr_nr_running--;
		dec_nr_running(rq);
	}
	grr_se->on_rq = 0;
	printlist(grr_rq);
}

static void yield_task_grr(struct rq *rq)
{
	//printk(KERN_ERR "yield_task_grr: called!!\n");
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

	//printk(KERN_ERR "put_prev_task_grr: called!!\n");

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

	requeue_grr_entity(grr_rq, grr_se, head);
}

static void task_tick_grr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_grr_entity *grr_se = &p->grre;
	if (p->grre.time_slice < 0) {
//		printk(KERN_ERR "task_tick_grr: p->grre.time_slice is garbage: %d, resetting to %d\n", p->grre.time_slice, GRR_TIMESLICE);
		p->grre.time_slice = GRR_TIMESLICE;
	} else {
//		printk(KERN_ERR "task_tick_grr: p->grre.time_slice is valid: %d\n", p->grre.time_slice);
	}

	//if (++ddd%300 == 0)
//	printk(KERN_ERR "task_tick_grr: called!! pid = %d pol = %d, slice = %d\n",
//			p->pid, p->policy, p->grre.time_slice);

//	if (p->grre.time_slice == 1)
//		printk(KERN_ERR "\n\n\n\n\n\n\ntask_tick_grr: BECAME ONE, NEXT TIME ZERO\n\n\n\n\n\n\n");

	if (p->policy != SCHED_GRR)
		return;

	if (--p->grre.time_slice) {
	//	printk(KERN_ERR "+");
		return;
	}
//	printk(KERN_ERR "Done..\n");

	p->grre.time_slice = GRR_TIMESLICE;

	if (grr_se->run_list.prev != grr_se->run_list.next) {
		printk(KERN_ERR "tick: Requeuing task: %d\n", p->pid);
		requeue_task_grr(rq, p, 0);
		set_tsk_need_resched(p);
		return;
	}
}

static void set_curr_task_grr(struct rq *rq)
{
	struct task_struct *p = rq->curr;
	struct sched_grr_entity *grr_se = &rq->curr->grre;
	struct grr_rq *grr_rq = grr_rq_of(grr_se);
	//TODO: Try using rq->curr

	printk(KERN_ERR "set_curr_task_grr: called!!\n");
	p->grre.exec_start = rq->clock_task;
	grr_rq->curr = grr_se;
	printk("set_curr_task_grr: grr_rq: %x, grr_rq->curr: %x, rq->curr: %x\n", grr_rq, grr_rq->curr, rq->curr);
}

static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	printk(KERN_ERR "switched_to_grr: called!!\n");
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
	printk(KERN_ERR "prio_changed_grr: called!!\n");
}

//TODO: Do we have to implement this?
static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	printk(KERN_ERR "get_rr_interval_grr: called!!\n");
	if (task->policy == SCHED_GRR)
                return GRR_TIMESLICE;
        else
                return 0;
}

/*
 * Simple, special scheduling class for the per-CPU idle tasks:
 */
const struct sched_class grr_sched_class = {
	//TODO: Move at correct location
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
