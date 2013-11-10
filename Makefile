PROJECTS		= hello_world gl_image get_page
PARALLEL_MAKE	= -j

all: $(PROJECTS)

$(PROJECTS):
    $(MAKE) $(PARALLEL_MAKE) -C $@
