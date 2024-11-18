# Linux System Monitor

A Linux kernel module and userspace application for real-time system monitoring.

## Overview

This project consists of two main components:
1. Kernel Module (`system_monitor.ko`): Collects system statistics
2. Userspace Application (`system_monitor_display`): Displays statistics in a user-friendly interface

This project is a small project, that may improve in the future to learn the fundamentals of the Linux kernel space and have it working with a user space application

### Features

- CPU usage monitoring
- Memory usage statistics
- Process tracking
- Network I/O monitoring
- Disk I/O statistics
- Historical data tracking
- Real-time updates

## Building and Installation

### Prerequisites

```bash
# Install required packages
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) ncurses-dev
```

### Kernel Module

```bash
# Build the kernel module
cd kernel
make

# Load the module
sudo insmod system_monitor.ko

# Verify module is loaded
lsmod | grep system_monitor

# View raw statistics
cat /proc/system_monitor
```

### Userspace Application

```bash
# Build the display program
cd userspace
make

# Run the monitor
./system_monitor_display
```

## Usage

### Kernel Module Control

The kernel module creates two proc entries:
- `/proc/system_monitor`: Statistics output
- `/proc/system_monitor_control`: Control interface

Control commands:
```bash
# Enable monitoring
echo "enable" > /proc/system_monitor_control

# Disable monitoring
echo "disable" > /proc/system_monitor_control
```

### Display Program

The display program shows:
- CPU usage with percentage
- Memory usage and available memory
- Process count and top processes
- Network I/O rates
- Disk I/O statistics

Controls:
- `Ctrl+C`: Exit
- `r`: Refresh display
- `q`: Quit

## Project Structure

```
system_monitor/
├── kernel/
│   ├── system_monitor.c
│   └── Makefile
├── userspace/
│   ├── system_monitor_display.c
│   └── Makefile
├── scripts/
│   ├── install.sh
│   ├── uninstall.sh
│   └── monitor.service
└── README.md
```

## Troubleshooting

Common issues and solutions:

1. Scripts won't execute:
   ```bash
   chmod +x scripts/*.sh
   ```
   Make the scripts executable.
   
2. Module won't load:
   ```bash
   dmesg | tail
   ```
   Check kernel logs for specific errors.

3. Permission denied:
   ```bash
   sudo chmod 666 /proc/system_monitor_control
   ```

4. Display program shows no data:
   - Verify module is loaded
   - Check proc entries exist
   - Ensure monitoring is enabled

## Development

To add new features:

1. Kernel Module:
   - Add new statistics collection in `system_monitor.c`
   - Update proc output format
   - Rebuild module

2. Display Program:
   - Add parsing for new statistics
   - Update display layout
   - Rebuild display program

## License

GPL v2
