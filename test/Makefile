# Host build of the CONFIG.TXT parser and key table. Run: make -C test
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wno-sign-conversion -g -fsanitize=address,undefined
SRCS = test_cfgfile.c ../main/ptt_cfgfile.c ../main/ptt_keynames.c

.PHONY: test clean
test: test_cfgfile
	./test_cfgfile

test_cfgfile: $(SRCS) ../main/ptt_cfgfile.h ../main/ptt_keynames.h
	$(CC) $(CFLAGS) -I../main -o $@ $(SRCS)

clean:
	rm -rf test_cfgfile test_cfgfile.dSYM
