SRCDIR = src
OBJDIR = obj
BINDIR = bin
CC = gcc
CFLAGS=-Wall
VPATH = src

objects = tool.o ads7828.o ad5694.o mcp23009.o mpl115.o tmp75.o sht21.o

%.o : %.c %.h
		$(CC) $(CFLAGS) -c $< -o $@

tool : $(objects)
		gcc  $(objects) -o $(BINDIR)/tool 


##prec : dac7578.c
##	gcc dac7578.c -o prec

.PHONY : clean

clean : 
	rm -f *~ $(OBJDIR)/*.o $(SRCDIR)/*~ $(BINDIR)/tool
