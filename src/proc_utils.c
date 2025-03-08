#include "proc_utils.h"

// Macro to get memmem
#define _GNU_SOURCE
#include <string.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int kill_old_and_get_pid_for(const char *process_name) {
  DIR *dir = opendir("/proc");
  if (!dir) {
    perror("kill_old_and_get_pid_for: opendir");
    return -1;
  }

  const size_t process_name_len = strlen(process_name);
  int pid = -1;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    int found_pid = atoi(entry->d_name);
    if (found_pid <= 0) {
      // filename wasn't a pid (eg /proc/cpuinfo)
      continue;
    }

    char tmpbuff[1024];
    snprintf(tmpbuff, sizeof(tmpbuff), "/proc/%s/cmdline", entry->d_name);
    FILE *cmd_file = fopen(tmpbuff, "r");
    if (!cmd_file) {
      // Process might have terminated
      continue;
    }

    const size_t read_sz = fread(tmpbuff, 1, sizeof(tmpbuff), cmd_file);
    if (read_sz > 0) {
      // cmd args may have \0's, so strstr won't work
      if (memmem(tmpbuff, read_sz, process_name, process_name_len) != NULL) {
        if (pid != -1) {
          fprintf(
              stderr,
              "Multiple pids (%d, %d) found for command %s. Killing pid %d\n",
              pid, found_pid, process_name, pid);
          if (kill(pid, SIGKILL) != 0) {
            perror("kill_old_and_get_pid_for: can't kill old pid");
          }
        }

        pid = found_pid;
      }
    }
    fclose(cmd_file);
  }

  closedir(dir);
  return pid;
}

int signal_single_kill_old_impl(int signum, const char *proc_name,
                                int last_known_pid, int retry) {
  if (last_known_pid == -1) {
    last_known_pid = kill_old_and_get_pid_for(proc_name);
  }

  if (last_known_pid <= 0) {
    printf("Can't find pid for %s, no signal sent\n", proc_name);
    return -1;
  }

  const int sigret = kill(last_known_pid, signum);
  if (sigret == 0) {
    return last_known_pid;
  }

  if (sigret == ESRCH) {
    if (retry > 0) {
      printf("Known pid %d for proc %s is no longer valid (crashed?), will "
             "search new pid\n",
             last_known_pid, proc_name);
      return signal_single_kill_old_impl(signum, proc_name, -1, retry - 1);
    } else {
      fprintf(stderr,
              "Signal single s failed after retrying, proc %s may be in a "
              "crashloop\n",
              proc_name);
      return -1;
    }
  }

  if (sigret < 0) {
    fprintf(stderr,
            "Failed to deliver signal to process %s (pid %d), error %d: %s\n",
            proc_name, last_known_pid, errno, strerror(errno));
    return last_known_pid;
  }

  // Shouldn't happen
  fprintf(
      stderr,
      "Failed to deliver signal to process %s (pid %d), unknown error %d: %s\n",
      proc_name, last_known_pid, errno, strerror(errno));
  return -1;
}

int signal_single_kill_old(int signum, const char *proc_name,
                           int last_known_pid) {
  return signal_single_kill_old_impl(signum, proc_name, last_known_pid, 1);
}
