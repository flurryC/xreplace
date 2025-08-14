COMPILER := g++
CFLAGS   := -std=c++17
TARGET   := bin/xreplace
OBJ      := bin/main.o

$(TARGET): $(OBJ)
	$(COMPILER) $(CFLAGS) $^ -o $@

$(OBJ): src/main.cpp
	$(COMPILER) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(TARGET)

.PHONY: clean