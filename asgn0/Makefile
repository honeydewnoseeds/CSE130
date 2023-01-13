CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra

all: hello

hello: hello.o
	$(CC) -o hello hello.o

hello.o: hello.c
	$(CC) $(CFLAGS) -c hello.c

clean: 
	rm -f hello hello.o

format: 
	clang-format -i -style=file *.[ch]
