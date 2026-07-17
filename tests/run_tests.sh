#!/usr/bin/env bash
# Script de regresión: corre kelc sobre todos los .kel de tests/ok y tests/bad
# y verifica que los códigos de salida sean los esperados.

set -u
cd "$(dirname "$0")/.."

KELC=${KELC:-./kelc}
if [ ! -x "$KELC" ] && [ ! -x "$KELC.exe" ]; then
    echo "$KELC no compilado. Ejecuta 'make' primero." >&2
    exit 2
fi

pass=0
fail=0
fails=()

run_case() {
    local file=$1
    local want=$2   # 0 = debe pasar, 1 = debe fallar
    "$KELC" "$file" >/dev/null 2>&1
    local got=$?
    # Normalizar: cualquier salida !=0 cuenta como "falló"
    if [ "$want" = 0 ] && [ "$got" = 0 ]; then
        pass=$((pass+1))
        printf "  ok   %s\n" "$file"
    elif [ "$want" = 1 ] && [ "$got" != 0 ]; then
        pass=$((pass+1))
        printf "  ok   %s (falló como se esperaba)\n" "$file"
    else
        fail=$((fail+1))
        fails+=("$file (esperado exit=$want, obtuvo exit=$got)")
        printf "  FAIL %s (esperado exit=%s, obtuvo exit=%s)\n" "$file" "$want" "$got"
    fi
}

