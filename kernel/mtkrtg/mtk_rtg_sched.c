/*
 * mtk_rtg_sched.c
 *
 * rtg sched file
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cpufreq.h>
#include <linux/sched.h>

#include <../kernel/sched/sched.h>
#include <../kernel/sched/walt.h>
#include <../kernel/sched/tune.h>
#include "include/mtk_rtg_sched.h"

#define DEFAULT_FREQ_UPDATE_INTERVAL	8000000  /* ns */
#define DEFAULT_UTIL_INVALID_INTERVAL	(~0U) /* ns */
#define DEFAULT_UTIL_UPDATE_TIMEOUT	20000000  /* ns */
#define DEFAULT_GROUP_RATE		60 /* 60FPS */

struct related_thread_group *related_thread_groups[MAX_NUM_CGROUP_COLOC_ID];
static LIST_HEAD(active_related_thread_groups);
static DEFINE_RWLOCK(related_thread_group_lock);

#define for_each_related_thread_group(grp) \
	list_for_each_entry(grp, &active_related_thread_groups, list)

#define ADD_TASK	0
#define REM_TASK	1

struct related_thread_group *lookup_related_thread_group(
	unsigned int group_id)
{
	return related_thread_groups[group_id];
}

void init_task_rtg(struct task_struct *p)
{
	rcu_assign_pointer(p->grp, NULL);
	INIT_LIST_HEAD(&p->grp_list);
}

int sched_set_group_window_size(unsigned int grp_id, unsigned int rate)
{
	struct related_thread_group *grp = NULL;
	unsigned long flag;

	if (!rate)
		return -EINVAL;

	grp = lookup_related_thread_group(grp_id);
	if (!grp) {
		pr_err("set window size for group %d fail\n", grp_id);
		return -ENODEV;
	}

	raw_spin_lock_irqsave(&grp->lock, flag);
	grp->window_size = NSEC_PER_SEC / rate;
	raw_spin_unlock_irqrestore(&grp->lock, flag);

	return 0;
}

void group_time_rollover(struct group_time *time)
{
	time->prev_window_load = time->curr_window_load;
	time->curr_window_load = 0;
	time->prev_window_exec = time->curr_window_exec;
	time->curr_window_exec = 0;
}

int sched_set_group_window_rollover(unsigned int grp_id)
{
	struct related_thread_group *grp = NULL;
	u64 wallclock;
	struct task_struct *p = NULL;
	unsigned long flag;
	int boost;

	grp = lookup_related_thread_group(grp_id);
	if (!grp) {
		pr_err("set window start for group %d fail\n", grp_id);
		return -ENODEV;
	}

	raw_spin_lock_irqsave(&grp->lock, flag);

	wallclock = walt_ktime_clock();
	grp->prev_window_time = wallclock - grp->window_start;
	grp->window_start = wallclock;

	list_for_each_entry(p, &grp->tasks, grp_list) {
		p->ravg.prev_window_load = p->ravg.curr_window_load;
		p->ravg.curr_window_load = 0;
		p->ravg.prev_window_exec = p->ravg.curr_window_exec;
		p->ravg.curr_window_exec = 0;
	}

	group_time_rollover(&grp->time);
	group_time_rollover(&grp->time_pref_cluster);

	raw_spin_unlock_irqrestore(&grp->lock, flag);

	return 0;
}

static struct group_cpu_time *_group_cpu_time(struct related_thread_group *grp,
				       int cpu)
{
	return grp ? per_cpu_ptr(grp->cpu_time, cpu) : NULL;
}

/*
 * A group's window_start may be behind. When moving it forward, flip prev/curr
 * counters. When moving forward > 1 window, prev counter is set to 0
 */
void group_sync_window_start(struct rq *rq, struct group_cpu_time *cpu_time)
{
	u64 delta;
	int nr_windows;
	u64 curr_sum = cpu_time->curr_runnable_sum;

	delta = rq->window_start - cpu_time->window_start;
	if (!delta)
		return;

	nr_windows = div64_u64(delta, walt_ravg_window);
	if (nr_windows > 1)
		curr_sum = 0;

	cpu_time->prev_runnable_sum  = curr_sum;
	cpu_time->curr_runnable_sum  = 0;

	cpu_time->window_start = rq->window_start;
}

struct group_cpu_time *group_update_cpu_time(struct rq *rq,
	struct related_thread_group *grp)
{
	struct group_cpu_time *cpu_time = NULL;

	if (grp) {
		/* cpu_time protected by rq_lock */
		cpu_time = _group_cpu_time(grp, cpu_of(rq));

		/* group window roll over */
		group_sync_window_start(rq, cpu_time);
	}

