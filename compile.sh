#!/bin/bash

clear

ulimit -n 524288 10485760 ; VAL=$( ulimit -Sn ) ; echo -e "MAX_FILENO: $VAL"

FLAG=" -lpthread -lcrypto -lssl -lz" #  -lsqlite3 -largon2
FILE=$(mktemp)
NAME="fileDB"

echo -e "\nKilling Service" ; $( killall $NAME )

echo -e "\nCompiling Service"
if !( g++ -o ./Build/$NAME ./Services/$NAME.cpp -I ./Modules $FLAG ) 2> "$FILE"; then
      echo -e "\n" ; cat "$FILE" >&2 ; exit
else
      echo -e "Done"
fi

echo -e "\nRunning Service" ; ./Build/$NAME;
