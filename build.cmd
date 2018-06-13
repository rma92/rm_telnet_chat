tcc -o main-tcc.exe main.c -lws2_32
gcc main.c -lws2_32 -o main-gcc.exe
cl /Femain-vcc.exe main.c

