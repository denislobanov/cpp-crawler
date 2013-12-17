CC=clang++
CPPFLAGS=-Wall -std=c++11 -g
DEPENDENCIES=glibmm-2.4 libxml-2.0 glib-2.0
OUT_DIR="."

INCLUDES=$(shell pkg-config --cflags $(DEPENDENCIES))
LDDFLAGS=$(shell curl-config --libs) $(shell pkg-config --libs $(DEPENDENCIES))
LIBRARIES=-lboost_system -lpthread

COMMON_OBJECTS=netio.o parser.o robots_txt.o database.o cache.o memory_mgr.o
WORKER_OBJECTS=ipc_client.o crawler_worker.o
MASTER_OBJECTS=crawler_master.o
UNIT_TESTS=test_netio test_parser test_crawler_worker test_robots_txt test_cache test_database test_memory_mgr test_ipc_client

all: crawler_worker crawler_master

tests: $(UNIT_TESTS)
ifneq ($(OUT_DIR), ".")
		@echo "copying tests"
		$(foreach ut, $(UNIT_TESTS), $(shell mv $(ut) $(OUT_DIR)/$(ut)))
endif

clean:
	rm -f $(UNIT_TESTS) *.o

crawler_worker: $(COMMON_OBJECTS) $(WORKER_OBJECTS) $(LIBRARIES)
	$(CC) $(LDDFLAGS) -o $@ $(COMMON_OBJECTS)

crawler_master: $(COMMON_OBJECTS) $(MASTER_OBJECTS) $(LIBRARIES)
	$(CC) $(LDDFLAGS) -o $@ $(COMMON_OBJECTS)

$(UNIT_TESTS): $(COMMON_OBJECTS) $(WORKER_OBJECTS)
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $@.cpp
	$(CC) $(LDDFLAGS) -o $@ $@.o $(COMMON_OBJECTS) $(WORKER_OBJECTS) $(LIBRARIES)

%.o : %.cpp
	$(CC) $(CPPFLAGS) $(INCLUDES) -c $<

.PHONY: all clean tests crawler_worker crawler_master $(UNIT_TESTS)
