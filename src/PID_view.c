#include "PID_view.h"

int main(int argv, char *args[])
{
    int watch = 0;
    if (argv < 2)
    {
        usage();
        return 1;
    }
    static const struct option arguments[] =
        {
            {"help", no_argument, NULL, 'h'},
            {"watch", required_argument, NULL, 'w'},
            {"seconds", required_argument, NULL, 's'},
            {NULL, 0, NULL, 0}};
    int opt;
    while ((opt = getopt_long(argv, args, "hw:s:", arguments, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage();
            return 0;
            break;
        case 'w':
            watch = 1;
            break;
        case 's':
            watch = 2;
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
    if (watch == 2)
    {
        process.pid = atoi(args[2]);
        process.seconds_update = atoi(args[4]);
    }

    while (true)
    {
        char filepath[32];
        char buffer[1024] = {0};

        snprintf(filepath, sizeof(filepath), "/proc/%d/status", process.pid);

        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Process %d not found or kill\n", process.pid);
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

        printf("\033[H\033[J");
        printf("=== PID-View v0.4 ===\n");
        printf("PID:          %d\n", process.pid);
        printf("Name:         %s\n", process.name);
        printf("Owner:        %s\n", process.uid_name);
        printf("State:        %c\n", process.state);
        printf("Priority:     %d\n", process.priority - 20);

        if (process.ppid != 0)
        {
            printf("Parent PID:   %d\n", process.ppid);
        }
        else
        {
            printf("Parent PID:   None\n");
        }

        printf("Threads:      %d\n", process.threads);
        printf("Virtual Mem:  %ld MB\n", process.vmem_mb);
        printf("Physical Mem: %ld MB (RSS) (%.2f%%)\n", process.rss_mb, process.all_memory);
        printf("Running Time: %02ld:%02ld:%02ld\n", h, m, s);
        printf("=====================\n");

        fflush(stdout);

        if (watch == 1)
        {
            sleep(1);
        }
        else if (watch == 2)
        {
            sleep(process.seconds_update);
        }
        if (watch == 0)
        {
            break;
        }
        else
        {
            continue;
        }
    }

    return 0;
}