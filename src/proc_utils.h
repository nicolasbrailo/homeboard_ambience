#pragma once

// Get a PID for process_name. If multiple are found, sigkill a random one.
int kill_old_and_get_pid_for(const char *process_name);

// Attempt to deliver a signal to last_known_pid. If it fails, look for a
// possible new pid using kill_old_and_get_pid_for, and attempt once more
int signal_single_kill_old(int signum, const char *proc_name,
                           int last_known_pid);
