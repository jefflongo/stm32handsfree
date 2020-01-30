# Executable name
EXECUTABLE = flash

# File locations
SRCDIR = src
IDIR = include
LDIR = lib
ODIR = build

# Includes
_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

# Libraries
ifeq ($(OS),Windows_NT)
	LIBS = -lftd2xx
else
	LIBS = -lftdi1 -ludev
endif

# Object files
_OBJ = bootloader.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

# Compile flags
CC = gcc
CFLAGS = -Wall -I$(IDIR)

# Linker flags
LDFLAGS = -Wl,-rpath=$(LDIR)

# Debug flags
DBGCFLAGS = -g -O0

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(DBGCFLAGS)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -L$(LDIR) $(LIBS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR)/*.o $(EXECUTABLE).exe $(EXECUTABLE)