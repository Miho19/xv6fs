CC = gcc
CLFAGS = -Wall -W -lcriterion -Iinclude/

OBJ = obj
SRC = src

GET_SRC := $(shell cp ../src/fs.c $(SRC)/fs.c )
GET_H := $(shell cp ../include/fs.h include/fs.h)
GET_FSIMG := $(shell cp ../image/fs_copy.img fs.img)

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

EXE = fstest

.PHONY: all test clean

all: $(EXE)
	./fstest 

$(EXE): $(OBJS)
	$(CC) $(CLFAGS) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CLFAGS) -c $< -o $@

$(OBJ):
	mkdir -p $@

clean:
	rm -rf $(EXE) $(OBJ) src/fs.c include/fs.h fs.img



