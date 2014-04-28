all:
	gcc -std=gnu99 -ofileserver src/*.c -lpthread

dbg:
	gcc -g -std=gnu99 -ofileserver src/*.c -lpthread

clean:
	rm fileserver
