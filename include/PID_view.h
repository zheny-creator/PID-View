#ifndef PID_VIEW_H
#define PID_VIEW_H

#include <unistd.h>
#include <stdio.h>
#include <sys/resource.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <getopt.h>
#include <stdbool.h>

struct process_in_ram
{
    char name[16];
    int priority;
    char state;
    pid_t pid;
    int ppid;
    int threads;
    char uid_name[32];
    long vmem_mb;
    long rss_mb;
    long uptime_sec;
    double all_memory;
    int seconds_update;
};

void parse_proc_status(const char *buffer, struct process_in_ram *proc);
void parse_advanced_metrics(struct process_in_ram *proc);
void parse_all_aviable_memory(struct process_in_ram *proc);
void usage(void);

#endif /* PID_VIEW_H */