CFLAGS+=-g -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls `pkg-config --cflags xdamage xext`
LDFLAGS+=`pkg-config --libs xdamage xext`

PROGS=xdamage_mon

xdamage_mon: xdamage_mon.o

all: $(PROGS)

clean:
	rm -f $(PROGS) *.o
