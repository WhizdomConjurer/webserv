#include "server_manager.hpp"
#include "mime.hpp"
#include "../cgi/cgi_handler.hpp"
#include "../logger/logger.hpp"
#include "../webserv.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* Creates an empty test manager; this version is only for local CGI browser tests. */
ServerManager::ServerManager()
{
}

/* Destroys the test manager; sockets are closed inside runServers()/serveLoop(). */
ServerManager::~ServerManager()
{
}

/* Stores parsed server configs so the lightweight test server can use them. */
void ServerManager::setupServers(const std::vector<ServerConfig> &servers)
{
	_servers = servers;
	Logger::logMsg(CYAN, CONSOLE_OUTPUT, "Test ServerManager loaded %lu server(s)",
		static_cast<unsigned long>(_servers.size()));
}

/* Starts one small blocking HTTP server for local static-file and CGI testing. */
void ServerManager::runServers()
{
	if (_servers.empty())
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "No server config loaded");
		return;
	}

	const ServerConfig &server = _servers[0];
	const int listen_fd = createListenSocket(server);
	if (listen_fd < 0)
		return;

	Logger::logMsg(YELLOW, CONSOLE_OUTPUT,
		"Test server listening on http://localhost:%d (blocking CGI smoke-test server)",
		server.getPort());
	serveLoop(listen_fd, server);
}

/* Creates, binds, and listens on one TCP socket for the configured port. */
int ServerManager::createListenSocket(const ServerConfig &server) const
{
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "socket() failed: %s", std::strerror(errno));
		return (-1);
	}

	const int enable = 1;
	::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(static_cast<uint16_t>(server.getPort()));

	if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "bind() failed on port %d: %s",
			server.getPort(), std::strerror(errno));
		::close(fd);
		return (-1);
	}
	if (::listen(fd, 32) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "listen() failed: %s", std::strerror(errno));
		::close(fd);
		return (-1);
	}
	return (fd);
}

/* Accepts clients one by one; this is intentionally simple and blocking for testing. */
void ServerManager::serveLoop(int listen_fd, const ServerConfig &server)
{
	while (true)
	{
		const int client_fd = ::accept(listen_fd, NULL, NULL);
		if (client_fd < 0)
		{
			if (errno == EINTR)
				continue;
			Logger::logMsg(RED, CONSOLE_OUTPUT, "accept() failed: %s", std::strerror(errno));
			break;
		}
		handleClient(client_fd, server);
		::close(client_fd);
	}
	::close(listen_fd);
}

/* Reads one request, dispatches to CGI or static handling, and sends the response. */
void ServerManager::handleClient(int client_fd, const ServerConfig &server)
{
	std::string raw_request;
	if (!readRequest(client_fd, raw_request))
	{
		sendAll(client_fd, buildHttpResponse(400, "text/html", getErrorPage(400)));
		return;
	}

	HttpRequest request = parseRequest(raw_request);
	const std::string path = request.getPath();
	std::string response;
	if (isCgiRequest(server, path))
		response = buildCgiResponse(server, request);
	else
		response = buildStaticResponse(server, request);
	sendAll(client_fd, response);
}

/* Reads headers and the optional Content-Length body for a single HTTP request. */
bool ServerManager::readRequest(int client_fd, std::string &raw_request) const
{
	char buffer[4096];
	size_t expected_body = 0;

	while (raw_request.find("\r\n\r\n") == std::string::npos)
	{
		const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return (false);
		raw_request.append(buffer, static_cast<size_t>(n));
		if (raw_request.size() > MAX_CONTENT_LENGTH)
			return (false);
	}

	const size_t header_end = raw_request.find("\r\n\r\n");
	const std::string headers = raw_request.substr(0, header_end);
	const std::string key = "Content-Length:";
	const size_t len_pos = headers.find(key);
	if (len_pos != std::string::npos)
	{
		size_t value_pos = len_pos + key.length();
		while (value_pos < headers.length() && std::isspace(headers[value_pos]))
			++value_pos;
		expected_body = static_cast<size_t>(std::atol(headers.c_str() + value_pos));
	}

	while (raw_request.size() < header_end + 4 + expected_body)
	{
		const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return (false);
		raw_request.append(buffer, static_cast<size_t>(n));
	}
	return (true);
}

