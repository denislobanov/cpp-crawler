SRC_DIR			= $(shell pwd)/src
BUILD_DIR		= $(shell pwd)/build

all: 
	mkdir -p $(BUILD_DIR)/crawler
	$(MAKE) OUT_DIR="$(BUILD_DIR)/crawler" -C $(SRC_DIR)

tests:
	mkdir -p $(BUILD_DIR)/tests
	$(MAKE) OUT_DIR="$(BUILD_DIR)/tests" -C $(SRC_DIR) tests

clean:
	rm -rf build
	$(MAKE) -C $(SRC_DIR) clean
