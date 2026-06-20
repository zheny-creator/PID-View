#ifndef PID_VIEW_H
#define PID_VIEW_H

#include <sys/types.h>

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