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
};

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
        struct passwd *pw = getpwuid(raw_uid);
        if (pw)
            strncpy(proc->uid_name, pw->pw_name, sizeof(proc->uid_name) - 1);
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
    ssize_t read_buffer;

    snprintf(path, sizeof(path), "/proc/meminfo");
    if ((fd = open(path, O_RDONLY)) >= 0)
    {
        if ((read_buffer = read(fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[read_buffer] = '\0';
        }
    }
    else
    {
        proc->all_memory = 0;
    }
    close(fd);
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
    fprintf(stderr, "Usage: pid-view [options]\n");
    fprintf(stderr, "  ./pid-view -h, --help           Show help message\n");
    fprintf(stderr, "  ./pid-view -w, --watch <PID>    Monitor process activity\n");
    fprintf(stderr, "  ./pid-view <PID>                Display current process status\n");
}

int main(int argv, char *args[])
{
    int watch = 0;
    if (argv < 2)
    {
        usage();
        return 1;
    }
    struct option arguments[] =
        {
            {"help", no_argument, NULL, 'h'},
            {"watch", required_argument, NULL, 'w'},
            {NULL, 0, NULL, 0}};
    int opt;
    while ((opt = getopt_long(argv, args, "hw:", arguments, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            fprintf(stdout, "Usage: pid-view [options]\n");
            fprintf(stdout, "  ./pid-view -h, --help           Show help message\n");
            fprintf(stdout, "  ./pid-view -w, --watch <PID>    Monitor process activity\n");
            fprintf(stdout, "  ./pid-view <PID>                Display current process status\n");
            return 0;
            break;
        case 'w':
            watch = 1;
            break;
        default:
            fprintf(stderr, "Unknown argument!");
            return 1;
        }
    }
    struct process_in_ram process = {0};
    if (watch == 0)
    {
        process.pid = atoi(args[1]);
    }
    else
    {
        process.pid = atoi(args[2]);
    }
    while (true)
    {
        char filepath[32];
        char buffer[1024] = {0};

        snprintf(filepath, sizeof(filepath), "/proc/%d/status", process.pid);

        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Process %d not found\n", process.pid);
            return 1;
        }

        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        if (bytes_read <= 0)
        {
            fprintf(stderr, "Failed to read status\n");
            return 1;
        }
        buffer[bytes_read] = '\0';

        parse_proc_status(buffer, &process);
        parse_advanced_metrics(&process);
        parse_all_aviable_memory(&process);

        long h = process.uptime_sec / 3600;
        long m = (process.uptime_sec % 3600) / 60;
        long s = process.uptime_sec % 60;

        printf("=== PID-View v0.4 ===\n");
        printf("PID:          %d\033[K\n", process.pid);
        printf("Name:         %s\033[K\n", process.name);
        printf("Owner:        %s\033[K\n", process.uid_name);
        printf("State:        %c\033[K\n", process.state);
        printf("Priority:     %d\033[K\n", process.priority - 20);

        if (process.ppid != 0)
        {
            printf("Parent PID:   %d\033[K\n", process.ppid);
        }
        else
        {
            printf("Parent PID:   None\033[K\n");
        }

        printf("Threads:      %d\033[K\n", process.threads);
        printf("Virtual Mem:  %ld MB\033[K\n", process.vmem_mb);
        printf("Physical Mem: %ld MB (RSS) (%.2f%%)\033[K\n", process.rss_mb, process.all_memory);
        printf("Running Time: %02ld:%02ld:%02ld\033[K\n", h, m, s);
        printf("=====================\n");

        fflush(stdout);

        sleep(1);
        if (watch == 0)
        {
            break;
        }
        else
        {
            printf("\033[12A");
            continue;
        }
    }

    return 0;
}