/*
 * System Monitor Display Program
 *
 * This program reads system statistics from the kernel module through /proc
 * and displays them in a user-friendly ncurses interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <time.h>

/* Constants */
#define PROC_FILE "/proc/system_monitor"
#define BUFFER_SIZE 4096
#define MAX_DISKS 16

/*Data Structures */

/**
 * system_stats - Structure to hold parsed system statistics
 *
 * Stores various system metrics collected from the kernel module.
 * All values are collected per reading cycle.
 */
struct system_stats {
    // CPU statistics
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;

    // Memory statistics (in KB)
    unsigned long total_mem;
    unsigned long free_mem;
    unsigned long used_mem;

    // Process information
    int process_count;

    // Network statistics
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long rx_packets;
    unsigned long tx_packets;
};

/* Global Variables */
static volatile int running = 1;

/* Function Declarations */

/**
 * signal_handler - Handles interrupt signals
 * @signo: Signal number (unused)
 *
 * Sets running flag to false for clean program termination.
 */
void signal_handler(int signo __attribute__((unused))) {
    running = 0;
}

/**
 * parse_line - Parses a single line of statistics
 * @line: Input line from proc file
 * @stats: Statistics structure to update
 *
 * Parses colon-separated key-value pairs and updates appropriate fields.
 */
void parse_line(char *line, struct system_stats *stats) {
    char *key = strtok(line, ":");
    char *value = strtok(NULL, "\n");

    if (!key || !value) return;

    if (strcmp(key, "cpu_stats") == 0) {
        sscanf(value, "%llu,%llu,%llu,%llu", &stats->user, &stats->nice, &stats->system, &stats->idle);
    } else if (strcmp(key, "memory_stats") == 0 ) {
        sscanf(value, "%lu,%lu,%lu", &stats->total_mem, &stats->free_mem, &stats->used_mem);
    } else if (strcmp(key, "process_count") == 0) {
        sscanf(value, "%d", &stats->process_count);
    } else if (strcmp(key, "network_stats") == 0) {
        sscanf(value, "%lu,%lu,%lu,%lu", &stats->rx_bytes, &stats->tx_bytes, &stats->rx_packets, &stats->tx_packets);
    }
}

/**
 * read_stats - Reads and parses all statistics from proc file
 * @stats: Statistics structure to fill
 *
 * Opens proc file, reads all lines, and parses each line.
 * Exits program if proc file cannot be opened.
 */
void read_stats(struct system_stats *stats) {
    FILE *fp = fopen(PROC_FILE, "r");
    if (!fp) {
        perror("Failed to open proc file");
        exit(1);
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        parse_line(line, stats);
    }

    fclose(fp);
}

/**
 * display_stats - Displays statistics using ncurses
 * @stats: Statistics to display
 *
 * Formats and displays all statistics in a colored, organized layout.
 * Uses different colors for different types of statistics.
 */
void display_stats(struct system_stats *stats) {
    clear();

    float cpu_total = stats->user + stats->nice + stats->system + stats->idle;
    float cpu_used = (cpu_total - stats->idle) / cpu_total * 100;

    attron(COLOR_PAIR(1));
    mvprintw(1, 2, "CPU Usage: %-6.2f%%", cpu_used);

    float mem_used_gb = stats->used_mem / (1024.0 * 1024);
    float mem_total_gb = stats->total_mem / (1024.0 * 1024);

    attron(COLOR_PAIR(2));
    mvprintw(3, 2, "Memory: %-6.2f GB / %-6.2f GB (%-6.1f%%)", mem_used_gb, mem_total_gb, (mem_used_gb / mem_total_gb) * 100);

    attron(COLOR_PAIR(3));
    mvprintw(5, 2, "Processes: %d", stats->process_count);

    attron(COLOR_PAIR(4));
    mvprintw(7, 2, "Network:");
    mvprintw(8, 4, "RX: %-6.2f MB (%-6.2f MB/s)", stats->rx_bytes / (1024.0 * 1024), stats->rx_packets / (1024.0 * 1024));
    mvprintw(9, 4, "TX: %-6.2f MB (%-6.2f MB/s)", stats->tx_bytes / (1024.0 * 1024), stats->tx_packets / (1024.0 * 1024));

    refresh();
}

/**
 * main - Program entry point
 *
 * Initializes ncurses, sets up signal handling, and runs main display loop.
 * Updates display every 500ms until interrupted.
 */
int main() {
    signal(SIGINT, signal_handler);

    initscr();
    start_color();
    use_default_colors();
    curs_set(0);
    noecho();

    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_BLUE, -1);
    init_pair(3, COLOR_YELLOW, -1);
    init_pair(4, COLOR_MAGENTA, -1);

    struct system_stats stats;

    while (running) {
        read_stats(&stats);
        display_stats(&stats);
        usleep(500000);
    }

    endwin();
    return 0;
}
