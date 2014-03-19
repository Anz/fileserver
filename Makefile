all:
	gcc -std=gnu99 -ofileserver src/*.c -lpthread

clean:
	rm fileserver
