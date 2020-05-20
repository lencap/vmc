TARGET     := vmc
VERSION    := 120
SRCDIR     := src
BUILDDIR   := bld

# VirtualBox C API bindings
PATH_SDK      := bindings
ifeq ($(BUILD_PLATFORM),win)
PLATFORM_INC  := -I$(PATH_SDK)/mscom/include
PLATFORM_LIB  := $(PATH_SDK)/mscom/lib
else
PLATFORM_INC  := -I$(PATH_SDK)/xpcom/include
PLATFORM_LIB  := $(PATH_SDK)/xpcom/lib
endif
GLUE_DIR      := $(PATH_SDK)/c/glue
VBOXINC       := -I$(PATH_SDK)/c/include -I$(GLUE_DIR) $(PLATFORM_INC)

# Locate all respective source files
SOURCES    := $(shell find $(SRCDIR) -type f -name *.c)
# Designate all respective object files
OBJECTS    := $(addprefix $(BUILDDIR)/,$(notdir $(SOURCES:.c=.o)))

CC         := gcc
CFLAGS     := -Wall -std=c99 -g
LDFLAGS    := -ldl -lpthread
INC        := $(VBOXINC)

# Link
$(TARGET): $(OBJECTS) $(BUILDDIR)/VBoxCAPIGlue.o $(BUILDDIR)/VirtualBox_i.o
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LDFLAGS)

# Compile
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -o $@ -c $<

$(BUILDDIR)/VBoxCAPIGlue.o : $(GLUE_DIR)/VBoxCAPIGlue.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -o $@ -c $<

$(BUILDDIR)/VirtualBox_i.o : $(PLATFORM_LIB)/VirtualBox_i.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -o $@ -c $<

.PHONY: all release install clean

all:
	@make clean
	@make
release:
	@tar czf $(TARGET)-$(VERSION).tar.gz $(TARGET)
	@shasum -a 256 $(TARGET)-$(VERSION).tar.gz
install:
	@install $(TARGET) /usr/local/bin/
clean:
	@rm -rf $(TARGET) $(TARGET)*.gz $(BUILDDIR)
