CC = gcc
CFLAGS = -g
LDFLAGS = -g
BIN_NATIVE = oss
BIN_TARGET = usr
OBJ_NATIVE = oss.o
OBJ_TARGET = usr.o

.SUFFIXES:
.SUFFIXES: .c .o .h

all: oss usr
$(BIN_NATIVE): $(OBJ_NATIVE)
	$(CC) $(LDFLAGS) -o $(BIN_NATIVE) $(OBJ_NATIVE) -lpthread
$(BIN_TARGET): $(OBJ_TARGET)
	$(CC) $(LDFLAGS) -o $(BIN_TARGET) $(OBJ_TARGET) -lpthread
$(OBJ_NATIVE): oss.c
	$(CC) $(CFLAGS) -c oss.c shmem.h
$(OBJ_TARGET): usr.c
	$(CC) $(CFLAGS) -c usr.c shmem.h
clean:
	/bin/rm -f *.o $(BIN_NATIVE) $(BIN_TARGET)
