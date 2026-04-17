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
echo "Resultado: $pass pasaron, $fail fallaron."
if [ "$fail" -ne 0 ]; then
    echo "Fallos:"
    for m in "${fails[@]}"; do echo "  - $m"; done
    exit 1
fi
exit 0
