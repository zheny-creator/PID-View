#include "PID_view.h"
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

void parse_proc_status(const char *buffer, struct process_in_ram *proc)
{
    const char *line = buffer;
    int raw_uid = -1;

    while (*line)
    {
        if (strncmp(line, "Name:", 5) == 0)
        {
            sscanf(line, "Name:\t%15[^\n]", proc->name);
        }
        else if (strncmp(line, "State:", 6) == 0)
        {
            sscanf(line, "State:\t%c", &proc->state);
        }
        else if (strncmp(line, "PPid:", 5) == 0)
        {
            sscanf(line, "PPid:\t%d", &proc->ppid);
        }
        else if (strncmp(line, "Threads:", 8) == 0)
        {
            sscanf(line, "Threads:\t%d", &proc->threads);
        }
        else if (strncmp(line, "Uid:", 4) == 0)
        {
            sscanf(line, "Uid:\t%d", &raw_uid);
        }

        while (*line && *line != '\n')
            line++;
        if (*line == '\n')
            line++;
    }

    if (raw_uid != -1)
    {
        const struct passwd *pw = getpwuid(raw_uid);
        if (pw)
            snprintf(proc->uid_name, sizeof(proc->uid_name), "%s", pw->pw_name);
        else
            snprintf(proc->uid_name, sizeof(proc->uid_name), "%d", raw_uid);
    }
    else
    {
        strcpy(proc->uid_name, "unknown");
    }
}

void parse_advanced_metrics(struct process_in_ram *proc)
{
    char path[32];
    char buf[512];
    int fd;
    ssize_t n;

    long page_size = sysconf(_SC_PAGESIZE);
    long clock_ticks = sysconf(_SC_CLK_TCK);

    snprintf(path, sizeof(path), "/proc/%d/statm", proc->pid);
    if ((fd = open(path, O_RDONLY)) >= 0)
    {
        if ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
        {
            buf[n] = '\0';
            long vmem_pages = 0, rss_pages = 0;
            sscanf(buf, "%ld %ld", &vmem_pages, &rss_pages);
            proc->vmem_mb = (vmem_pages * page_size) / (1024 * 1024);
            proc->rss_mb = (rss_pages * page_size) / (1024 * 1024);
        }
        close(fd);
    }
    else
    {
        proc->vmem_mb = 0;
        proc->rss_mb = 0;
    }

    double uptime_sys = 0.0;
    if ((fd = open("/proc/uptime", O_RDONLY)) >= 0)
    {
        if ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
        {
            buf[n] = '\0';
            sscanf(buf, "%lf", &uptime_sys);
        }
        close(fd);
    }

    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    if ((fd = open(path, O_RDONLY)) >= 0)
    {
        if ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
        {
            buf[n] = '\0';
            const char *p = strrchr(buf, ')');
            if (p)
            {
                p += 2;
                long unsigned starttime = 0;
                int scanned = sscanf(p, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %d %*d %*d %*d %lu",
                                     &proc->priority, &starttime);
                if (scanned == 2)
                {
                    double process_start_sec = (double)starttime / clock_ticks;
                    proc->uptime_sec = (long)(uptime_sys - process_start_sec);
                    if (proc->uptime_sec < 0)
                        proc->uptime_sec = 0;
                }
            }
        }
        else
        {
            proc->uptime_sec = 0;
        }
        close(fd);
    }
}

void parse_all_aviable_memory(struct process_in_ram *proc)
{
    char path[32];
    char buffer[1024] = {0};
    const char *line = buffer;
    long long all_memory = 0;
    int fd;
    snprintf(path, sizeof(path), "/proc/meminfo");
    if ((fd = open(path, O_RDONLY)) >= 0)
    {
        ssize_t read_buffer = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        if (read_buffer > 0)
        {
            buffer[read_buffer] = '\0';
        }
    }
    else
    {
        proc->all_memory = 0;
    }
    while (*line)
    {
        if (strncmp(line, "MemTotal:", 9) == 0)
        {
            long mem_kb = 0;
            if (sscanf(line, "MemTotal:\t%ld", &mem_kb) == 1)
            {
                all_memory = mem_kb / 1024;
            }
            break;
        }
        while (*line && *line != '\n')
            line++;
        if (*line == '\n')
            line++;
    }
    if (all_memory > 0)
    {
        proc->all_memory = ((double)proc->rss_mb / (double)all_memory) * 100.0;
    }
    else
    {
        proc->all_memory = 0;
    }
}

void usage(void)
{
    fprintf(stdout, "Usage: pid-view [options]\n");
    fprintf(stdout, "   -h, --help           Show help message\n");
    fprintf(stdout, "   -w, --watch          Monitor process activity\n");
    fprintf(stdout, "   -s, --seconds <SEC>  Update interval in seconds\n");
    fprintf(stdout, "   <PID>                Display current process status\n");
}
