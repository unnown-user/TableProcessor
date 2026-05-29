all: spreadsheet

spreadsheet: main.o spreadsheet.o formula.o csv.o
	gcc -o spreadsheet main.o spreadsheet.o formula.o csv.o -lm

main.o: main.c spreadsheet.h
	gcc -c main.c -o main.o

spreadsheet.o: spreadsheet.c spreadsheet.h formula.h eval.h csv.h
	gcc -c spreadsheet.c -o spreadsheet.o

formula.o: formula.c formula.h eval.h
	gcc -c formula.c -o formula.o

csv.o: csv.c csv.h spreadsheet.h
	gcc -c csv.c -o csv.o

clean:
	rm -f main.o spreadsheet.o formula.o csv.o spreadsheet
