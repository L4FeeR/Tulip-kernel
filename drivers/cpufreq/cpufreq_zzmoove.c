/*
 * ZZMoove CPUFreq governor
 * ported for Linux 3.18+ / 4.4 kernels
 *
 * Modified version that works on ARM/ARM64
 * Original Author: Zane Zaminsky
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/tick.h>

#define ZZ_NAME        "zzmoove"
#define DEF_SAMPLING   (30000)
#define DEF_UP_THRESH  (70)
#define DEF_DOWN_DIFF  (10)

static unsigned int sampling_rate = DEF_SAMPLING;
static unsigned int up_threshold = DEF_UP_THRESH;
static unsigned int up_threshold_diff = DEF_DOWN_DIFF;

struct zz_cpuinfo {
    struct delayed_work work;
    unsigned int cur_load;
};

static DEFINE_PER_CPU(struct zz_cpuinfo *, zz_data);
static DEFINE_MUTEX(zz_lock);

static void zz_check_load(struct work_struct *work)
{
    struct zz_cpuinfo *pcpu =
        container_of(work, struct zz_cpuinfo, work.work);
    unsigned int cpu = smp_processor_id();
    unsigned int load;
    struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

    if (!policy)
        return;

    load = pcpu->cur_load;

    if (load > up_threshold) {
        policy->cur = policy->max;
        __cpufreq_driver_target(policy, policy->max,
            CPUFREQ_RELATION_H);
    } else if (load < (up_threshold - up_threshold_diff)) {
        policy->cur = policy->min;
        __cpufreq_driver_target(policy, policy->min,
            CPUFREQ_RELATION_L);
    }

    schedule_delayed_work_on(cpu, &pcpu->work,
        usecs_to_jiffies(sampling_rate));

    cpufreq_cpu_put(policy);
}

static void zz_update_load(unsigned int cpu)
{
    struct zz_cpuinfo *pcpu = per_cpu(zz_data, cpu);

    if (!pcpu)
        return;

    pcpu->cur_load = (unsigned int)(get_cpu_idle_time(cpu, NULL, 0) / 1000);
}

static int zz_governor_start(struct cpufreq_policy *policy)
{
    unsigned int cpu;

    mutex_lock(&zz_lock);

    for_each_cpu(cpu, policy->cpus) {
        struct zz_cpuinfo *pcpu;

        pcpu = kzalloc(sizeof(struct zz_cpuinfo), GFP_KERNEL);
        if (!pcpu) {
            mutex_unlock(&zz_lock);
            return -ENOMEM;
        }

        INIT_DELAYED_WORK(&pcpu->work, zz_check_load);
        per_cpu(zz_data, cpu) = pcpu;

        schedule_delayed_work_on(cpu, &pcpu->work,
            usecs_to_jiffies(sampling_rate));
    }

    mutex_unlock(&zz_lock);
    return 0;
}

static void zz_governor_stop(struct cpufreq_policy *policy)
{
    unsigned int cpu;

    mutex_lock(&zz_lock);

    for_each_cpu(cpu, policy->cpus) {
        struct zz_cpuinfo *pcpu = per_cpu(zz_data, cpu);

        if (!pcpu)
            continue;

        cancel_delayed_work_sync(&pcpu->work);
        kfree(pcpu);
        per_cpu(zz_data, cpu) = NULL;
    }

    mutex_unlock(&zz_lock);
}

static int zz_governor_event(struct cpufreq_policy *policy,
                             unsigned int event)
{
    switch (event) {
    case CPUFREQ_GOV_START:
        return zz_governor_start(policy);

    case CPUFREQ_GOV_STOP:
        zz_governor_stop(policy);
        break;
    }

    return 0;
}

static struct cpufreq_governor zzmoove_gov = {
    .name = ZZ_NAME,
    .owner = THIS_MODULE,
    .governor = zz_governor_event,
};

static int __init zz_init(void)
{
    return cpufreq_register_governor(&zzmoove_gov);
}

static void __exit zz_exit(void)
{
    cpufreq_unregister_governor(&zzmoove_gov);
}

MODULE_AUTHOR("Zane Zaminsky");
MODULE_DESCRIPTION("ZZMoove governor (ARM/ARM64 compatible)");
MODULE_LICENSE("GPL");

module_init(zz_init);
module_exit(zz_exit);
