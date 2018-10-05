#!/bin/bash
RET=0
echo -n "Test using valgrind for memory errors and leaks: "
if [[ "$NOMEMTEST" -ne "1" ]] && hash valgrind 2>/dev/null; then
    NOMEMTEST=0;
    echo -e "\x1B[32menabled\x1B[0m";
else
    NOMEMTEST=1;
    echo -e "\x1B[33mdisabled\x1B[0m";
fi

if [[ "$OSTYPE" == "darwin" ]]; then
    LCRYPT=
else
    LCRYPT=-lcrypt
fi

run () {
    if [ ! -f $1/sources ]; then
       return;
    fi
    C_SRC=$(cat $1/sources)

    SOURCE=$1/$1.c
    OUT=$1.out
    rm "$OUT" 2> /dev/null

    gcc -I"../include" -O0 -g3 -Wall -Wextra -Winline -std=gnu99 $SOURCE $C_SRC -lm -lpcre2-8 -lcleri -luuid -luv $LCRYPT -o "$OUT"
    if [[ "$NOMEMTEST" -ne "1" ]]; then
        valgrind --tool=memcheck --error-exitcode=1 --leak-check=full -q ./$OUT
    else
        ./$OUT
    fi
    rc=$?; if [[ $rc != 0 ]]; then RET=$((RET+1)); fi
    rm "$OUT" 2> /dev/null
    rm -r "$OUT.dSYM" 2> /dev/null
}

if [ $# -eq 0 ]; then
    for d in test_*/ ; do
        run "${d%?}"
    done
else
    name=`echo $1 | sed 's/\(test_\)\?\(.*\?\)$/\2/g' | sed 's/\(.*\)\/$/\1/g'`
    run "test_$name"
fi

exit $RET