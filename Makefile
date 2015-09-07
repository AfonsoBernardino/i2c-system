SRCDIR = src
OBJDIR = obj
BINDIR = bin
CC     = gcc
CFLAGS = -Wall

TOOLSRC = tool.c ads7828.c ad5694.c mcp23009.c mpl115.c tmp75.c sht21.c
TOOLOBJ = $(patsubst %.c, %.o, $(TOOLSRC))

HVSRC = hv.c ad5694.c mcp23009.c
HVOBJ = $(patsubst %.c, %.o, $(HVSRC))

PRECSRC = dac7578.c
PRECOBJ = $(patsubst %.c, %.o, $(PRECSRC))

all: tool hv prec

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully."

tool : $(addprefix $(OBJDIR)/, $(TOOLOBJ))
	@$(CC)  $^ -o $(BINDIR)/$@ 
	@echo "Linking "$@" complete."

hv : $(addprefix $(OBJDIR)/, $(HVOBJ))
	@$(CC)  $^ -o $(BINDIR)/$@ 
	@echo "Linking "$@" complete."

prec : $(addprefix $(OBJDIR)/, $(PRECOBJ))
	@$(CC)  $^ -o $(BINDIR)/$@ 
	@echo "Linking "$@" complete."

.PHONY : all

clean : 
	@rm -f *.o *~ $(OBJDIR)/*.o $(SRCDIR)/*~ 
	@rm -r $(BINDIR)/tool
	@rm -f $(BINDIR)/hv
	@rm -f $(BINDIR)/prec
	@rm -fr bin
	@rm -fr obj


