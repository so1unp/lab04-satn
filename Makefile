CC=gcc
BIN=./bin

# Agregamos -lpthread (hilos) y -lncurses (interfaz gráfica) a las flags de compilación
CFLAGS=-g -Wall -Wextra -Wshadow -Wconversion -Wunreachable-code -lpthread -lncurses

PROG=nave estacion servidor

LIST=$(addprefix $(BIN)/, $(PROG))

.PHONY: all
all: $(LIST)

# Regla específica para compilar 'nave'. 
# Junta el punto de entrada (nave_main.c) con la lógica del módulo (nave.c)
$(BIN)/nave: nave_main.c nave.c
	@mkdir -p $(BIN)
	$(CC) -o $@ $^ $(CFLAGS)

# Regla genérica para los otros procesos individuales (estacion y servidor)
$(BIN)/%: %.c
	@mkdir -p $(BIN)
	$(CC) -o $@ $< $(CFLAGS)

%: %.c
	@mkdir -p $(BIN)
	$(CC) -o $(BIN)/$@ $< $(CFLAGS)

test:
	@./test.sh ||:

.PHONY: clean
clean:
	rm -f $(LIST)

zip:
	git archive --format zip --output ${USER}-lab03.zip HEAD

html:
	pandoc -o README.html -f gfm README.md