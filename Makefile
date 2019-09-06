CC=gcc
EXECUTABLE=pbar

.PHONY: default clean

default: $(EXECUTABLE)

$(EXECUTABLE): pbar.c
	$(CC) -o $@ -Wall $<

clean:
	rm -rf $(EXECUTABLE)
