#include <linux/mm.h>
#include <linux/version.h>

/*
 * On Linux kernels 5.7+, kallsyms_lookup_name() is no longer exported,
 * so we have to use kprobes to get the address.
 * Full credit to @f0lg0 for the idea.
 * See https://github.com/xcellerator/linux_kernel_hacking/commit/7e063f7d7da9190622f488b0e0345c0e57436586
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
#include <linux/kprobes.h>
#define KPROBE_LOOKUP 1

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

#if defined(CONFIG_X86_64)
int (*set_memory_rw_sym)(unsigned long addr, int numpages) = 0;
int (*set_memory_ro_sym)(unsigned long addr, int numpages) = 0;

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
#elif defined(CONFIG_ARM64)
unsigned long stext_sym;
unsigned long init_begin_sym;
void (*update_mapping_prot_sym)(phys_addr_t phys, unsigned long virt, phys_addr_t size, pgprot_t prot) = 0;

void kp_mark_linear_text_alias_rw(void) {
  update_mapping_prot_sym(__pa_symbol(stext_sym), (unsigned long)lm_alias(stext_sym), init_begin_sym - stext_sym, PAGE_KERNEL);
}

void kp_mark_linear_text_alias_ro(void) {
  update_mapping_prot_sym(__pa_symbol(stext_sym), (unsigned long)lm_alias(stext_sym), init_begin_sym - stext_sym, PAGE_KERNEL_RO);
}

#else
#error "Unsupported architecture"
#endif

bool kp_resolve_symbols(void) {
#if defined(CONFIG_X86_64)
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
#elif defined(CONFIG_ARM64)
  if (!stext_sym) {
    stext_sym = kp_kallsyms_lookup_name("_stext");
  }

  if (!init_begin_sym) {
    init_begin_sym = kp_kallsyms_lookup_name("__init_begin");
  }

  if (!update_mapping_prot_sym) {
    update_mapping_prot_sym = (void*)kp_kallsyms_lookup_name("update_mapping_prot");
  }

  if (!stext_sym) {
    pr_err("Could not find _stext symbol\n");
    return false;
  }

  if (!init_begin_sym) {
    pr_err("Could not find __init_begin symbol\n");
    return false;
  }

  if (!update_mapping_prot_sym) {
    pr_err("Could not find update_mapping_prot symbol\n");
    return false;
  }
#else
#error "Unsupported architecture"
#endif

  return true;
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
