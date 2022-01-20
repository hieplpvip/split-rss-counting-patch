#include <linux/module.h>
#include <linux/string.h>
#include <linux/version.h>
#include "patcher.h"
#include "utils.h"

MODULE_AUTHOR("Le Bao Hiep");
MODULE_DESCRIPTION("Patch handle_mm_fault to get more accurate RSS values");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");

unsigned long handle_mm_fault_addr;
unsigned long handle_mm_fault_patch_addr;
uint8_t handle_mm_fault_patch_find[] = {0x0F, 0x8F, 0x08, 0x09, 0x00, 0x00};  // jg 0x90e
uint8_t handle_mm_fault_patch_repl[] = {0xE9, 0x09, 0x09, 0x00, 0x00, 0x90};  // jmp 0x90e; nop
static_assert(sizeof(handle_mm_fault_patch_find) == sizeof(handle_mm_fault_patch_repl), "Patch size mismatch");

static int __init split_rss_counting_patch_init(void) {
  int i;

  /* Resolve symbols for set_memory_rw/set_memory_ro. */
  kp_resolve_symbols();

  /* Find the address of handle_mm_fault. */
  handle_mm_fault_addr = kp_kallsyms_lookup_name("handle_mm_fault");
  if (!handle_mm_fault_addr) {
    pr_err("Could not find address of handle_mm_fault\n");
    return -ENXIO;
  }

  /* Find the address to apply patch. */
  handle_mm_fault_patch_addr = 0;
  for (i = 0; i < 300; i++) {
    if (memcmp((void *)(handle_mm_fault_addr + i), handle_mm_fault_patch_find, sizeof(handle_mm_fault_patch_find)) == 0) {
      handle_mm_fault_patch_addr = handle_mm_fault_addr + i;
      break;
    }
  }
  if (!handle_mm_fault_patch_addr) {
    pr_err("Could not find the address to apply patch\n");
    return -ENXIO;
  }

  /* Patch! */
  if (!kp_patcher_patch(handle_mm_fault_patch_addr, handle_mm_fault_patch_repl, sizeof(handle_mm_fault_patch_repl))) {
    pr_err("Could not patch handle_mm_fault\n");
    return -ENXIO;
  }

  return 0;
}

static void __exit split_rss_counting_patch_exit(void) {
  /* Revert the patch on exit. */
  pr_info("Exit! Reverting the patch\n");
  if (!kp_patcher_patch(handle_mm_fault_patch_addr, handle_mm_fault_patch_find, sizeof(handle_mm_fault_patch_find))) {
    pr_err("Could not revert the patch in handle_mm_fault\n");
  }

  return;
}

module_init(split_rss_counting_patch_init);
module_exit(split_rss_counting_patch_exit);
