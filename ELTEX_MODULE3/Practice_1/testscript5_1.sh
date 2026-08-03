#!/bin/bash

PROG=./Task5_1

echo "Сборка"
gcc -Wall -Wextra -o "$PROG" Task5_1.c || { echo "ошибка сборки"; exit 1; }

WORK=$(mktemp -d)                      # тестируем во временной папке
trap 'rm -rf "$WORK"' EXIT
cd "$WORK" || exit 1

# тестовые файлы
echo "hello world" > a.txt
mkdir adir

PASS=0; FAIL=0
ok()  { echo "  [OK]   $1"; PASS=$((PASS+1)); }
bad() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }

# запуск: $1 — что подать на stdin; код возврата сохраняем в r
run() { echo "$1" | timeout 5 "$PROG" >out.txt 2>err.txt; }

echo "1) Несуществующий файл: ошибка и нет копии"
run "nope.txt"; r=$?
if [ $r -ne 0 ] && { [ -s err.txt ] || [ -s out.txt ]; } && [ ! -e nope.txt.copy ]; then
     ok "ненулевой код, сообщение выведено"; else bad "exit=$r"; fi

echo "2) Пустое имя (просто Enter)"
run ""; r=$?
if [ $r -ne 0 ]; then ok "завершилась с ошибкой"; else bad "exit=$r"; fi

echo "3) Ctrl+D вместо имени (сразу EOF)"
timeout 5 "$PROG" </dev/null >out.txt 2>err.txt; r=$?
if [ $r -ne 0 ]; then ok "завершилась с ошибкой"; else bad "exit=$r"; fi

echo "4) Каталог вместо файла"
run "adir"; r=$?
if [ $r -ne 0 ]; then ok "чтение не удалось, ненулевой код"; else bad "exit=$r"; fi

echo "5) Файл без прав чтения (не сработает под root)"
echo secret > noperm.txt; chmod 000 noperm.txt
run "noperm.txt"; r=$?
if [ $r -ne 0 ]; then ok "open не прошёл"; else bad "exit=$r"; fi
chmod 644 noperm.txt

echo
echo "Итог: OK=$PASS FAIL=$FAIL"
[ $FAIL -eq 0 ]