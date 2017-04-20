CC = gcc
CFLAGS = -W -Wall -O2
TARGET = aio_echo_server
SRCS = main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

clean:
	rm -f $(TARGET)
