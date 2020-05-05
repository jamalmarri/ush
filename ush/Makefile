CC=gcc
CFLAGS= -g -Wall

DEPEND=ush.o expand.o builtin.o strmode.o

ush: $(DEPEND)
	$(CC) $(CFLAGS) -o $@ $(DEPEND)

clean:
	-rm ush $(DEPEND) 2>/dev/null || true

ush.o expand.o builtin.o strmode.o: defn.h
