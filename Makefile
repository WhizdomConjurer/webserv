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

# Subject / integration checklist
# ✅ Makefile exists and defines NAME, all, clean, fclean, re.
# ✅ Build uses c++ with -Wall -Wextra -Werror and -std=c++98.
# ✅ Current CGI files have been split correctly into .hpp declarations and .cpp definitions.
# ✅ Logger, parser, main, utils, and CGI functions now have function-level comments.
# ✅ CGI handler no longer shallow-copies owned char ** arrays.
# ✅ CGI handler closes owned pipe descriptors during cleanup.
# ✅ Obvious include-path typos from ../inc/... were corrected for the current tree.
# ✅ Dummy server modules exist for ServerManager, ServerConfig, Location, HttpRequest, and Mime.
# ✅ These dummy modules let parser/logger/CGI integration compile and run basic smoke tests.
# ⚠️ Dummy modules are placeholders and must be replaced by the real team implementations.
# ⚠️ Real ServerManager must create sockets, set O_NONBLOCK, and drive all socket/pipe I/O with one poll-equivalent loop.
# ⚠️ Real HttpRequest must parse raw HTTP bytes, headers, body, chunked input, malformed requests, and timeouts.
# ⚠️ Real ServerConfig/Location must validate the complete subject configuration behavior.
# ⚠️ Real Mime must provide the final MIME table used by static responses.

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

.PHONY: all clean fclean re test-eval test-core test-bonus test-quick

-include $(DEP)
