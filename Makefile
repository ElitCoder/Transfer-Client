NAME		:= Transfer-Client

OBJ_FOLDER	:= obj
SRC_FOLDER	:= src
BIN_FOLDER	:= bin

CPP_FILES	:= $(wildcard $(SRC_FOLDER)/*.cpp)
OBJ_FILES	:= $(addprefix $(OBJ_FOLDER)/,$(notdir $(CPP_FILES:.cpp=.o)))

CXX_FLAGS	:= -std=c++14 -Wall -Wextra -pedantic-errors
CXX_FLAGS	+= -g
#CXX_FLAGS	+= -O3

LDLIBS		:= -lpthread
TARGET		:= $(BIN_FOLDER)/$(NAME)

all: build

clean:
	rm -rf $(TARGET) $(OBJ_FOLDER)/*

build: $(OBJ_FILES)
	$(CXX) $^ -o $(TARGET) $(LDLIBS)

$(OBJ_FOLDER)/%.o: $(SRC_FOLDER)/%.cpp
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

CXX_FLAGS += -MMD
-include $(OBJFILES:.o=.d)