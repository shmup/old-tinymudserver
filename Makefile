CC=g++
CCFLAGS=-g -Wall

O_FILES = tinymudserver.o

startup : $(O_FILES)
	$(CC) $(CCFLAGS) -o tinymudserver $(O_FILES)

.SUFFIXES : .o .cpp

.cpp.o :  
	$(CC) $(CCFLAGS) -c $<

clean:
	rm *.o
