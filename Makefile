CC = gcc
LDLIBS = -pthread

DCFLAGS = -Wall -Wextra -Wpedantic \
-std=c99 -Wformat=2 -Wformat-overflow -Wformat-truncation -O2 -Wconversion -D_FORTIFY_SOURCE=2 -fstack-clash-protection -fstack-protector-strong -fstack-protector-all -Wdouble-promotion -pipe -fcf-protection -fpie -pedantic-errors -m64 -g3

CFLAGS = -O2 -std=c99 -m64 -pipe -fcf-protection -fstack-protector-strong -fstack-protector-all -D_FORTIFY_SOURCE=2 -fpie
LDFLAGS = -s -pie -Wl,-z,now -Wl,-z,relro -Wl,-z,defs

.PHONY: all clean debug
all: mycel
mycel: mycel.o
        $(CC) $(LDFLAGS) mycel.o $(LDLIBS) -o mycel
mycel.o: mycel.c
        $(CC) $(CFLAGS) $(LDLIBS) -c mycel.c -o mycel.o
clean:
        -rm -rf *.o *.s mycel mycel_debug
debug: mycel_debug
mycel_debug: mycel.c
        $(CC) $(DCFLAGS) mycel.c $(LDLIBS) -pie -o mycel_debug
