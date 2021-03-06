// SPDX-License-Identifier: GPL-2.0-or-later

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
int handle_mm_fault_patch_size;
uint8_t handle_mm_fault_patch_find[20];
uint8_t handle_mm_fault_patch_repl[20];

bool split_rss_counting_patch_build(void) {
#if defined(CONFIG_X86_64)
  int i;
  uint8_t cmp_edx_0x40[] = {0x83, 0xfa, 0x40};
  uint8_t jg[] = {0x0f, 0x8f};
  unsigned long cmp_edx_0x40_addr = 0;
  unsigned long jg_addr = 0;
  uint32_t jump_distance;

  /* Find cmp edx, 0x40. */
  for (i = 0; i < 300; i++) {
    if (memcmp((void *)(handle_mm_fault_addr + i), cmp_edx_0x40, sizeof(cmp_edx_0x40)) == 0) {
      cmp_edx_0x40_addr = handle_mm_fault_addr + i;
      break;
    }
  }
  if (!cmp_edx_0x40_addr) {
    pr_err("Could not find instruction cmp edx, 0x40\n");
    return false;
  }

  /* Find jg */
  for (i = 0; i < 20; ++i) {
    if (memcmp((void *)(cmp_edx_0x40_addr + i), jg, sizeof(jg)) == 0) {
      jg_addr = cmp_edx_0x40_addr + i;
      break;
    }
  }
  if (!jg_addr) {
    pr_err("Could not find instruction jg\n");
    return false;
  }

  /*
   * We'll patch: jg <distance> (6 bytes)
   * into:        jmp <distance> (5 bytes)
   *              nop (1 byte)
   */
  jump_distance = *(uint32_t *)(jg_addr + 2) + 6;
  handle_mm_fault_patch_addr = jg_addr;
  handle_mm_fault_patch_size = 6;
  memcpy((void *)handle_mm_fault_patch_find, (void *)jg_addr, 6);
  handle_mm_fault_patch_repl[0] = 0xe9;
  *(uint32_t *)(handle_mm_fault_patch_repl + 1) = jump_distance - 5;
  handle_mm_fault_patch_repl[5] = 0x90; /* nop */
#elif defined(CONFIG_ARM64)
  int i;
  uint32_t opcode, imm;
  uint32_t cmp_w1_0x40 = 0x7101003f;
  uint32_t b_mask = 0xff00001f;
  uint32_t b_gt = 0x5400000c;
  unsigned long cmp_w1_0x40_addr = 0;
  unsigned long b_gt_addr = 0;

  /* Find cmp w1, #0x40. */
  for (i = 0; i < 300; i++) {
    if (*((uint32_t *)(handle_mm_fault_addr) + i) == cmp_w1_0x40) {
      cmp_w1_0x40_addr = (unsigned long)((uint32_t *)(handle_mm_fault_addr) + i);
      break;
    }
  }
  if (!cmp_w1_0x40_addr) {
    pr_err("Could not find instruction cmp w1, #0x40\n");
    return false;
  }

  /* Find b.gt */
  for (i = 0; i < 10; ++i) {
    opcode = *((uint32_t *)(cmp_w1_0x40_addr) + i);
    if ((opcode & b_mask) == b_gt) {
      b_gt_addr = (unsigned long)((uint32_t *)(cmp_w1_0x40_addr) + i);
      break;
    }
  }
  if (!b_gt_addr) {
    pr_err("Could not find instruction b.gt\n");
    return false;
  }

  /*
   * We'll patch: b.gt <distance> (4 bytes)
   * into:        b    <distance> (4 bytes)
   */
  imm = (opcode >> 5) & ((1ul << 19) - 1);       /* imm19 */
  imm = imm | ((imm & 0x40000) ? 0x3f80000 : 0); /* sign extend to imm26 */
  handle_mm_fault_patch_addr = b_gt_addr;
  handle_mm_fault_patch_size = 4;
  *(uint32_t *)handle_mm_fault_patch_find = opcode;
  *(uint32_t *)handle_mm_fault_patch_repl = imm | 0x14000000;
#else
#error "Unsupported architecture"
#endif

  pr_info("Built patch successfully! Patch find:\n");
  kp_dump_memory((unsigned long)handle_mm_fault_patch_find, handle_mm_fault_patch_size);
  pr_info("Patch repl:\n");
  kp_dump_memory((unsigned long)handle_mm_fault_patch_repl, handle_mm_fault_patch_size);

  return true;
}

static int __init split_rss_counting_patch_init(void) {
  /* Resolve symbols for set_memory_rw/set_memory_ro. */
  kp_resolve_symbols();

  /* Find the address of handle_mm_fault. */
  handle_mm_fault_addr = kp_kallsyms_lookup_name("handle_mm_fault");
  if (!handle_mm_fault_addr) {
    pr_err("Could not find address of handle_mm_fault\n");
    return -ENXIO;
  }

  /* Build the patch. */
  if (!split_rss_counting_patch_build()) {
    pr_err("Could not build patch\n");
    return -ENXIO;
  }

  /* Patch! */
  if (!kp_patcher_patch(handle_mm_fault_patch_addr, handle_mm_fault_patch_repl, handle_mm_fault_patch_size)) {
    pr_err("Could not patch handle_mm_fault\n");
    return -ENXIO;
  }

  return 0;
}

static void __exit split_rss_counting_patch_exit(void) {
  /* Revert the patch on exit. */
  pr_info("Exit! Reverting the patch\n");
  if (!kp_patcher_patch(handle_mm_fault_patch_addr, handle_mm_fault_patch_find, handle_mm_fault_patch_size)) {
    pr_err("Could not revert the patch in handle_mm_fault\n");
  }

  return;
}

module_init(split_rss_counting_patch_init);
module_exit(split_rss_counting_patch_exit);
