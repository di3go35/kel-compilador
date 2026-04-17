# Kel — Compilador

Compilador del lenguaje **Kel** escrito en C para el curso de Compiladores y
Teoría de Lenguajes — UNJBG 2026-I.

Kel es un lenguaje imperativo, tipado estático con inferencia, con estética
inspirada en Kotlin y Rust.

```kel
fn suma(a: int, b: int) -> int {
  return a + b
}

fn main() {
  val nombre = "Diego"
  var puntaje = 0
  val nums: [int] = [1, 2, 3]

  for i in 0..3 {
    puntaje = suma(puntaje, nums[i])
  }

  if puntaje > 5 {
    println("Hola, " + nombre)
  } else {
    println("Puntaje bajo")
  }
}
```

## Estado

**Unidad I completa** (lexer, parser, análisis semántico). Unidad II (código
intermedio, optimización, generación final) pendiente.

## Requisitos

- `gcc` (TDM-GCC, MinGW, o el del sistema en Linux/macOS)
- `make`
- `bash` (para la suite de regresión)

## Compilar

```bash
make
```

Genera el ejecutable `kelc` (o `kelc.exe` en Windows).

## Uso

```bash
./kelc programa.kel            # compila y reporta fases
./kelc --tokens programa.kel   # muestra el stream de tokens
./kelc --ast programa.kel      # muestra el AST con tipos inferidos
./kelc --sem programa.kel      # solo reporta resultado semántico
./kelc --help                  # ayuda
```

## Tests

```bash
make test
```

Corre la suite de regresión en `tests/ok/` (deben compilar) y `tests/bad/`
(deben fallar).

Targets extra del Makefile:

- `make asan` — build con AddressSanitizer (requiere Linux/WSL)
- `make valgrind` — corre los tests bajo valgrind
- `make clean` — elimina binarios

## Estructura del proyecto

```
src/
  lexer.h   / lexer.c      — Etapa 1: tokenizador
  parser.h  / parser.c     — Etapa 2: parser descendente recursivo → AST
  semantic.h/ semantic.c   — Etapa 3: tabla de símbolos + chequeo de tipos
  diag.h    / diag.c       — reporte de errores con línea fuente y carat
  codegen.h                — esqueleto para Etapa 4 (TAC)
  main.c                   — CLI
tests/
  ok/                      — programas válidos
  bad/                     — programas inválidos (esperamos que fallen)
  run_tests.sh             — suite de regresión
CLAUDE.md                  — especificación completa del lenguaje
Makefile
```

## Documentación

La especificación completa del lenguaje (tokens, gramática, tipos, decisiones
de diseño) está en [`CLAUDE.md`](CLAUDE.md).

## Equipo

Grupo de 4 personas — Ingeniería en Informática y Sistemas, 5to semestre.
Docente: MSc. Ing. Manuel Yuri Apaza Valencia.
