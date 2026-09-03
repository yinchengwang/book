CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++20
TARGET = test
MOD = math
MOD_PCM = $(MOD).pcm
SRC = main.cpp

# GCC 14+ 支持模块；旧版本需要 -fmodules-ts
# 注意：模块支持仍在演进，跨编译器可能有问题
all: $(MOD_PCM) $(TARGET)

$(MOD_PCM): $(MOD).ixx
	$(CXX) $(CXXFLAGS) -c $(MOD).ixx -o $(MOD_PCM)

$(TARGET): $(SRC) $(MOD_PCM)
	$(CXX) $(CXXFLAGS) -fprebuilt-module-path=. $(SRC) $(MOD_PCM) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(MOD_PCM)

.PHONY: all run clean