	return cpu_time;
}

/*
 * Task's cpu usage is accounted in:
 *	rq->curr/prev_runnable_sum,  when its ->grp is NULL
 *	grp->cpu_time[cpu]->curr/prev_runnable_sum, when its ->grp is !NULL
 *
 * Transfer task's cpu usage between those counters when transitioning between
 * groups
 */
static void transfer_busy_time(struct rq *rq, struct related_thread_group *grp,
				struct task_struct *p, int event)
{
	u64 wallclock;
	struct group_cpu_time *cpu_time = NULL;
	u64 *src_curr_runnable_sum = NULL;
	u64 *dst_curr_runnable_sum = NULL;
	u64 *src_prev_runnable_sum = NULL;
	u64 *dst_prev_runnable_sum = NULL;

	wallclock = walt_ktime_clock();

	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_UPDATE, wallclock, 0);

	raw_spin_lock(&grp->lock);
	/* cpu_time protected by related_thread_group_lock, grp->lock rq_lock */
	cpu_time = _group_cpu_time(grp, cpu_of(rq));
	if (event == ADD_TASK) {
		group_sync_window_start(rq, cpu_time);
		cpu_time->curr_runnable_sum += p->ravg.curr_window;
		cpu_time->prev_runnable_sum += p->ravg.prev_window;
	} else {
		/*
		 * In case of REM_TASK, cpu_time->window_start would be
		 * uptodate, because of the update_task_ravg() we called
		 * above on the moving task. Hence no need for
		 * group_sync_window_start()
		 */
		cpu_time->curr_runnable_sum -= p->ravg.curr_window;
		cpu_time->prev_runnable_sum -= p->ravg.prev_window;
	}

	raw_spin_unlock(&grp->lock);
}

