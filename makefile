CC =gcc
FLAGS=-Wall -Wextra -pedantic -g --std=c17
MAIN=parser

SOURCES=$(shell find -type f -name '*.c')
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))

.PHONY: all clean recompile

all: $(MAIN)

$(MAIN): $(OBJECTS)
	@echo "Compiling..."
	$(CC) $(FLAGS) $^ -o $@
	@echo "Done!"

recompile:
	@echo "Recompiling..."
	rm -rf $(MAIN) *.o *.gch
	@make all
	@echo "Done!"

clean:
	@echo "Removing files"
	rm -rf $(MAIN) *.o *.gch
	@echo "Done!"