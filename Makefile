all: server client

server: bin/aurrasd

client: bin/aurras

bin/aurrasd: obj/aurrasd.o
	gcc -g obj/aurrasd.o -o bin/aurrasd

obj/aurrasd.o: src/aurrasd.c
	gcc -Wall -g -c src/aurrasd.c -o obj/aurrasd.o

bin/aurras: obj/aurras.o
	gcc -g obj/aurras.o -o bin/aurras

obj/aurras.o: src/aurras.c
	gcc -Wall -g -c src/aurras.c -o obj/aurras.o

clean:
	-rm obj/* tmp/* bin/aurras bin/aurrasd

open_server:
	./bin/aurrasd etc/aurrasd.conf bin/aurrasd-filters/

test:
	bin/aurras transform samples/sample-1-so.m4a tmp/sample-1-so.m4a lento
	bin/aurras transform samples/sample-1-so.m4a tmp/sample-1-so.m4a rapido
	bin/aurras status
