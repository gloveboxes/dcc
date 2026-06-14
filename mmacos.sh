rm -f dcc dccpeep dccrtlstrip

sh src/dcc/build-dcc.sh
clang -std=c89 -Wall -Wextra -O2 -o dccpeep src/dccpeep/dccpeep.c
clang -std=c89 -Wall -Wextra -O2 -o dccrtlstrip src/dccrtlstrip/dccrtlstrip.c

chmod +x runall.sh
chmod +x ma.sh