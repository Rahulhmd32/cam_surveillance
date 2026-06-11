SRCDIR := src
BUILDDIR := build
TARGET := cam_surveillance_di1

CXX ?= aarch64-openwrt-linux-g++
SDK_DIR ?= /opt/di1-sdk/bsp
BACKUP_LIB_DIR ?= /opt/di1-sdk/backups

STAGING_DIR := $(SDK_DIR)/bsp4/staging_dir/target-aarch64_cortex-a53_musl
SDK_CUST := $(SDK_DIR)/bsp4/package/customization

CXXFLAGS := -O2 -std=c++17 -Wall -Wextra \
	-I$(STAGING_DIR)/usr/include \
	-I$(SDK_CUST)/di1/libdi1utils/files/di1utils/inc \
	-I$(SDK_CUST)/icat_api/icatsys/src/include \
	-I$(SDK_CUST)/libipc_core/files

LDFLAGS := -L$(BACKUP_LIB_DIR) \
	-Wl,-rpath-link,$(BACKUP_LIB_DIR) \
	-Wl,--allow-shlib-undefined

LDLIBS := -ldi1utils -licatsys -lipc_core -loswrapper -lpthread -lrt

.PHONY: all clean

all: $(BUILDDIR)/$(TARGET)

$(BUILDDIR)/$(TARGET): $(BUILDDIR)/main.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/main.o: $(SRCDIR)/main.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)
