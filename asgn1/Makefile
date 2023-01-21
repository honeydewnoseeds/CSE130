CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra

all: memory

hello: memory.o
	$(CC) -o memory memory.o

hello.o: memory.c
	$(CC) $(CFLAGS) -c memory.c

clean: 
	rm -f memory memory.o

format: 
	clang-format -i -style=file *.[ch]
