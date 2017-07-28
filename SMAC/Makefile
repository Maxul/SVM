CC = gcc
CFLAG = -g -Wall -O2
LDFLAG = -lpthread

BIN = $(patsubst %.c,%,$(wildcard *.c)) 

all : $(BIN)

$(BIN) : % : %.c
	$(CC) $< $(CFLAG) $(LDFLAG) -o $@

clean :
	-rm $(BIN)
	