/* Parses the request line, headers, query string, and body into HttpRequest. */
HttpRequest ServerManager::parseRequest(const std::string &raw_request) const
{
	HttpRequest request;
	const size_t line_end = raw_request.find("\r\n");
	const std::string request_line = raw_request.substr(0, line_end);
	std::stringstream line(request_line);
	std::string method;
	std::string target;
	std::string version;

	line >> method >> target >> version;
	(void)version;
	request.setMethodStr(method);
	request.setPath(getPathWithoutQuery(target));
	request.setQuery(getQueryFromTarget(target));

	size_t pos = line_end + 2;
	while (pos < raw_request.length())
	{
		const size_t next = raw_request.find("\r\n", pos);
		if (next == std::string::npos || next == pos)
			break;
		const std::string header = raw_request.substr(pos, next - pos);
		const size_t colon = header.find(':');
		if (colon != std::string::npos)
		{
			size_t value = colon + 1;
			while (value < header.length() && std::isspace(header[value]))
				++value;
			request.setHeader(header.substr(0, colon), header.substr(value));
		}
		pos = next + 2;
	}

	const size_t header_end = raw_request.find("\r\n\r\n");
	if (header_end != std::string::npos)
		request.setBody(raw_request.substr(header_end + 4));
	return (request);
}

