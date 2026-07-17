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

**Unidad I completa** (lexer, parser, análisis semántico) con tabla de
símbolos visible (`--symbols`) y built-ins de entrada. De la Unidad II, las
**Etapas 4, 5 y 6 están hechas**: `--ir` muestra el código intermedio (TAC) de
cualquier programa válido, `--opt` lo optimiza (propagación de constantes y
copias, plegado, simplificación algebraica y eliminación de código muerto, por
bloque básico), `--emit-c` genera el C equivalente y `-o` lo compila con gcc
hasta un ejecutable.

## Requisitos

- `gcc` (TDM-GCC, MinGW, o el del sistema en Linux/macOS)
- `make` (en Windows con TDM-GCC: `mingw32-make`)
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
./kelc --symbols programa.kel  # muestra la tabla de símbolos
./kelc --ir programa.kel       # muestra el código intermedio TAC (Etapa 4)
./kelc --opt programa.kel      # muestra el TAC optimizado (Etapa 5)
./kelc --stats programa.kel    # métricas del compilador y reducción del --opt
./kelc --emit-c programa.kel > programa.c   # imprime el C generado (Etapa 6)
./kelc programa.kel -o programa             # ejecutable vía gcc (Etapa 6)
./kelc --sem programa.kel      # solo reporta resultado semántico
./kelc --help                  # ayuda
```

`-o` invoca `gcc` (debe estar en el PATH) sobre el C generado y deja el `.c` al
lado del ejecutable.

## Tests

```bash
make test
```

Corre la suite de regresión en `tests/ok/` (deben compilar) y `tests/bad/`
(deben fallar).

Targets extra del Makefile:

- `make asan` — build con AddressSanitizer. **Requiere Linux/WSL**: TDM-GCC
  no incluye libasan, así que en Windows falla con `cannot find -lasan`.
- `make valgrind` — corre los tests bajo valgrind
- `make clean` — elimina binarios

## Estructura del proyecto

```
src/
  lexer.h   / lexer.c      — Etapa 1: tokenizador
  parser.h  / parser.c     — Etapa 2: parser descendente recursivo → AST
  semantic.h/ semantic.c   — Etapa 3: tabla de símbolos + chequeo de tipos
  diag.h    / diag.c       — reporte de errores con línea fuente y carat
  ir.h                     — Etapa 4: tipos del código intermedio (TAC)
  ir.c                     — Etapa 4: generación de TAC
  symtab.h  / symtab.c     — log de la tabla de símbolos
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
de diseño, roadmap) está en [`SPEC.md`](SPEC.md).

## Equipo

Grupo de 4 personas — Ingeniería en Informática y Sistemas, 5to semestre.
Docente: MSc. Ing. Manuel Yuri Apaza Valencia.
