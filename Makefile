SRCDIR = src
OBJDIR = obj
BINDIR = bin
CC     = gcc
CFLAGS = -Wall

##objects = tool.o ads7828.o ad5694.o mcp23009.o mpl115.o tmp75.o sht21.o

sources = tool.c ads7828.c ad5694.c mcp23009.c mpl115.c tmp75.c sht21.c
objects = $(patsubst %.c, %.o, $(sources))

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully."

$(BINDIR)/tool : $(addprefix $(OBJDIR)/, $(objects))
	$(CC)  $^ -o $@ 
	@echo "Linking complete."

##prec : dac7578.c
##	gcc dac7578.c -o prec

.PHONY : clean

clean : 
	rm -f *.o *~ $(OBJDIR)/*.o $(SRCDIR)/*~ $(BINDIR)/tool


