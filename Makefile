SRC_DIR			= $(shell pwd)/src
UTIL_DIR		= $(SRC_DIR)/utils
BUILD_DIR		= $(shell pwd)/build

all: 
	mkdir -p $(BUILD_DIR)/crawler
	$(MAKE) OUT_DIR="$(BUILD_DIR)/crawler" -C $(SRC_DIR)

tests:
	mkdir -p $(BUILD_DIR)/tests
	$(MAKE) -j OUT_DIR="$(BUILD_DIR)/tests" -C $(SRC_DIR) tests

utils: tests
	mkdir $(BUILD_DIR)/utils
	$(MAKE) -j OUT_DIR="$(BUILD_DIR)/utils" -C $(UTIL_DIR) utils

clean:
	rm -rf build
	$(MAKE) -C $(SRC_DIR) clean
	$(MAKE) -C $(UTIL_DIR) clean
