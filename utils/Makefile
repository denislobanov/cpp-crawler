CC=clang++
CPPFLAGS=-Wall -std=c++11 -g
DEPENDENCIES=glibmm-2.4 libxml-2.0 glib-2.0
OUT_DIR="."

INCLUDES=$(shell pkg-config --cflags $(DEPENDENCIES)) -I../src/include
LDDFLAGS=$(shell curl-config --libs) $(shell pkg-config --libs $(DEPENDENCIES))
LIBRARIES=-lboost_system -lpthread -lboost_serialization

COMMON_OBJECTS=../src/netio.o ../src/robots_txt.o

UTILS=dump_page dump_robots

all: tools

tools: $(UTILS)
ifneq ($(OUT_DIR), ".")
		@echo "copying utils"
		$(foreach ut, $(UTILS), $(shell mv $(ut) $(OUT_DIR)/$(ut)))
endif

clean:
	rm -f $(UTILS) *.o

$(UTILS): $(COMMON_OBJECTS)
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $@.cpp
	$(CC) $(LDDFLAGS) -o $@ $@.o $(COMMON_OBJECTS) $(LIBRARIES)

%.o : %.cpp
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $<

.PHONY: all clean tools $(UTILS)
