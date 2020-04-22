CC=gcc
CFLAGS= -g -Wall

DEPEND=ush.o

ush: $(DEPEND)
	$(CC) $(CFLAGS) -o $@ $(DEPEND)

clean:
	-rm ush ush.o 2>/dev/null || true
