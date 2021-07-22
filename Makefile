CC = gcc
CLFAGS = -Wall -W $(shell pkg-config fuse3 --cflags --libs) -Iinclude/ -D_FILE_OFFSET_BITS=64
LDFLAGS = $(shell pkg-config fuse3 --cflags --libs)


# directories
SRC = src
OBJ = obj

# get source files convert names into object files too
SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

EXE = main

options = -d -f -s
MOUNT_POINT = mnt

.PHONY: all run clean



all: $(EXE)
	
$(EXE): $(OBJS)
	$(CC) $(CLFAGS) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CLFAGS) -c $< -o $@

$(OBJ) $(MOUNT_POINT):
	mkdir -p $@


run: $(EXE) | $(MOUNT_POINT)
	./$(EXE) $(options) $(MOUNT_POINT)


clean:
	$(RM) -r $(OBJ) $(MOUNT_POINT) $(EXE)
