BASE_SOURCES   = queue.c
HEADERS        = queue.h
COMPILE        = -Wall -Wpedantic -Werror -Wextra
CC             = clang
REMOVE         = rm -f

all : queue.o 
	$(CC) -o queue queue.o 

queue.o : $(BASE_SOURCES) $(HEADERS)
	$(CC) $(COMPILE) -c $(BASE_SOURCES)

clean :
	$(REMOVE) queue queue.o

format :
	clang-format -i -style=file *.[ch]