/* Builds a static-file response from the configured document root. */
std::string ServerManager::buildStaticResponse(const ServerConfig &server,
	const HttpRequest &request) const
{
	if (request.getMethodStr() != "GET")
		return (buildHttpResponse(405, "text/html", getErrorPage(405)));

	std::string file_path = resolveStaticPath(server, request.getPath());
	struct stat st;
	if (::stat(file_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (!file_path.empty() && file_path[file_path.length() - 1] != '/')
			file_path += "/";
		file_path += server.getIndex();
	}

	std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
	if (!file)
		return (buildHttpResponse(404, "text/html", getErrorPage(404)));

	std::stringstream body;
	body << file.rdbuf();
	return (buildHttpResponse(200, Mime::getType(file_path), body.str()));
}

/* Executes a configured Python CGI through CgiHandler and wraps its output as HTTP. */
std::string ServerManager::buildCgiResponse(const ServerConfig &server, HttpRequest &request) const
{
	const Location *location = findCgiLocation(server, request.getPath());
	if (!location)
		return (buildHttpResponse(404, "text/html", getErrorPage(404)));
	if (!location->acceptsMethod(request.getMethodStr()))
		return (buildHttpResponse(405, "text/html", getErrorPage(405)));

	std::string script_path = request.getPath();
	if (script_path.find("/cgi-bin/") == 0)
		script_path = script_path.substr(1);

	CgiHandler cgi(script_path);
	short error = 0;
	std::vector<Location> temp_locations;
	temp_locations.push_back(*location);
	std::vector<Location>::iterator it = temp_locations.begin();
	cgi.initEnv(request, it);
	cgi.execute(error);
	if (error)
		return (buildHttpResponse(error, "text/html", getErrorPage(error)));

	if (!request.getBody().empty())
		::write(cgi.pipe_in[1], request.getBody().c_str(), request.getBody().length());
	::close(cgi.pipe_in[1]);
	cgi.pipe_in[1] = -1;

	std::string cgi_output;
	char buffer[4096];
	ssize_t n;
	while ((n = ::read(cgi.pipe_out[0], buffer, sizeof(buffer))) > 0)
		cgi_output.append(buffer, static_cast<size_t>(n));
	::close(cgi.pipe_out[0]);
	cgi.pipe_out[0] = -1;

	int status = 0;
	::waitpid(cgi.getCgiPid(), &status, 0);
	if (!cgi_output.empty() && cgi_output.find("HTTP/1.1") == 0)
		return (cgi_output);
	return (normalizeCgiOutput(cgi_output));
}

/* Converts CGI headers/body into a complete HTTP/1.1 response for browser testing. */
std::string ServerManager::normalizeCgiOutput(const std::string &cgi_output) const
{
	std::string normalized = cgi_output;
	size_t pos = normalized.find("\r\n");
	while (pos != std::string::npos)
	{
		normalized.replace(pos, 2, "\n");
		pos = normalized.find("\r\n");
	}

	const size_t header_end = normalized.find("\n\n");
	if (header_end == std::string::npos)
		return (buildHttpResponse(200, "text/plain", normalized));

	const std::string raw_headers = normalized.substr(0, header_end);
	const std::string body = normalized.substr(header_end + 2);
	std::stringstream input(raw_headers);
	std::string line;
	short status = 200;
	std::string headers;
	bool has_content_length = false;

	while (std::getline(input, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (line.empty())
			continue;
		if (line.find("Status:") == 0)
		{
			size_t value = 7;
			while (value < line.length() && std::isspace(line[value]))
				++value;
			status = static_cast<short>(std::atoi(line.c_str() + value));
		}
		else
		{
			if (line.find("Content-Length:") == 0 || line.find("content-length:") == 0)
				has_content_length = true;
			headers += line + "\r\n";
		}
	}
	if (!has_content_length)
		headers += "Content-Length: " + toString(body.length()) + "\r\n";
	headers += "Connection: close\r\n";

	std::stringstream response;
	response << "HTTP/1.1 " << status << " " << statusCodeString(status) << "\r\n";
	response << headers << "\r\n" << body;
	return (response.str());
}

/* Builds a normal HTTP response with Content-Length and Connection close. */
std::string ServerManager::buildHttpResponse(short status, const std::string &content_type,
	const std::string &body) const
{
	std::stringstream response;
	response << "HTTP/1.1 " << status << " " << statusCodeString(status) << "\r\n";
	response << "Content-Type: " << content_type << "\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n\r\n";
	response << body;
	return (response.str());
}

/* Builds a simple 302 redirect response for future route testing. */
std::string ServerManager::buildRedirectResponse(const std::string &location) const
{
	std::stringstream response;
	response << "HTTP/1.1 302 Found\r\n";
	response << "Location: " << location << "\r\n";
	response << "Content-Length: 0\r\n";
	response << "Connection: close\r\n\r\n";
	return (response.str());
}

/* Returns a marker string when HTTP headers are complete; kept for future real parser. */
std::string ServerManager::findHeaderEnd(const std::string &raw_request) const
{
	const size_t pos = raw_request.find("\r\n\r\n");
	if (pos == std::string::npos)
		return ("");
	return (raw_request.substr(pos, 4));
}

/* Decodes minimal percent-encoding for static and CGI URL paths. */
std::string ServerManager::urlDecode(const std::string &value) const
{
	std::string decoded = value;
	size_t pos = decoded.find('%');
	while (pos != std::string::npos && pos + 2 < decoded.length())
	{
		const std::string hex = decoded.substr(pos + 1, 2);
		const char ch = static_cast<char>(fromHexToDec(hex));
		decoded.replace(pos, 3, std::string(1, ch));
		pos = decoded.find('%', pos + 1);
	}
	return (decoded);
}

/* Extracts the path part from a request target. */
std::string ServerManager::getPathWithoutQuery(const std::string &target) const
{
	const size_t query = target.find('?');
	if (query == std::string::npos)
		return (urlDecode(target));
	return (urlDecode(target.substr(0, query)));
}

/* Extracts the query part from a request target without the leading question mark. */
std::string ServerManager::getQueryFromTarget(const std::string &target) const
{
	const size_t query = target.find('?');
	if (query == std::string::npos)
		return ("");
	return (target.substr(query + 1));
}

/* Maps a URL path to a static path under the server root. */
std::string ServerManager::resolveStaticPath(const ServerConfig &server,
	const std::string &url_path) const
{
	std::string clean_path = url_path;
	while (!clean_path.empty() && clean_path[0] == '/')
		clean_path.erase(0, 1);
	if (clean_path.empty())
		clean_path = server.getIndex();
	return (server.getRoot() + clean_path);
}

/* Returns true when the URL should be handled by the configured CGI location. */
bool ServerManager::isCgiRequest(const ServerConfig &server, const std::string &url_path) const
{
	return (findCgiLocation(server, url_path) != NULL);
}

/* Finds the first CGI location matching the URL path and extension. */
const Location *ServerManager::findCgiLocation(const ServerConfig &server,
	const std::string &url_path) const
{
	const std::vector<Location> &locations = server.getLocations();
	for (std::vector<Location>::const_iterator it = locations.begin(); it != locations.end(); ++it)
	{
		if (it->getCgiExtension().empty())
			continue;
		if (url_path.find(it->getPath()) != 0)
			continue;
		for (std::vector<std::string>::const_iterator ext = it->getCgiExtension().begin();
			ext != it->getCgiExtension().end(); ++ext)
		{
			if (url_path.length() >= ext->length()
				&& url_path.rfind(*ext) != std::string::npos)
				return (&(*it));
		}
	}
	return (NULL);
}

/* Sends the complete response buffer, retrying short writes. */
void ServerManager::sendAll(int client_fd, const std::string &response) const
{
	size_t sent = 0;
	while (sent < response.length())
	{
		const ssize_t n = ::send(client_fd, response.c_str() + sent,
			response.length() - sent, 0);
		if (n <= 0)
			return;
		sent += static_cast<size_t>(n);
	}
}
