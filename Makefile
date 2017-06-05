CC = gcc
CFLAGS = -W -Wall -O2
TARGET = aio_echo_server
SOURCES = aiocb.c client.c error.c main.c
HEADERS = aiocb.h client.h error.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) -lrt

clean:
	rm -f $(TARGET)