int sched_set_group_freq_update_interval(unsigned int grp_id, unsigned int interval)
{
	struct related_thread_group *grp = NULL;
	unsigned long flag;

	if (interval == 0)
		return -EINVAL;

	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (grp_id == DEFAULT_CGROUP_COLOC_ID ||
	    grp_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	grp = lookup_related_thread_group(grp_id);
	if (!grp) {
		pr_err("set update interval for group %d fail\n", grp_id);
		return -ENODEV;
	}

	raw_spin_lock_irqsave(&grp->lock, flag);
	grp->freq_update_interval = interval * NSEC_PER_MSEC;
	raw_spin_unlock_irqrestore(&grp->lock, flag);

	return 0;
}

bool group_should_update_freq(struct related_thread_group *grp, int cpu, unsigned int flags)
{
	if (!grp || grp->id != DEFAULT_RT_FRAME_ID)
		return true;

	if (flags & RTG_FREQ_FORCE_UPDATE) {
		sugov_mark_util_change(cpu, FORCE_UPDATE);
		return true;
	} else if (flags & RTG_FREQ_NORMAL_UPDATE) {
		if (ktime_get_ns() - grp->last_freq_update_time >= grp->freq_update_interval) {
			sugov_mark_util_change(cpu, FORCE_UPDATE);
			return true;
		}
	}

	return false;
}

int sched_set_group_load_mode(struct rtg_load_mode *mode)
{
	struct related_thread_group *grp = NULL;
	unsigned long flag;

	if (mode->grp_id <= DEFAULT_CGROUP_COLOC_ID ||
		mode->grp_id >= MAX_NUM_CGROUP_COLOC_ID) {
		pr_err("grp_id=%d is invalid.\n", mode->grp_id);
		return -EINVAL;
	}

	grp = lookup_related_thread_group(mode->grp_id);
	if (!grp) {
		pr_err("set invalid interval for group %d fail\n", mode->grp_id);
		return -ENODEV;
	}

	raw_spin_lock_irqsave(&grp->lock, flag);
	grp->mode.freq_enabled = !!mode->freq_enabled;
	grp->mode.util_enabled = !!mode->util_enabled;
	raw_spin_unlock_irqrestore(&grp->lock, flag);

	return 0;
}

int sched_set_group_util_invalid_interval(unsigned int grp_id, unsigned int interval)
{
	struct related_thread_group *grp = NULL;
	unsigned long flag;

	if (interval == 0)
		return -EINVAL;

	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (grp_id == DEFAULT_CGROUP_COLOC_ID ||
	    grp_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	grp = lookup_related_thread_group(grp_id);
	if (!grp) {
		pr_err("set invalid interval for group %d fail\n", grp_id);
		return -ENODEV;
	}

	raw_spin_lock_irqsave(&grp->lock, flag);
	grp->util_invalid_interval = interval * NSEC_PER_MSEC;
	raw_spin_unlock_irqrestore(&grp->lock, flag);

	return 0;
}

static inline bool
group_should_invalid_util(struct related_thread_group *grp, u64 now)
{
	if (grp->util_invalid_interval == DEFAULT_UTIL_INVALID_INTERVAL)
		return false;

	return (now - grp->last_freq_update_time >= grp->util_invalid_interval);
}

static inline void free_group_cputime(struct related_thread_group *grp)
{
	if (grp->cpu_time)
		free_percpu(grp->cpu_time);
}

static int alloc_group_cputime(struct related_thread_group *grp)
{
	int i;
	struct group_cpu_time *cpu_time = NULL;
	int cpu = raw_smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	u64 window_start = rq->window_start;

	grp->cpu_time = alloc_percpu(struct group_cpu_time);
	if (!grp->cpu_time)
		return -ENOMEM;

	for_each_possible_cpu(i) {
		cpu_time = per_cpu_ptr(grp->cpu_time, i);
		memset(cpu_time, 0, sizeof(struct group_cpu_time));
		cpu_time->window_start = window_start;
	}

	return 0;
}

int alloc_related_thread_groups(void)
{
	int i, ret;
	struct related_thread_group *grp = NULL;

	/* groupd_id = 0 is invalid as it's special id to remove group. */
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = kzalloc(sizeof(*grp), GFP_NOWAIT);
		if (!grp) {
			ret = -ENOMEM;
			goto err;
		}

		if (alloc_group_cputime(grp)) {
			kfree(grp);
			ret = -ENOMEM;
			goto err;
		}

		grp->id = i;
		grp->freq_update_interval = DEFAULT_FREQ_UPDATE_INTERVAL;
		grp->util_invalid_interval = DEFAULT_UTIL_INVALID_INTERVAL;
		grp->util_update_timeout = DEFAULT_UTIL_UPDATE_TIMEOUT;
		grp->window_size = NSEC_PER_SEC / DEFAULT_GROUP_RATE;
		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);
		related_thread_groups[i] = grp;
	}

	return 0;

err:
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = lookup_related_thread_group(i);
		if (grp) {
			free_group_cputime(grp);
			kfree(grp);
			related_thread_groups[i] = NULL;
		} else {
			break;
		}
	}

	return ret;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct related_thread_group *grp = p->grp;
	struct rq *rq = NULL;
	int empty_group = 1;
	struct rq_flags flag;
	unsigned long irqflag;

	rq = __task_rq_lock(p, &flag);
	transfer_busy_time(rq, p->grp, p, REM_TASK);

	raw_spin_lock_irqsave(&grp->lock, irqflag);
	list_del_init(&p->grp_list);
	rcu_assign_pointer(p->grp, NULL);

	if (p->on_cpu)
		grp->nr_running--;

	if ((int)grp->nr_running < 0) {
		WARN_ON(1);
		grp->nr_running = 0;
	}

	if (!list_empty(&grp->tasks))
		empty_group = 0;
	else
		grp->time.normalized_util = 0;

	raw_spin_unlock_irqrestore(&grp->lock, irqflag);
	__task_rq_unlock(rq, &flag);

	/* Reserved groups cannot be destroyed */
	if (empty_group && grp->id != DEFAULT_CGROUP_COLOC_ID) {
		 /*
		  * We test whether grp->list is attached with list_empty()
		  * hence re-init the list after deletion.
		  */
		write_lock(&related_thread_group_lock);
		list_del_init(&grp->list);
		write_unlock(&related_thread_group_lock);
	}
}

static int
add_task_to_group(struct task_struct *p, struct related_thread_group *grp)
{
	struct rq *rq = NULL;
	struct rq_flags flag;
	unsigned long irqflag;
	int boost;

	/*
	 * Change p->grp under rq->lock. Will prevent races with read-side
	 * reference of p->grp in various hot-paths
	 */
	rq = __task_rq_lock(p, &flag);
	transfer_busy_time(rq, grp, p, ADD_TASK);

	raw_spin_lock_irqsave(&grp->lock, irqflag);
	list_add(&p->grp_list, &grp->tasks);
	rcu_assign_pointer(p->grp, grp);

	if (p->on_cpu) {
		grp->nr_running++;
		if (grp->nr_running == 1)
			grp->mark_start = max(grp->mark_start, walt_ktime_clock()); //lint !e666 !e732
	}

	raw_spin_unlock_irqrestore(&grp->lock, irqflag);
	__task_rq_unlock(rq, &flag);

	return 0;
}