echo "== tests/ok (deben compilar) =="
for f in tests/ok/*.kel; do
    [ -e "$f" ] || continue
    run_case "$f" 0
done

echo
echo "== tests/bad (deben fallar) =="
for f in tests/bad/*.kel; do
    [ -e "$f" ] || continue
    run_case "$f" 1
done

echo
echo "== tests/symbols (salida de --symbols) =="
for f in tests/symbols/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    if [ ! -e "$exp" ]; then
        fail=$((fail+1))
        fails+=("$f (falta $exp)")
        printf "  FAIL %s (falta %s)\n" "$f" "$exp"
        continue
    fi
    # tr -d '\r': el binario de MinGW abre stdout en modo texto y traduce
    # \n a \r\n incluso en un pipe; el .expected está en LF puro.
    got=$("$KELC" --symbols "$f" 2>/dev/null | tr -d '\r')
    if [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1))
        printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1))
        fails+=("$f (--symbols no coincide con $exp)")
        printf "  FAIL %s (--symbols no coincide)\n" "$f"
        printf "    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" \
            "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done

echo
echo "== tests/ir (salida de --ir) =="
for f in tests/ir/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    if [ ! -e "$exp" ]; then
        fail=$((fail+1))
        fails+=("$f (falta $exp)")
        printf "  FAIL %s (falta %s)\n" "$f" "$exp"
        continue
    fi
    # tr -d '\r' en los dos lados: ver la nota de tests/symbols.
    got=$("$KELC" --ir "$f" 2>/dev/null | tr -d '\r')
    if [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1))
        printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1))
        fails+=("$f (--ir no coincide con $exp)")
        printf "  FAIL %s (--ir no coincide)\n" "$f"
        printf "    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" \
            "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done

echo
echo "== tests/run (end-to-end: kelc -> gcc -> ejecutar) =="
E2E_DIR="$(mktemp -d)"
for f in tests/run/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    if [ ! -e "$exp" ]; then
        fail=$((fail+1))
        fails+=("$f (falta $exp)")
        printf "  FAIL %s (falta %s)\n" "$f" "$exp"
        continue
    fi
    base="$(basename "${f%.kel}")"
    cfile="$E2E_DIR/$base.c"
    exe="$E2E_DIR/$base.exe"
    if ! "$KELC" --emit-c "$f" > "$cfile" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (kelc --emit-c falló)")
        printf "  FAIL %s (kelc --emit-c falló)\n" "$f"; continue
    fi
    # El requisito de §5.4.2 solo vale si un test lo hace fallar:
    if ! gcc -Wall -Wextra -Werror -o "$exe" "$cfile" 2> "$E2E_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (el C generado no compila limpio)")
        printf "  FAIL %s (gcc rechazó el C generado):\n" "$f"
        sed 's/^/    /' "$E2E_DIR/$base.gcc.log"; continue
    fi
    # tr -d '\r' en los dos lados: ver la nota de tests/symbols.
    got=$("$exe" < /dev/null | tr -d '\r')
    if [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (salida distinta)")
        printf "  FAIL %s\n    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" "$f" "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done
rm -rf "$E2E_DIR"

echo
echo "== tests/read (end-to-end con stdin) =="
READ_DIR="$(mktemp -d)"
for f in tests/read/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"; sin="${f%.kel}.stdin"
    base="$(basename "${f%.kel}")"
    cfile="$READ_DIR/$base.c"; exe="$READ_DIR/$base.exe"
    want_rc=0
    [ -e "${f%.kel}.exitcode" ] && want_rc=$(cat "${f%.kel}.exitcode")
    if ! "$KELC" --emit-c "$f" > "$cfile" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (kelc falló)"); printf "  FAIL %s (kelc falló)\n" "$f"; continue
    fi
    if ! gcc -Wall -Wextra -Werror -o "$exe" "$cfile" 2> "$READ_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (C no compila)"); printf "  FAIL %s (gcc):\n" "$f"
        sed 's/^/    /' "$READ_DIR/$base.gcc.log"; continue
    fi
    # rc del ejecutable, no del tr: capturamos la salida y $? antes de
    # normalizar. Con un pipe, $? sería el de tr (siempre 0) y invalido.kel
    # (que debe salir con 1) pasaría en falso.
    raw=$("$exe" < "$sin" 2>/dev/null); rc=$?
    got=$(printf '%s' "$raw" | tr -d '\r')
    if [ "$rc" = "$want_rc" ] && [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (salida o exit code)")
        printf "  FAIL %s (exit %s, esperado %s)\n    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" \
               "$f" "$rc" "$want_rc" "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done
rm -rf "$READ_DIR"

echo
echo "== equivalencia del optimizador (--opt no altera la salida) =="
EQ_DIR="$(mktemp -d)"
for f in tests/run/*.kel; do
    [ -e "$f" ] || continue
    base="$(basename "${f%.kel}")"
    c0="$EQ_DIR/$base.0.c"; e0="$EQ_DIR/$base.0.exe"
    c1="$EQ_DIR/$base.1.c"; e1="$EQ_DIR/$base.1.exe"
    if ! "$KELC" --emit-c "$f" > "$c0" 2>/dev/null \
      || ! "$KELC" --opt --emit-c "$f" > "$c1" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (emisión falló)")
        printf "  FAIL %s (emisión falló)\n" "$f"; continue
    fi
    # El C optimizado también debe compilar limpio con -Werror: si el
    # optimizador deja una etiqueta sin salto, aquí revienta con -Wunused-label.
    if ! gcc -Wall -Wextra -Werror -o "$e0" "$c0" 2>/dev/null \
      || ! gcc -Wall -Wextra -Werror -o "$e1" "$c1" 2> "$EQ_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (el C optimizado no compila)")
        printf "  FAIL %s (gcc rechazó el C optimizado):\n" "$f"
        sed 's/^/    /' "$EQ_DIR/$base.gcc.log"; continue
    fi
    o0=$("$e0" < /dev/null | tr -d '\r'); o1=$("$e1" < /dev/null | tr -d '\r')
    if [ "$o0" = "$o1" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (--opt cambió la salida)")
        printf "  FAIL %s (--opt cambió la salida)\n    --- sin --opt ---\n%s\n    --- con --opt ---\n%s\n" \
               "$f" "$o0" "$o1"
    fi
done
rm -rf "$EQ_DIR"

echo
echo "== tests/opt (cada pase dispara sobre --opt) =="
for f in tests/opt/*.kel; do
    [ -e "$f" ] || continue
    base="${f%.kel}"
    got=$("$KELC" --opt "$f" 2>/dev/null | tr -d '\r')
    okc=1; why=""
    if [ -e "$base.present" ]; then
        while IFS= read -r pat; do
            [ -z "$pat" ] && continue
            case "$got" in *"$pat"*) : ;; *) okc=0; why="falta: $pat" ;; esac
        done < "$base.present"
    fi
    if [ -e "$base.absent" ]; then
        while IFS= read -r pat; do
            [ -z "$pat" ] && continue
            case "$got" in *"$pat"*) okc=0; why="sobra: $pat" ;; esac
        done < "$base.absent"
    fi
    if [ "$okc" = 1 ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f ($why)")
        printf "  FAIL %s (%s)\n    --- --opt ---\n%s\n" "$f" "$why" "$got"
    fi
done

echo
echo "Resultado: $pass pasaron, $fail fallaron."
if [ "$fail" -ne 0 ]; then
    echo "Fallos:"
    for m in "${fails[@]}"; do echo "  - $m"; done
    exit 1
fi
exit 0
