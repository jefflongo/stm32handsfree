# File locations
SRCDIR = src
IDIR = include
LDIR = lib
ODIR = build

# Includes
_DEPS = ftd2xx.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

# Libraries
# LIBS = -lftd2xx
LIBS = -lftdi -lusb-1.0

# Object files
_OBJ = bootloader.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

# Compile flags
CC = gcc
CFLAGS = -Wall -I$(IDIR)

# Debug flags
DBGCFLAGS = -g -O0

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

all: bootload

bootload: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -L$(LDIR) $(LIBS)

.PHONY: clean

clean:
	rm -rf $(ODIR)/*.o $(ODIR)/*.exe