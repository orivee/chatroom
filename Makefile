# set compiler and compile options
TARGET = server client
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread $(shell mysql_config --cflags)
LDFLAGS = -pthread
LDLIBS = $(shell mysql_config --libs)

# set a list of directories
# INCDIR = include
BUILDDIR := build
BINDIR := bin
SRCDIR := src

# set the include folder where the .h files reside
CFLAGS += -I$(SRCDIR)

SRCEXT := c
SRV_SOURCES := $(shell find $(SRCDIR)/server -type f -name "*.$(SRCEXT)")
SRV_OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SRV_SOURCES:.$(SRCEXT)=.o))
CLI_SOURCES := $(shell find $(SRCDIR)/client -type f -name "*.$(SRCEXT)")
CLI_OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(CLI_SOURCES:.$(SRCEXT)=.o))

all: $(TARGET)

server: $(SRV_OBJECTS)
	@echo " Linking ..."
	@echo " $(CC) $(LDFLAGS) $^ -o $(BINDIR)/server $(LDLIBS)"; $(CC) $(LDFLAGS) $^ -o $(BINDIR)/server $(LDLIBS)

$(BUILDDIR)/server/%.o: $(SRCDIR)/server/%.c
	@mkdir -p $(BUILDDIR)/server
	@echo " $(CC) $(CFLAGS) -c -o $@ $<"; $(CC) $(CFLAGS) -c -o $@ $<

client: $(CLI_OBJECTS)
	@echo " Linking ..."
	@echo " $(CC) $(LDFLAGS) $^ -o $(BINDIR)/client"; $(CC) $(LDFLAGS) $^ -o $(BINDIR)/client

$(BUILDDIR)/client/%.o: $(SRCDIR)/client/%.c
	@mkdir -p $(BUILDDIR)/client
	@echo " $(CC) $(CFLAGS) -c -o $@ $<"; $(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo " Cleaning ...";
	@echo " $(RM) -r $(BUILDDIR) $(foreach exec,$(TARGET),$(BINDIR)/$(exec))"; $(RM) -r $(BUILDDIR) $(foreach exec, $(TARGET), $(BINDIR)/$(exec))

.PHONY: clean
