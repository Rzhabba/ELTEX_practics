#!/bin/bash
#проверяем параметры

echo "Сборка"
make || { echo "ошибка сборки"; exit 1; }
PROG="$(pwd)/final"

WORK=$(mktemp -d)                       # тестируем во временной папке
trap 'rm -rf "$WORK"' EXIT
cd "$WORK" || exit 1

echo "hello world" > a.txt
echo "second"      > b.txt
head -c 100000 /dev/urandom > big.bin   # больше BUF_SIZE — много блоков
: > empty.txt

PASS=0; FAIL=0
ok()  { echo "  [OK]   $1"; PASS=$((PASS+1)); }
bad() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }
run() { timeout 5 "$PROG" "$@" >out.txt 2>err.txt; }

echo "1) Один файл"
run a.txt; r=$?
if [ $r -eq 0 ] && cmp -s a.txt a.txt.copy; then ok "копия совпадает"; else bad "exit=$r"; fi

echo "2) Несколько файлов за один запуск"
run a.txt b.txt big.bin empty.txt; r=$?
if [ $r -eq 0 ] && cmp -s a.txt a.txt.copy && cmp -s b.txt b.txt.copy \
   && cmp -s big.bin big.bin.copy && cmp -s empty.txt empty.txt.copy; then
    ok "все копии совпадают"; else bad "exit=$r"; fi

echo "3) Несуществующий файл среди существующих"
run a.txt nope.txt b.txt; r=$?
if [ $r -eq 0 ] && cmp -s a.txt a.txt.copy && cmp -s b.txt b.txt.copy \
   && [ ! -e nope.txt.copy ] && [ -s err.txt ]; then
    ok "пропущен с диагностикой в stderr"; else bad "exit=$r"; fi

echo "4) Все файлы не существуют"
run nope1.txt nope2.txt; r=$?
if [ -s err.txt ] && [ ! -e nope1.copy ] && [ ! -e nope1.txt.copy ]; then
    ok "копий нет, ошибки в stderr"; else bad "exit=$r"; fi

echo "5) Нет аргументов — usage"
run; r=$?
if [ $r -ne 0 ] && [ -s err.txt ]; then ok "usage в stderr"; else bad "exit=$r"; fi

echo "6) -p без имени канала"
run -p; r=$?
if [ $r -ne 0 ] && [ -s err.txt ]; then ok "ошибка"; else bad "exit=$r"; fi

echo "7) Именованный канал (-p)"
run -p mypipe a.txt big.bin; r=$?
if [ $r -eq 0 ] && cmp -s a.txt a.txt.copy && cmp -s big.bin big.bin.copy; then
    ok "копии совпадают"; else bad "exit=$r"; fi
if [ ! -e mypipe ] && [ ! -e mypipe.ack ]; then ok "FIFO удалены после работы"; else bad "FIFO остались"; fi


echo "8) Повторный запуск перезаписывает копию (O_TRUNC)"
echo "new content" > a.txt
run a.txt; r=$?
if [ $r -eq 0 ] && cmp -s a.txt a.txt.copy; then ok "копия обновлена"; else bad "exit=$r"; fi

echo
echo "Итог: OK=$PASS FAIL=$FAIL"
[ $FAIL -eq 0 ]