#!/bin/sh
make clean
i686-w64-mingw32-gcc -Iext/sqlite-amalgamation-3420000/ ext/sqlite-amalgamation-3420000/sqlite3.c -pthread -c
i686-w64-mingw32-g++ -std=gnu++17 -DMINGW -Iext/sqlite-amalgamation-3420000/ -I ext/pugixml-1.13/src/ emlconv.cc  ext/pugixml-1.13/src/pugixml.cpp sqlwriter.cc  -c  -pthread 
i686-w64-mingw32-g++ -std=gnu++17 emlconv.o pugixml.o sqlite3.o sqlwriter.o  -static-libgcc -static-libstdc++ -o emlconv -pthread
i686-w64-mingw32-strip emlconv.exe 
zip emlconv-$(date +%Y%m%d-%H%M).zip emlconv.exe doPS1 doPS2 useful-views useful-queries tk-and-ps2script ps1script makeheader

