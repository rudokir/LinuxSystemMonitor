#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}System Monitor Uninstallation Script${NC}\n"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Please run as root${NC}"
    exit 1
fi

# Stop and disable service
echo -e "${GREEN}Stopping system monitor service...${NC}"
systemctl stop system-monitor
systemctl disable system-monitor
rm /etc/systemd/system/system-monitor.service
systemctl daemon-reload

# Remove kernel module
echo -e "\n${GREEN}Removing kernel module...${NC}"
rmmod system_monitor

# Remove userspace program
echo -e "\n${GREEN}Removing userspace program...${NC}"
rm /usr/local/bin/system_monitor_display

echo -e "\n${GREEN}Uninstallation complete!${NC}"
