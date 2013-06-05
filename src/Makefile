all:
	gcc -c -fpic -o simplehttp.o luasimplecurl.c -lcurl -llua5.2
	gcc -O -shared -fpic -o simplehttp.so simplehttp.o -lcurl -llua5.2
