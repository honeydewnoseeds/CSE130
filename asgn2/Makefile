BASE_SOURCES   = httpserver.c
BASE_OBJECTS   = asgn2_helper_funcs.a
HEADERS        = asgn2_helper_funcs.h
COMPILE        = -Wall -Wpedantic -Werror -Wextra
CC             = clang
REMOVE         = rm -f 

all : httpserver.o $(BASE_OBJECTS)
	$(CC) -o httpserver httpserver.o $(BASE_OBJECTS)

httpserver.o : $(BASE_SOURCES) $(HEADERS)
	$(CC) $(COMPILE) -c $(BASE_SOURCES)

clean :
	$(REMOVE) httpserver httpserver.o

format : 
	clang-format -i -style=file *.[ch]

