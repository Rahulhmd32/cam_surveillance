SRCDIR := src
BUILDDIR := build
TARGET := cam_surveillance_di1
SRCS := $(SRCDIR)/main.cpp $(SRCDIR)/camera.cpp $(SRCDIR)/cloud_vms_client.cpp $(SRCDIR)/ws_util.cpp
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
CXX ?= aarch64-linux-gnu-g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -Iinclude
LDLIBS := -lpthread
.PHONY: all clean
all: $(BUILDDIR)/$(TARGET)
$(BUILDDIR)/$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -rf $(BUILDDIR)
