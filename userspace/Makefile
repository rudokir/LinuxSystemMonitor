CC=gcc
CFLAGS=-Wall -Wextra
LIBS=-lncurses

all: display

display: system_monitor_display.c
	$(CC) $(CFLAGS) -o system_monitor_display system_monitor_display.c $(LIBS)

clean:
	rm -f system_monitor_display
