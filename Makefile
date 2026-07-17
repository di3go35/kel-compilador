CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=gnu11 -g -D__USE_MINGW_ANSI_STDIO
SRC = src/main.c src/lexer.c src/parser.c src/semantic.c src/diag.c src/symtab.c src/ir.c src/emit_c.c src/optimize.c
OUT = kelc

# Build con AddressSanitizer + LeakSanitizer (sug. 10).
# ASan detecta leaks, use-after-free, buffers fuera de rango, etc.
# En mingw/Windows puede no estar disponible; usar WSL o Linux para
# resultados fiables.
ASAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
OUT_ASAN = kelc_asan

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

test: $(OUT)
	bash tests/run_tests.sh

# Compila con sanitizers y corre la suite. Si falla, revisar stderr
# para ver el backtrace del leak / error.
asan: $(SRC)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) -o $(OUT_ASAN) $(SRC)
	@echo "== Ejecutando suite bajo AddressSanitizer =="
	KELC=./$(OUT_ASAN) bash tests/run_tests.sh

# Variante con valgrind (Linux). Corre el compilador en cada test
# y reporta leaks al final.
valgrind: $(OUT)
	@for f in tests/ok/*.kel tests/bad/*.kel; do \
		echo "--- $$f ---"; \
		valgrind --leak-check=full --error-exitcode=1 --quiet ./$(OUT) $$f >/dev/null; \
	done

clean:
	rm -f $(OUT) $(OUT).exe $(OUT_ASAN) $(OUT_ASAN).exe
