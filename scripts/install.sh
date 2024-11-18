#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}System Monitor Installation Script${NC}\n"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Please run as root${NC}"
    exit 1
fi

# Install dependencies
echo -e "${GREEN}Installing dependencies...${NC}"
apt-get update
apt-get install -y build-essential linux-headers-$(uname -r) libncurses-dev

# Build kernel module
echo -e "\n${GREEN}Building kernel module...${NC}"
cd ../kernel
make clean
make
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build kernel module${NC}"
    exit 1
fi

# Install kernel module
echo -e "\n${GREEN}Installing kernel module...${NC}"
insmod system_monitor.ko
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to load kernel module${NC}"
    exit 1
fi

# Build userspace program
echo -e "\n${GREEN}Building userspace program...${NC}"
cd ../userspace
make clean
make
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to build userspace program${NC}"
    exit 1
fi

# Create systemd service
echo -e "\n${GREEN}Creating systemd service...${NC}"
cat > /etc/systemd/system/system-monitor.service << EOL
[Unit]
Description=System Monitor Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/system_monitor_display
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOL

# Install userspace program
cp system_monitor_display /usr/local/bin/
chmod +x /usr/local/bin/system_monitor_display

# Set up proc permissions
chmod 666 /proc/system_monitor_control

# Enable and start service
systemctl daemon-reload
systemctl enable system-monitor
systemctl start system-monitor

echo -e "\n${GREEN}Installation complete!${NC}"
echo -e "You can now:"
echo -e "1. Run '${YELLOW}system_monitor_display${NC}' to start the display program"
echo -e "2. Check '${YELLOW}cat /proc/system_monitor${NC}' for raw stats"
echo -e "3. View service status with '${YELLOW}systemctl status system-monitor${NC}'"
