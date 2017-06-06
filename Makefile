CFLAGS = -W -Wall -std=c11 -O2
TARGET = aio_echo_server
SOURCES = client.c error.c main.c
HEADERS = client.h error.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) -lrt

clean:
	rm -f $(TARGET)
