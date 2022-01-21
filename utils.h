// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __UTILS_H
#define __UTILS_H

unsigned long kp_kallsyms_lookup_name(const char* name);
bool kp_resolve_symbols(void);
void kp_dump_memory(unsigned long addr, int size);

#if defined(CONFIG_X86_64)
bool kp_set_memory_rw(unsigned long addr, int size);
bool kp_set_memory_ro(unsigned long addr, int size);
#elif defined(CONFIG_ARM64)
void kp_mark_linear_text_alias_rw(void);
void kp_mark_linear_text_alias_ro(void);
#else
#error "Unsupported architecture"
#endif

#endif
