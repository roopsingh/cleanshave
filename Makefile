SYSTYPE:=$(shell uname)
local_base=/usr/local
#CC = gcc
cc = $(CC)
CFLAGS = -ggdb -O0
INCLUDE = -I. -I$(local_base)/include
LDFLAGS = -L. -L$(local_base)/lib
SRCC = cleanshave.c
TARGET = cleanshave

all: $(TARGET)

$(TARGET): $(SRCC)
  $(cc) $(SRCC) $(CFLAGS) $(INCLUDE) $(LDFLAGS) $(LIB) -o $(TARGET)

clean:
	@rm -f $(TARGET)
