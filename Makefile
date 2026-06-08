CC=gcc
BIN=./bin/.


CFLAGS=-g -Wall -Wextra -Wshadow -Wconversion -Wunreachable-code -I "."


LDFLAGS=-lpthread -lncurses

PROG=nave estacion servidor

LIST=$(addprefix $(BIN)/, $(PROG))

.PHONY: all
all: $(LIST)

$(BIN)/%: %.c
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

test:
	@./test.sh ||:

.PHONY: clean
clean:
	rm -f $(LIST)

zip:
	git archive --format zip --output ${USER}-lab03.zip HEAD

html:
	pandoc -o README.html -f gfm README.md