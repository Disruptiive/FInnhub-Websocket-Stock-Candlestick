CC=gcc
CFLAGS=-lwebsockets -lcjson

c: c.o
	$(CC) client.o $(CFLAGS) -o client

c.o:
	$(CC) -c client.c $(CFLAGS) -o client.o

clean:
	rm -f  *.o client