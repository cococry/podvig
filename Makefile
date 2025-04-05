include config.mk

CC 				= cc
LIBS 			= -lleif ${RUNARA_LIBS} ${PLATFORM_SPECIFIC_LIBS} 
CFLAGS		= ${WINDOWING} ${ADDITIONAL_FLAGS} -Wall -pedantic -O3 -ffast-math -g
SRC_FILES = $(wildcard src/*.c)
OBJ_FILES = $(SRC_FILES:.c=.o)

all: lib/libpodvig.a

lib/libpodvig.a: $(OBJ_FILES)
	mkdir -p lib
	ar cr lib/libpodvig.a $(OBJ_FILES)
	rm -f $(OBJ_FILES)

%.o: %.c
	${CC} -c $< -o $@ ${LIBS} ${CFLAGS}

lib:
	mkdir -p lib

clean:
	rm -rf lib

install:
	cp lib/libpodvig.a /usr/local/lib/ 
	cp -r include/podvig /usr/local/include/ 

uninstall:
	rm -f /usr/local/lib/libpodvig.a
	rm -rf /usr/local/include/podvig/

.PHONY: all clean install uninstall
