#include "sched.h"

/*
 * grr scheduling class.
 *
 */

#ifdef CONFIG_SMP
static int
select_task_rq_grr(struct task_struct *p, int sd_flag, int flags)
{
	printk(KERN_ERR "select_task_rq_grr: called!\n");
	return task_cpu(p); /* IDLE tasks as never migrated */
}
#endif /* CONFIG_SMP */
/*
 * Idle tasks are unconditionally rescheduled:
 */

static void check_preempt_curr_grr(struct rq *rq, struct task_struct *p, int flags)
{
	printk(KERN_ERR "check_preempt_curr_grr: called!\n");
	resched_task(rq->idle);
}

static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	printk(KERN_ERR "pick_next_task_grr: called!\n");
	schedstat_inc(rq, sched_goidle);
	calc_load_account_idle(rq);
	return rq->idle;
}

/*
 * It is not legal to sleep in the idle task - print a warning
 * message if some code attempts to do it:
 */
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	raw_spin_unlock_irq(&rq->lock);
	printk(KERN_ERR "bad: scheduling from the idle thread!\n");
	dump_stack();
	raw_spin_lock_irq(&rq->lock);
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
	/* no enqueue/yield_task for idle tasks */

	/* dequeue is not valid, we print a debug message there: */
	.dequeue_task		= dequeue_task_grr,

	.check_preempt_curr	= check_preempt_curr_grr,

	.pick_next_task		= pick_next_task_grr,
	.put_prev_task		= put_prev_task_grr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_grr,
#endif

	.set_curr_task          = set_curr_task_grr,
	.task_tick		= task_tick_grr,

	.get_rr_interval	= get_rr_interval_grr,

	.prio_changed		= prio_changed_grr,
	.switched_to		= switched_to_grr,
};
