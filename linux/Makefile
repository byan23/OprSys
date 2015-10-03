CC = gcc
CFLAGS = -Wall -Werror

mysh: mysh.o
	$(CC) -o mysh mysh.o $(CFLAGS)
mysh.o: mysh.c
	$(CC) -c mysh.c

clean:
	rm -fv *.o
	rm mysh
