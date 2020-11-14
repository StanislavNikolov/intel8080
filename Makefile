CC=gcc
CFLAGS=-Wall -O2

space_invaders: 8080.o space_invaders.o
	$(CC) $(CFLAGS) 8080.o space_invaders.o -lSDL2 -o space_invaders

debug: 8080.o debug.o other.o

space_invaders.o: space_invaders.c
	$(CC) $(CFLAGS) -c space_invaders.c -o space_invaders.o

8080.o: 8080.c
	$(CC) $(CFLAGS) -c 8080.c -o 8080.o

debug.o: debug.c
	$(CC) $(CFLAGS) -c debug.c -o debug.o

other.o: other.c
	$(CC) $(CFLAGS) -c other.c -o other.o
