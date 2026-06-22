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
#include <termios.h>
#include "PID_view.h"
#define BUFFER_MAX 1025
#define PATH_MAX 32

int main(int argc, char *argv[])
{
    struct termios new_settings, old_settings;
    int watch = 0;
    char *endptr;
    long time = 0;
    long pid_val = 0;
    if (argc < 2)
    {
        usage();
        return 1;
    }
    static const struct option arguments[] =
        {
            {"help", no_argument, NULL, 'h'},
            {"watch", no_argument, NULL, 'w'},
            {"seconds", required_argument, NULL, 's'},
            {NULL, 0, NULL, 0}};
    int opt;
    struct process_in_ram process = {0};
    while ((opt = getopt_long(argc, argv, "hws:", arguments, NULL)) != -1)
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
            errno = 0;
            watch = 2;
            time = strtol(optarg, &endptr, 0);
            if (time <= 0)
            {
                fprintf(stderr, "enter a number greater than zero");
                return 1;
            }
            break;
        }
    }
    if (optind >= argc)
    {
        usage();
        return 1;
    }
    pid_val = strtol(argv[optind], &endptr, 0);
    process.pid = (pid_t)pid_val;
    process.seconds_update = (int)time;
    if (watch == 1 || watch == 2)
    {
        tcgetattr(STDIN_FILENO, &old_settings);
        new_settings = old_settings;
        new_settings.c_cc[VTIME] = 0;
        new_settings.c_cc[VMIN] = 0;
        new_settings.c_lflag &= ~ECHO;
        new_settings.c_lflag &= ~ICANON;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    }
    while (true)
    {
        char symbol_to_exit = {0};
        if (watch == 1 || watch == 2)
        {
            ssize_t n = read(STDIN_FILENO, &symbol_to_exit, 1);
            if (n > 0 && symbol_to_exit == 'q')
            {
                break;
            }
        }
        char filepath[PATH_MAX];
        char buffer[BUFFER_MAX] = {0};

        snprintf(filepath, sizeof(filepath), "/proc/%d/status", process.pid);

        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Process %d not found or kill\n", process.pid);
            if (watch == 1 || watch == 2)
            {
                tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            }
            return 1;
        }

        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        if (bytes_read <= 0)
        {
            fprintf(stderr, "Failed to read status\n");
            if (watch == 1 || watch == 2)
            {
                tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            }
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
        printf("=== PID-View v0.5 ===\n");
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
            continue;
        }
        else if (watch == 2)
        {

            sleep(process.seconds_update);
            continue;
        }
        if (watch == 0)
        {
            break;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

    return 0;
}