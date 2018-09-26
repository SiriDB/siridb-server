#!/bin/bash
RET=0

run () {
    C_SRC=$(cat $1/sources)

    SOURCE=$1/$1.c
    OUT=$1.out
    rm "$OUT" 2> /dev/null

    gcc -I"../include" -O0 -g3 -Wall -Wextra -Winline -std=gnu99 $SOURCE $C_SRC -o "$OUT"
    if [[ "$NOMEMTEST" -ne "1" ]] && hash valgrind 2>/dev/null; then
        echo -n "(memtest) "
        valgrind --tool=memcheck --error-exitcode=1 --leak-check=full -q ./$OUT
    else
        ./$OUT
    fi
    rc=$?; if [[ $rc != 0 ]]; then RET=$((RET+1)); fi
    rm "$OUT" 2> /dev/null
}

if [ $# -eq 0 ]; then
    for d in test_*/ ; do
        run "${d%?}"
    done
else
    run "$1"
fi

exit $RET