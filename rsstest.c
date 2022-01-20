#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <unistd.h>

void dump(int pid) {
  char cmd[1000];
  sprintf(cmd,
          "grep '^VmRSS' /proc/%d/status;"
          "grep '^Rss:' /proc/%d/smaps_rollup;"
          "echo",
          pid, pid);
  system(cmd);
}

int main(void) {
  eventfd_t dummy;
  int child_wait = eventfd(0, EFD_SEMAPHORE | EFD_CLOEXEC);
  int child_resume = eventfd(0, EFD_SEMAPHORE | EFD_CLOEXEC);
  if (child_wait == -1 || child_resume == -1) err(1, "eventfd");
  pid_t child = fork();
  if (child == -1) err(1, "fork");
  if (child == 0) {
    if (prctl(PR_SET_PDEATHSIG, SIGKILL)) err(1, "PDEATHSIG");
    if (getppid() == 1) exit(0);
    char *mapping = mmap(NULL, 80 * 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    eventfd_write(child_wait, 1);
    eventfd_read(child_resume, &dummy);
    for (int i = 0; i < 40; i++) mapping[0x1000 * i] = 1;
    eventfd_write(child_wait, 1);
    eventfd_read(child_resume, &dummy);
    for (int i = 40; i < 80; i++) mapping[0x1000 * i] = 1;
    eventfd_write(child_wait, 1);
    eventfd_read(child_resume, &dummy);
    exit(0);
  }

  eventfd_read(child_wait, &dummy);
  dump(child);
  eventfd_write(child_resume, 1);

  eventfd_read(child_wait, &dummy);
  dump(child);
  eventfd_write(child_resume, 1);

  eventfd_read(child_wait, &dummy);
  dump(child);
  eventfd_write(child_resume, 1);

  exit(0);
}
