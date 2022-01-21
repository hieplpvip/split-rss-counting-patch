# split-rss-counting-patch

A kernel module that patches Linux kernel "on-the-fly" to skip `TASK_RSS_EVENTS_THRESH`
check in [`check_sync_rss_stat`](https://elixir.bootlin.com/linux/v5.10/source/mm/memory.c#L192).

## Why?

Read the first post in references for details. In summary, Linux does not
update the global RSS counters immediately but only after every 64 page faults.
This may result in inaccurate memory usage report, especially if a process manages
to fault fewer than 64 pages in its lifetime.

The patch itself is fairly simple (patch assembly code to make the jump always happen),
but the patching process is not. We need to lock all CPU cores to make sure
nothing is running the code we are about to patch, set the memory to RW, apply
the patch, and set the memory back to RO. In addition, changing memory permission
on x86-64 and arm64 requires two totally different approaches.

In the end, we have a (flawlessly?) working patch. Still, you may ask,
_why would someone bother spending so much effort patching the kernel live
when you can modify the source code and recompile?_

Good question! There are several reasons:

- I want to learn more about how the Linux kernel works.
  Hacking through the kernel is a great way to learn.
- Loading a kernel module is much easier and faster than recompiling the kernel.
  I don't want to maintain a patched kernel for every machine I use. Dynamically
  patching the kernel is a popular workaround, most notably in Hackintosh.
  (Having a background in developing KEXTs for Hackintosh actually helps a lot.)
- This could be the base for more complicated patch. For me, the hardest part
  in this project is not creating the patch, but disabling the write protection.
  Fortunately, that only needs to be done once.
- It's fun.

## What does this patch?

Source code of `check_sync_rss_stat`:

```c
#define TASK_RSS_EVENTS_THRESH	(64)
static void check_sync_rss_stat(struct task_struct *task)
{
	if (unlikely(task != current))
		return;
	if (unlikely(task->rss_stat.events++ > TASK_RSS_EVENTS_THRESH))
		sync_mm_rss(task->mm);
}
```

We patch the second if statement. The conditional jump (jump if greater)
is replaced with an unconditional jump, so `sync_mm_rss` is always called.

Note that the compiler will inline `check_sync_rss_stat` in `handle_mm_fault`,
so we are actually patching `handle_mm_fault`.

In addition to the patch being applied "on-the-fly", the patch itself is also
generated "on-the-fly" to account for possible change in the jump target.

## How to use it?

You need `make` and `linux-headers-*` to build this kernel module.

```
sudo apt install build-essentials linux-headers-$(uname -r)
```

To build the module:

```
make
```

To load the module and apply the patch:

```
make load
```

To unload the module and revert the patch:

```
make unload
```

## Disclaimer

**I am not responsible for any damage caused by using this kernel module.
Use it at your own risk.**

The patching process at this point is very much experimental.
It may crash your kernel, though it had never happened.
However, the patch itself is safe, and once it has been applied,
it should not cause any issues.

## References

- https://tbrindus.ca/sometimes-the-kernel-lies-about-process-memory-usage/
- https://www.codeproject.com/Articles/1192422/Patching-the-Linux-kernel-at-run-time
- https://xcellerator.github.io/posts/linux_rootkits_11/#the-kallsyms-problem
- https://blog.csdn.net/weixin_42915431/article/details/115289115 (for ideas on mapping memory as RW on arm64)
