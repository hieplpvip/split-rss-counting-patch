# split-rss-counting-patch

Patch Linux kernel on-the-fly to skip `TASK_RSS_EVENTS_THRESH` check in [`check_sync_rss_stat`](https://elixir.bootlin.com/linux/v5.10/source/mm/memory.c#L192).

## References

- https://tbrindus.ca/sometimes-the-kernel-lies-about-process-memory-usage/
- https://www.codeproject.com/Articles/1192422/Patching-the-Linux-kernel-at-run-time
- https://blog.csdn.net/weixin_42915431/article/details/115289115 (for ideas on mapping memory as RW on arm64)
