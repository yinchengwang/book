# Makefile for network_debug demo

SHELL := /bin/bash
TARGET := network_debug
SOURCES := main.sh

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	chmod +x $(SOURCES)

clean:
	@echo "Nothing to clean"

run: $(TARGET)
	./$(TARGET)
