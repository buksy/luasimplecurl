all:
	gcc -c -fpic -o simplehttp.o luasimplehttp.c -lcurl -llua5.2 -lyajl
	gcc -O -shared -fpic -o simplehttp.so simplehttp.o -lcurl -llua5.2 -lyajl
