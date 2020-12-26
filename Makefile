EXE := mfxp

SRC_DIR := src
OBJ_DIR := obj

SRC := $(wildcard $(SRC_DIR)/*.c) $(wildcard libs/inih/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS := -Iinclude
CFLAGS   := -std=gnu99 -Wall -g
LDLIBS   := -lssl -lcrypto -lbsd -lreadline -lpthread

.PHONY: all clean tags

all: $(EXE) tags

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir $@

tags:
	-ctags -R

clean:
	$(RM) $(OBJ) $(EXE)
