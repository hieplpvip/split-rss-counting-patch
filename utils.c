#include <linux/version.h>

/*
 * On Linux kernels 5.7+, kallsyms_lookup_name() is no longer exported,
 * so we have to use kprobes to get the address.
 * Full credit to @f0lg0 for the idea.
 * See https://github.com/xcellerator/linux_kernel_hacking/commit/7e063f7d7da9190622f488b0e0345c0e57436586
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
#define KPROBE_LOOKUP 1
#include <linux/kprobes.h>

static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name",
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char* name);
kallsyms_lookup_name_t kallsyms_lookup_name_sym = 0;
#else
#include <linux/kallsyms.h>
#endif

unsigned long kp_kallsyms_lookup_name(const char* name) {
#ifdef KPROBE_LOOKUP
  if (!kallsyms_lookup_name_sym) {
    register_kprobe(&kp);
    kallsyms_lookup_name_sym = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);
  }
  return kallsyms_lookup_name_sym(name);
#else
  return kallsyms_lookup_name(name);
#endif
}

int (*set_memory_rw_sym)(unsigned long addr, int numpages) = 0;
int (*set_memory_ro_sym)(unsigned long addr, int numpages) = 0;

bool kp_resolve_symbols(void) {
  if (!set_memory_rw_sym) {
    set_memory_rw_sym = (void*)kp_kallsyms_lookup_name("set_memory_rw");
  }

  if (!set_memory_ro_sym) {
    set_memory_ro_sym = (void*)kp_kallsyms_lookup_name("set_memory_ro");
  }

  if (!set_memory_rw_sym) {
    pr_err("Could not find set_memory_rw symbol\n");
    return false;
  }

  if (!set_memory_ro_sym) {
    pr_err("Could not find set_memory_ro symbol\n");
    return false;
  }

  return true;
}

bool kp_set_memory_rw(unsigned long addr, int size) {
  unsigned long b;
  int pages = 1;

  b = (addr - (addr % PAGE_SIZE));

  /* Check if the modification go outside the page bounds. */
  if ((addr + size) > b + PAGE_SIZE) {
    /* Out-of-bounds, minimum 2 pages, max... depends. */
    pages = 2 + (size / PAGE_SIZE);
  }

  return set_memory_rw_sym(b, pages) == 0;
}

bool kp_set_memory_ro(unsigned long addr, int size) {
  unsigned long b;
  int pages = 1;

  b = (addr - (addr % PAGE_SIZE));

  /* Check if the modification go outside the page bounds. */
  if ((addr + size) > b + PAGE_SIZE) {
    /* Out-of-bounds, minimum 2 pages, max... depends. */
    pages = 2 + (size / PAGE_SIZE);
  }

  return set_memory_ro_sym(b, pages) == 0;
}

void kp_dump_memory(unsigned long addr, int size) {
#define KP_DUMP_COLUMNS 16

  int i;
  char* t = (char*)addr;

  pr_info("Dumping memory at %pK:\n", (void*)addr);

  for (i = 0; i < KP_DUMP_COLUMNS; i++) {
    if (i % KP_DUMP_COLUMNS == 0) {
      pr_cont("       ");
    }
    pr_cont("%02d ", i);
  }
  pr_cont("\n\n");

  for (i = 0; i < size; i++) {
    if (i % KP_DUMP_COLUMNS == 0) {
      if (i) {
        pr_cont("\n");
      }
      printk("%04x   ", i / KP_DUMP_COLUMNS);
    }
    pr_cont("%02x ", (unsigned char)t[i]);
  }
  pr_cont("\n");
}
