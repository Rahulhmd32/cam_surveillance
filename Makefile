SRCDIR := src
BUILDDIR := build
TARGET := cam_surveillance_di1
SRCS := $(SRCDIR)/main.cpp $(SRCDIR)/camera.cpp $(SRCDIR)/cloud_vms_client.cpp $(SRCDIR)/ws_util.cpp
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
CXX = aarch64-openwrt-linux-g++
SDK_DIR ?= /opt/di1-sdk/bsp
BACKUP_LIB_DIR ?= /opt/di1-sdk/backups
SDK_CUST := $(SDK_DIR)/bsp4/package/customization
DI1UTILS_INC := $(SDK_CUST)/di1/libdi1utils/files/di1utils/inc
K3000_LIB := $(SDK_CUST)/di1/k3000-driver/files/user/lib
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -Iinclude -I$(DI1UTILS_INC)
LDFLAGS := -L$(BACKUP_LIB_DIR) -L$(K3000_LIB) \
	-Wl,-rpath-link,$(BACKUP_LIB_DIR) -Wl,-rpath-link,$(K3000_LIB) \
	-Wl,--allow-shlib-undefined
LDLIBS := -ldi1utils -licatcv -licat_api -licatsys -lipc_core -ldrm \
	-lOpenVG -ldmprm -ldmpnative -loswrapper -ldmpallocator \
	-lpthread -lrt
.PHONY: all clean
all: $(BUILDDIR)/$(TARGET)
$(BUILDDIR)/$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -rf $(BUILDDIR)
