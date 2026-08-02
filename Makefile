NAME := webserv

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
CPPFLAGS := -Isrc -Isrc/cgi -Isrc/logger -Isrc/parser -Isrc/server
WEBSERV_PYTHON ?= python3

SRC := \
	src/main.cpp \
	src/utils.cpp \
	src/cgi/cgi_handler.cpp \
	src/logger/logger.cpp \
	src/parser/config_file.cpp \
	src/parser/config_parser.cpp \
	src/server/http_request.cpp \
	src/server/location.cpp \
	src/server/mime.cpp \
	src/server/server_config.cpp \
	src/server/server_manager.cpp

OBJ_DIR := obj
OBJ := $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEP := $(OBJ:.o=.d)

# Portable C++98 build used on both Linux and macOS. Header dependencies are
# generated with -MMD -MP so changing a header rebuilds the affected objects.

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

test-eval: re
	$(WEBSERV_PYTHON) tests/evaluation_suite.py

test-core: re
	$(WEBSERV_PYTHON) tests/evaluation_suite.py --section source --section config --section core --section cgi

test-bonus: re
	$(WEBSERV_PYTHON) tests/evaluation_suite.py --section bonus

test-quick: re
	$(WEBSERV_PYTHON) tests/evaluation_suite.py --quick

test-siege: re
	sh tests/siege_stress.sh

.PHONY: all clean fclean re test-eval test-core test-bonus test-quick test-siege

-include $(DEP)
