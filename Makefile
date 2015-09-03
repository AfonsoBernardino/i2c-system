SRCDIR = src
OBJDIR = obj
BINDIR = bin
CC     = gcc
CFLAGS = -Wall

sources = tool.c ads7828.c ad5694.c mcp23009.c mpl115.c tmp75.c sht21.c
objects = $(patsubst %.c, %.o, $(sources))

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully."

$(BINDIR)/tool : $(addprefix $(OBJDIR)/, $(objects))
	$(CC)  $^ -o $@ 
	@echo "Linking complete."

.PHONY : clean

clean : 
	rm -f *.o *~ $(OBJDIR)/*.o $(SRCDIR)/*~ $(BINDIR)/tool
	rm -fr bin
	rm -fr obj


