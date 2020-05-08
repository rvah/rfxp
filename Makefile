CC = cc
FILES = main.c \
		conn.c \
		crypto.c \
		filesystem.c \
		site.c \
		ui.c \
		command.c \
		config.c \
		thread.c \
		msg.c \
		log.c \
		parse.c \
		util.c \
		ui_helpers.c \
		skiplist.c \
		sort.c \
		priolist.c \
		hilight.c \
		ui_indicator.c \
		libs/inih/ini.c

LIBS = -lssl -lcrypto -lbsd -lreadline -lpthread
FLAGS = -std=gnu99 -Wall -g
OUT = mfxp

all: $(FILES)
	$(CC) $(FLAGS) -o $(OUT) $(FILES) $(LIBS)

clean:
	rm -f *.o $(OUT)
