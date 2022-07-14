all: chatclient

chatclient: chatclient.o
	gcc chatclient.o -o chatclient

chatclient.o: chatclient.c
	gcc -c chatclient.c


clean: 
	rm -rf *.o chatclient 
