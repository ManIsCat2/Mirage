DIRECTORIES := src src/network src/imgui
TARGET := Mirage
BUILD_DIR := build
CXX := clang++
ASAN := 0
CXXFLAGS := -O3 -Iinclude -Isrc/imgui -std=c++23 -MMD -MP
LDFLAGS := -lz

WINDOWS_BUILD := 0
LINUX_BUILD := 0
OSX_BUILD := 0

ARM_BUILD := 0
BITS_32 := 0

ifeq ($(ASAN),1)
	CXXFLAGS += -g -fsanitize=address -fsanitize=undefined
	LDFLAGS += -fsanitize=address -fsanitize=undefined
endif

# get platform
ifeq ($(OS),Windows_NT)
	WINDOWS_BUILD := 1
else ifeq ($(shell uname -s),Darwin)
	OSX_BUILD := 1
	ifeq ($(shell uname -m),arm64)
		ARM_BUILD := 1
	endif
else
	LINUX_BUILD := 1
	machine = $(shell sh -c 'uname -m 2>/dev/null || echo unknown')
	ifneq (,$(findstring aarch64,$(machine)))
		ARM_BUILD := 1
	endif

	ifneq (,$(findstring arm,$(machine)))
		ARM_BUILD := 1
		BITS_32 := 1
	endif
endif

COOPNET_LIB :=

# setup linker flags
ifeq ($(WINDOWS_BUILD),1)
	CXX := g++ # use g++ for the c++ compiler by default
	LDFLAGS += -Llib/win64 -lcoopnet -ljuice -lglfw3 -lopengl32 -lgdi32 -lws2_32 -liphlpapi -lbcrypt -static
else ifeq ($(OSX_BUILD),1)
	ifeq ($(ARM_BUILD),1)
		LDFLAGS += -Llib/mac_arm -lcoopnet -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -Wl,-rpath,@executable_path
		COOPNET_LIB += ./lib/mac_arm/libcoopnet.dylib
		COOPNET_LIB += ./lib/mac_arm/libjuice.1.6.2.dylib
	else
		LDFLAGS += -Llib/mac_intel -lcoopnet -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -Wl,-rpath,@executable_path
		COOPNET_LIB += ./lib/mac_intel/libcoopnet.dylib
		COOPNET_LIB += ./lib/mac_intel/libjuice.1.6.2.dylib
	endif
else
	ifeq ($(ARM_BUILD),1)
		ifeq ($(BITS_32),1)
			LDFLAGS += -Llib/linux/arm -lcoopnet -ljuice -lglfw -lGL -ldl -lpthread
		else
			LDFLAGS += -Llib/linux/aarch64 -lcoopnet -ljuice -lglfw -lGL -ldl -lpthread
	else
		LDFLAGS += -Llib/linux/x86_64 -lcoopnet -ljuice -lglfw -lGL -ldl -lpthread
	endif
endif

SOURCES := $(wildcard $(addsuffix /*.cpp,$(DIRECTORIES)))
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
ifeq ($(OSX_BUILD),1)
	cp $(COOPNET_LIB) $(BUILD_DIR)/
endif

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)

.PHONY: all clean
