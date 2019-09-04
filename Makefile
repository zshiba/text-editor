CFLAGS=-std=c99 -Wall -Wextra
SOURCES=editor.c
EXECUTABLE=editor

build: ${SOURCES}
	${CC} ${CFLAGS} ${SOURCES} -o ${EXECUTABLE}

clean:
	rm ${EXECUTABLE}
