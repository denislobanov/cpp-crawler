CC=clang++
CPPFLAGS=-Wall -std=c++11
DEPENDENCIES=glibmm-2.4 libxml-2.0 glib-2.0

INCLUDES=$(shell pkg-config --cflags $(DEPENDENCIES))
LDDFLAGS=$(shell curl-config --libs) $(shell pkg-config --libs $(DEPENDENCIES))

COMMON_OBJECTS=netio.o parser.o robots_txt.o database.o cache.o memory_mgr.o ipc_client.o crawler_worker.o
UNIT_TESTS=test_netio test_parser test_crawler test_robots_txt test_cache test_database test_memory_mgr test_ipc_client

all: crawler

tests: $(UNIT_TESTS)

clean:
	rm -f $(UNIT_TESTS) *.o

crawler: $(COMMON_OBJECTS)
	$(CC) $(LDDFLAGS) -o $@ $(COMMON_OBJECTS)

$(UNIT_TESTS): $(COMMON_OBJECTS)
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $@.cpp
	$(CC) $(LDDFLAGS) -o $@ $@.o $(COMMON_OBJECTS)

%.o : %.cpp
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $<

.PHONY: all clean tests $(UNIT_TESTS)
