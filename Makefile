SUBSYSTEMS		= crawler
PARALLEL_MAKE	= -j
SRC_DIR			= $(shell pwd)/src
BUILD_DIR		= $(shell pwd)/build
BOOST_DIR		= $(BUILD_DIR)/boost

all: $(SUBSYSTEMS)

$(SUBSYSTEMS): boost
	echo "Building $@"
	$(foreach dir, $(SUBSYSTEMS), mkdir -p $(BUILD_DIR)/$(dir))
	$(MAKE) $(PARALLEL_MAKE) -C $(SRC_DIR)/$@

boost:
	echo "Building Boost"
	mkdir $(BOOST_DIR)
	cd $(SRC_DIR)/boost/boost
	./bootstrap.sh --prefix=$(BUILD_DIR)/boost --with-libraries=system
	./b2 install
	echo "Done."

clean:
	rm -rf build
	$(foreach comp, $(SUBSYSTEMS), $(MAKE) $(PARALLEL_MAKE) -C $(SRC_DIR)/$(comp) clean)
