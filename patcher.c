// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/string.h>
#include "utils.h"

struct kp_patcher_task {
  struct task_struct *t;
  unsigned int cpu;
  unsigned long addr;
  void *value;
  int size;
};

/* Max number of CPU actually handled. */
#define KP_MAX_CPUS 32

/* Number of detected CPUs. */
int kp_patcher_number_of_cpus = 0;

/* IDs of the CPUs. */
int kp_patcher_cpu_ids[KP_MAX_CPUS] = {-1};

/* State of CPUs; no locking required since each CPU just touch it's own byte. */
/* 0 - unlocked, 1 - locked. */
int kp_patcher_cpu_states[KP_MAX_CPUS] = {0};

/* Kernel threads info. */
struct kp_patcher_task kp_patcher_cpu_tasks[KP_MAX_CPUS] = {{0}};

/* In case of error kill everyone. */
int kp_patcher_die = 0;

/* The elected patching CPU. */
int kp_patcher_main_cpu = -1;

/* Switching this to 1 will triggers our threads. */
int kp_patcher_proceed = 0;

int kp_patcher_thread(void *arg) {
  int i;
  int p;

  struct kp_patcher_task *t = (struct kp_patcher_task *)arg;

  pr_info("Thread of CPU %u running...\n", t->cpu);

  /* Wait until someone (kp_patcher_patch) told us to proceed with the job. */
  while (!kp_patcher_proceed) {
    if (kp_patcher_die) {
      /* Kill this thread. */
      goto out;
    }

    schedule();
  }

  /* Lock CPU. */
  get_cpu(); /* disable preemption */
  kp_patcher_cpu_states[t->cpu] = 1;

  do {
    p = 1;

    /* The patching process is up to us! */
    if (t->cpu == kp_patcher_main_cpu) {
      for (i = 0; i < kp_patcher_number_of_cpus; i++) {
        p &= kp_patcher_cpu_states[i];
      }

      /* If 1, everyone is locked. */
      if (p) {
        pr_info("All CPUs locked. Patching...\n");

        /* Set memory to RW. */
#if defined(CONFIG_X86_64)
        if (!kp_set_memory_rw(t->addr, t->size)) {
          pr_err("Could not set memory to RW.\n");
          kp_patcher_proceed = 0;
          goto release;
        }
#elif defined(CONFIG_ARM64)
        /*
         * Due to how memory mapping is done on arm64, it's not possible
         * (AFAIK) to mark a specific memory region as RW, so we have to
         * mark the whole .text section as RW. This should not be (or is it?)
         * a security issue since we have effectively locked all CPUs,
         * and the time window is extremely short.
         *
         * In addition, we need to patch the linear alias of .text,
         * not .text itself. The exact reason for this is unknown, but I
         * speculate it's related to permission, as .text is marked as R+X
         * while the linear alias is only R.
         *
         * See https://github.com/torvalds/linux/commit/5ea5306c3235a157f06040c59730b1133115ed26
         */
        kp_mark_linear_text_alias_rw();
        t->addr = (unsigned long)lm_alias(t->addr);
#else
#error "Unsupported architecture"
#endif

        /* Apply patch. Dump before and after. */
        kp_dump_memory(t->addr, t->size);
        barrier();
        memcpy((void *)t->addr, t->value, t->size);
        barrier();
        kp_dump_memory(t->addr, t->size);
        pr_info("Patch applied.\n");

        /* Set memory back to RO. */
#if defined(CONFIG_X86_64)
        if (!kp_set_memory_ro(t->addr, t->size)) {
          pr_err("Could not set memory to RO.\n");
        }
#elif defined(CONFIG_ARM64)
        kp_mark_linear_text_alias_ro();
#else
#error "Unsupported architecture"
#endif

        kp_patcher_proceed = 0;
      }
    }

    if (!kp_patcher_proceed) {
      break;
    }
  } while (1);

#if defined(CONFIG_X86_64)
release:
#endif
  /* Unlock CPU. */
  kp_patcher_cpu_states[t->cpu] = 0;
  put_cpu(); /* enable preemption */

out:
  pr_info("Thread of CPU %u died...\n", t->cpu);
  return 0;
}

bool kp_patcher_patch(unsigned long addr, void *value, int size) {
  int i;
  int cpu;

  /* Detect CPUs. */
  kp_patcher_number_of_cpus = 0;
  for_each_cpu(cpu, cpu_online_mask) {
    if (kp_patcher_number_of_cpus > KP_MAX_CPUS) {
      pr_err("Too much CPUs to handle!\n");
      return false;
    }

    kp_patcher_cpu_ids[kp_patcher_number_of_cpus] = cpu;
    kp_patcher_number_of_cpus++;

    pr_info("CPU %u is online...\n", cpu);
  }

  /* The first one is our patcher. */
  kp_patcher_main_cpu = kp_patcher_cpu_ids[0];

  for (i = 0; i < kp_patcher_number_of_cpus; i++) {
    kp_patcher_cpu_states[i] = 0;
    kp_patcher_cpu_tasks[i].cpu = kp_patcher_cpu_ids[i];
    kp_patcher_cpu_tasks[i].t = kthread_create(kp_patcher_thread, &kp_patcher_cpu_tasks[i], "kp%d", kp_patcher_cpu_ids[i]);
    kp_patcher_cpu_tasks[i].addr = addr;
    kp_patcher_cpu_tasks[i].value = value;
    kp_patcher_cpu_tasks[i].size = size;

    if (!kp_patcher_cpu_tasks[i].t) {
      pr_err("Error while starting %d.\n", kp_patcher_cpu_ids[i]);

      /* Kill threads which already started. */
      kp_patcher_die = 1;
      return false;
    }

    /* Bind the task on that CPU. */
    kthread_bind(kp_patcher_cpu_tasks[i].t, kp_patcher_cpu_tasks[i].cpu);
    wake_up_process(kp_patcher_cpu_tasks[i].t);
  }

  /* Wait a "little". */
  schedule();

  kp_patcher_proceed = 1;

  return true;
}
