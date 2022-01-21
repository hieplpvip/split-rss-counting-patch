// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __PATCHER_H
#define __PATCHER_H

bool kp_patcher_patch(unsigned long addr, void *value, int size);

#endif