static int __sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct related_thread_group *grp = NULL;

	if (group_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	/* Switching from one group to another directly is not permitted */
	if ((current != p && (p->flags & PF_EXITING)) ||
	    (!p->grp && !group_id) ||
	    (p->grp && group_id))
		goto done;

	if (!group_id) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_related_thread_group(group_id);
	write_lock(&related_thread_group_lock);
	if (list_empty(&grp->list))
		list_add(&grp->list, &active_related_thread_groups);
	write_unlock(&related_thread_group_lock);

	rc = add_task_to_group(p, grp);
done:
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

/* group_id == 0: remove task from rtg */
int _sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (group_id == DEFAULT_CGROUP_COLOC_ID)
		return -EINVAL;

	return __sched_set_group_id(p, group_id);
}

int sched_set_group_id(pid_t pid, unsigned int group_id)
{
	struct task_struct *p = NULL;
	int ret;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	ret = _sched_set_group_id(p, group_id);
	put_task_struct(p);

	return ret;
}

unsigned int sched_get_group_id(struct task_struct *p)
{
	unsigned int group_id;
	struct related_thread_group *grp = NULL;

	rcu_read_lock();
	grp = task_related_thread_group(p);
	group_id = grp ? grp->id : 0;
	rcu_read_unlock();

	return group_id;
}

/*
 * We create a default colocation group at boot. There is no need to
 * synchronize tasks between cgroups at creation time because the
 * correct cgroup hierarchy is not available at boot. Therefore cgroup
 * colocation is turned off by default even though the colocation group
 * itself has been allocated. Furthermore this colocation group cannot
 * be destroyted once it has been created. All of this has been as part
 * of runtime optimizations.
 *
 * The job of synchronizing tasks to the colocation group is done when
 * the colocation flag in the cgroup is turned on.
 */
static int __init create_default_coloc_group(void)
{
	struct related_thread_group *grp = NULL;
	unsigned long flags;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);
	list_add(&grp->list, &active_related_thread_groups);
	write_unlock_irqrestore(&related_thread_group_lock, flags);

	return 0;
}
late_initcall(create_default_coloc_group);

int sync_cgroup_colocation(struct task_struct *p, bool insert)
{
	unsigned int grp_id = insert ? DEFAULT_CGROUP_COLOC_ID : 0;

	return __sched_set_group_id(p, grp_id);
}

void add_new_task_to_grp(struct task_struct *new)
{
	struct related_thread_group *grp = NULL;
	struct task_struct *leader = new->group_leader;
	unsigned int leader_grp_id = sched_get_group_id(leader);
	unsigned long flag;

	if (leader_grp_id != DEFAULT_CGROUP_COLOC_ID)
		return;

	if (thread_group_leader(new))
		return;
	if (leader_grp_id == DEFAULT_CGROUP_COLOC_ID) {
		if (!same_schedtune(new, leader))
			return;
	}

	rcu_read_lock();
	grp = task_related_thread_group(leader);
	rcu_read_unlock();

	/*
	 * It's possible that someone already added the new task to the
	 * group. A leader's thread group is updated prior to calling
	 * this function. It's also possible that the leader has exited
	 * the group. In either case, there is nothing else to do.
	 */
	if (!grp || new->grp)
		return;

	raw_spin_lock_irqsave(&grp->lock, flag);

	rcu_assign_pointer(new->grp, grp);
	list_add(&new->grp_list, &grp->tasks);

	raw_spin_unlock_irqrestore(&grp->lock, flag);
}

void sched_update_rtg_tick(struct task_struct *p)
{
	struct related_thread_group *grp = NULL;

	rcu_read_lock();
	grp = task_related_thread_group(p);
	if (!grp || list_empty(&grp->tasks)) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	if (grp->rtg_class)
		grp->rtg_class->sched_update_rtg_tick(grp);
}

void sched_set_group_min_util(struct related_thread_group *grp, int min_util)
{
	struct task_struct *p = NULL;
	unsigned long flags;
	u64 now;

	if (min_util < 0 || min_util > SCHED_CAPACITY_SCALE)
		return;

	raw_spin_lock_irqsave(&grp->lock, flags);
	if (list_empty(&grp->tasks))
		goto unlock;

	list_for_each_entry(p, &grp->tasks, grp_list)
		set_task_min_util_tick(grp, p, min_util);

	now = ktime_get_ns();
	grp->last_util_update_time = now;

unlock:
	raw_spin_unlock_irqrestore(&grp->lock, flags);
}
