CC=gcc
CFLAGS=-Wall -O2

space_invaders: 8080.o space_invaders.o
	$(CC) $(CFLAGS) 8080.o space_invaders.o -lSDL2 -o space_invaders

space_invaders.o: space_invaders.c
	$(CC) $(CFLAGS) -c space_invaders.c -o space_invaders.o

8080.o: 8080.c
	$(CC) $(CFLAGS) -c 8080.c -o 8080.o

