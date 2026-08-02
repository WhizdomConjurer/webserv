#pragma once
#ifndef WEBSERV_HPP
# define WEBSERV_HPP

# include <iostream>
# include <fcntl.h>
# include <cstring>
# include <string> 
# include <unistd.h>
# include <dirent.h>
# include <sstream>
# include <cstdlib>
# include <fstream>
# include <cctype>
# include <ctime>
# include <cstdarg>
# include <cstdio>
# include <cerrno>
# include <new>
# include <stdint.h>

/* STL Containers */
# include <map>
# include <set>
# include <vector>
# include <algorithm>
# include <iterator>
# include <list>

/* System */
# include <sys/types.h>
# include <sys/wait.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <unistd.h>
# include <signal.h>

/* Network */
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>

# include "parser/config_parser.hpp"
# include "parser/config_file.hpp"
# include "server/server_config.hpp"
# include "server/location.hpp"
# include "server/http_request.hpp"
# include "cgi/cgi_handler.hpp"
# include "server/mime.hpp"
# include "logger/logger.hpp"


#define CONNECTION_TIMEOUT 60 // Time in seconds before client get kicked out if no data was sent.
#ifdef TESTER
    #define MESSAGE_BUFFER 40000 
#else
    #define MESSAGE_BUFFER 40000
#endif

#define MAX_URI_LENGTH 4096
#define MAX_CONTENT_LENGTH 30000000

template <typename T>
std::string toString(const T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

/* Utility helpers shared by parser, response generation, and CGI code. */

std::string statusCodeString(short);
std::string getErrorPage(short);
bool removeRegularFile(const std::string &path);
int buildHtmlIndex(std::string &, std::vector<uint8_t> &, size_t &);
int ft_stoi(std::string str);
unsigned int fromHexToDec(const std::string& nb);


#endif
