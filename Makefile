CC = gcc
CFLAGS = -std=c99 -Wall
SRC = src/core/*.c
TYPES = src/core/types/*.c
TARGET = valp
LIBS = -ledit


all:
	$(CC) $(CFLAGS) $(SRC) $(TYPES) $(LIBS) -o $(TARGET)

d_print:
	$(CC) $(CFLAGS) $(SRC) $(TYPES) $(LIBS) -D DEBUG_PRINT_CODE -o $(TARGET)

d_trace:
	$(CC) $(CFLAGS) $(SRC) $(TYPES) $(LIBS) -D DEBUG_TRACE_EXECUTION -o $(TARGET)

repl:
	$(RUN) ./$(TARGET)

clean:
	rm $(TARGET)

test: $(TARGET)
	for t in test/core/*.vp; do $(RUN) ./$(TARGET) "$$t"; done
	for t in test/core/types/*.vp; do $(RUN) ./$(TARGET) "$$t"; done

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
	@echo Debug:
	@echo
	@echo '  make d_print    Compile with DEBUG_PRINT_CODE flag'
	@echo '  make d_trace    Compile with DEBUG_TRACE_EXECUTION flag'
	@echo