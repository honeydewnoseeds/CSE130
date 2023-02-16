BASE_OBJECTS   = queue.c
HEADERS        = queue.h
COMPILE        = -Wall -Wpedantic -Werror -Wextra
CC             = clang
REMOVE         = rm -f

all : queue.o 

queue.o : $(BASE_OBJECTS) $(HEADERS)
	$(CC) $(COMPILE) -c $(BASE_OBJECTS)

clean :
	$(REMOVE) queue.o

format :
	clang-format -i -style=file *.[ch]
