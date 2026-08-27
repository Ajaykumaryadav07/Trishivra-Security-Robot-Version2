CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

ROOT := $(CURDIR)
BUILD_DIR := $(ROOT)/build
SRC_DIR := $(ROOT)/src
INCLUDE_DIR := $(ROOT)/include
TARGET := $(BUILD_DIR)/security_robot_v2
TEST_TARGET := $(BUILD_DIR)/smoke_test

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/camera.cpp \
	$(SRC_DIR)/yolo.cpp \
	$(SRC_DIR)/depth.cpp \
	$(SRC_DIR)/slam.cpp \
	$(SRC_DIR)/navigation.cpp \
	$(SRC_DIR)/motor.cpp \
	$(SRC_DIR)/esp32.cpp \
	$(SRC_DIR)/telegram.cpp \
	$(SRC_DIR)/voice.cpp \
	$(SRC_DIR)/config_loader.cpp

TEST_SRCS := \
	$(SRC_DIR)/config_loader.cpp \
	$(ROOT)/tests/smoke_test.cpp

all: $(TARGET)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $(SRCS) -o $@

$(TEST_TARGET): $(TEST_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $(TEST_SRCS) -o $@

$(BUILD_DIR):
	mkdir -p $@

test: $(TEST_TARGET)
	$(TEST_TARGET)

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test run clean
