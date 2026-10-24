CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
TARGET   = myshell
OBJS     = command.o tokenizer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean