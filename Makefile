CC = gcc
SRC = src/core/*.c
TARGET = valp

all:
	$(CC) $(SRC) -o $(TARGET)

repl:
	$(RUN) ./$(TARGET)

clean:
	rm $(TARGET)

test: $(TARGET)
	for t in test/*.vp; do $(RUN) ./$(TARGET) "$$t" || exit; done

help:
	@echo
	@echo '  VALP LANGUAGE'
	@echo
	@echo Usage:
	@echo
	@echo '  make            Build valp'
	@echo '  make repl       Start a Repl'
	@echo '  make test       Run tests'
	@echo '  make clean      Clean Valp executable'
	@echo