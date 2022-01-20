#ifndef __UTILS_H
#define __UTILS_H

unsigned long kp_kallsyms_lookup_name(const char* name);
bool kp_resolve_symbols(void);
bool kp_set_memory_rw(unsigned long addr, int size);
bool kp_set_memory_ro(unsigned long addr, int size);
void kp_dump_memory(unsigned long addr, int size);

#endif
