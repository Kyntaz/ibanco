CFLAGS = -pthread -g -Wall -pedantic

i-banco: commandlinereader.o contas.o i-banco.o
	gcc -pthread -o i-banco commandlinereader.o contas.o i-banco.o

i-banco.o: i-banco.c commandlinereader.h contas.h
	gcc $(CFLAGS) -c i-banco.c

contas.o: contas.c contas.h
	gcc $(CFLAGS) -c contas.c

commandlinereader.o: commandlinereader.c commandlinereader.h
	gcc $(CFLAGS) -c commandlinereader.c

clean:
	rm -f *.o i-banco

zip:
	zip i-banco.zip i-banco.c contas.c contas.h commandlinereader.c commandlinereader.h Makefile
