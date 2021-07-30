CC = gcc
CLFAGS = -Wall -W $(shell pkg-config fuse3 --cflags --libs) -Iinclude/ # -D_FILE_OFFSET_BITS=64
LDFLAGS = $(shell pkg-config fuse3 --cflags --libs)


# directories
SRC = src
OBJ = obj
TST = tests


# get source files convert names into object files too
SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

# Testing
TSTS := $(TST)/fstest.c
TSTSBIN := $(patsubst $(TST)/%.c, $(TST)/bin/%, $(TSTS))

EXE = xv6fs

options = -d -f -s
MOUNT_POINT = mnt

.PHONY: all run stop clean test



all: $(EXE) $(MOUNT_POINT)
	
$(EXE): $(OBJS)
	$(CC) $(CLFAGS) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CLFAGS) -c $< -o $@

$(OBJ) $(MOUNT_POINT) $(TST)/bin:
	mkdir -p $@

$(TST)/bin/%: $(TST)/%.c
	$(CC) $(CFLAGS) $< -o $@ -lcriterion

tests: $(TST)/bin $(TSTSBIN)
	for test in $(TSTSBIN) ; do ./$$test --verbose -f; done

	

run: $(EXE) | $(MOUNT_POINT)
	./$(EXE) $(options) $(MOUNT_POINT)

stop: $(EXE) | $(MOUNT_POINT)
	fusermount -u $(MOUNT_POINT)

clean:
	$(RM) -r $(OBJ) $(MOUNT_POINT) $(EXE) $(TST)/bin
