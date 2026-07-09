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
#include <ctime>
#include <unistd.h>

/* Erstellt einen leeren Manager; die eigentlichen ServerConfig-Objekte kommen später über setupServers(). */
ServerManager::ServerManager()
{
}

/* Räumt den Manager auf; offene Sockets werden im Loop selbst geschlossen. */
ServerManager::~ServerManager()
{
}

/* Speichert alle vom Parser erzeugten Serverblöcke, damit der Manager daraus Listener bauen kann. */
void ServerManager::setupServers(const std::vector<ServerConfig> &servers)
{
	_servers = servers;
	Logger::logMsg(CYAN, CONSOLE_OUTPUT, "Test ServerManager loaded %lu server(s)",
		static_cast<unsigned long>(_servers.size()));
}

/*
 * Startet den HTTP-Loop.
 *
 * Wichtig für "mehreren Servern zuhören":
 * Der Parser kann mehrere server{}-Blöcke laden, aber diese aktuelle Testversion
 * nimmt nur _servers[0], erstellt genau einen listen_fd und blockiert dann in accept().
 * Ein echter Mehrserver-Loop würde für jeden Server einen eigenen listen_fd anlegen
 * und alle FDs gemeinsam mit select(), poll() oder kqueue/epoll überwachen.
 */
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

/* Erstellt einen TCP-Socket, bindet ihn an den konfigurierten Port und schaltet listen() ein. */
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

/* Nimmt Clients nacheinander an; accept() blockiert hier, bis ein Client verbunden ist. */
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

/* Liest eine Anfrage, entscheidet zwischen CGI und statischer Datei und sendet die fertige Antwort. */
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

/* Liest HTTP-Header und danach Content-Length- oder chunked-Body vollständig ein. */
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
	if (hasChunkedBody(headers))
	{
		while (!isChunkedBodyComplete(raw_request))
		{
			const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
			if (n <= 0)
				return (false);
			raw_request.append(buffer, static_cast<size_t>(n));
			if (raw_request.size() > MAX_CONTENT_LENGTH)
				return (false);
		}
		return (true);
	}

	const std::string content_length = getHeaderValue(headers, "Content-Length");
	if (!content_length.empty())
		expected_body = static_cast<size_t>(std::atol(content_length.c_str()));
	while (raw_request.size() < header_end + 4 + expected_body)
	{
		const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return (false);
		raw_request.append(buffer, static_cast<size_t>(n));
	}
	return (true);
}

/* Sucht einen Header case-insensitive in einem rohen Headerblock. */
std::string ServerManager::getHeaderValue(const std::string &headers,
	const std::string &name) const
{
	size_t pos = 0;
	while (pos < headers.length())
	{
		const size_t end = headers.find("\r\n", pos);
		const std::string line = headers.substr(pos,
			(end == std::string::npos ? headers.length() : end) - pos);
		const size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string wanted = name;
			for (size_t i = 0; i < key.length(); ++i)
				key[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
			for (size_t i = 0; i < wanted.length(); ++i)
				wanted[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(wanted[i])));
			if (key == wanted)
			{
				size_t value = colon + 1;
				while (value < line.length() && std::isspace(line[value]))
					++value;
				return (line.substr(value));
			}
		}
		if (end == std::string::npos)
			break;
		pos = end + 2;
	}
	return ("");
}

/* Prüft, ob Transfer-Encoding: chunked gesetzt ist. */
bool ServerManager::hasChunkedBody(const std::string &headers) const
{
	std::string transfer_encoding = getHeaderValue(headers, "Transfer-Encoding");
	for (size_t i = 0; i < transfer_encoding.length(); ++i)
		transfer_encoding[i] = static_cast<char>(
			std::tolower(static_cast<unsigned char>(transfer_encoding[i])));
	return (transfer_encoding.find("chunked") != std::string::npos);
}

/* Erkennt das Ende eines chunked Bodys anhand des finalen 0-Chunks. */
bool ServerManager::isChunkedBodyComplete(const std::string &raw_request) const
{
	const size_t header_end = raw_request.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (false);
	const std::string body = raw_request.substr(header_end + 4);
	if (body.length() < 5)
		return (false);
	return (body.find("\r\n0\r\n\r\n") != std::string::npos
		|| body.find("\r\n0\r\n") == body.length() - 5);
}

/* Dekodiert einen Transfer-Encoding: chunked Body in den rohen CGI-Body. */
std::string ServerManager::decodeChunkedBody(const std::string &body) const
{
	std::string decoded;
	size_t pos = 0;
	while (pos < body.length())
	{
		const size_t line_end = body.find("\r\n", pos);
		if (line_end == std::string::npos)
			break;
		std::string size_text = body.substr(pos, line_end - pos);
		const size_t extension = size_text.find(';');
		if (extension != std::string::npos)
			size_text = size_text.substr(0, extension);
		const size_t chunk_size = static_cast<size_t>(fromHexToDec(size_text));
		pos = line_end + 2;
		if (chunk_size == 0)
			break;
		if (pos + chunk_size > body.length())
			break;
		decoded.append(body, pos, chunk_size);
		pos += chunk_size + 2;
	}
	return (decoded);
}

/* Zerlegt den rohen HTTP-Text in Methode, Pfad, Query, Header und Body. */
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
	{
		if (request.getHeader("transfer-encoding").find("chunked") != std::string::npos)
			request.setBody(decodeChunkedBody(raw_request.substr(header_end + 4)));
		else
			request.setBody(raw_request.substr(header_end + 4));
	}
	return (request);
}

/* Schaltet einen Descriptor auf non-blocking, damit select() die CGI-Pipes steuern kann. */
bool ServerManager::setNonBlocking(int fd) const
{
	if (fd < 0)
		return (false);
	return (::fcntl(fd, F_SETFL, O_NONBLOCK) != -1);
}

/* Bedient CGI-stdin und CGI-stdout mit select() und bricht hängende Prozesse nach Timeout ab. */
bool ServerManager::runCgiWithSelect(CgiHandler &cgi, const std::string &body,
	std::string &cgi_output) const
{
	const time_t start = std::time(NULL);
	const int timeout_seconds = 5;
	size_t written = 0;
	bool input_open = (cgi.pipe_in[1] >= 0);
	bool output_open = (cgi.pipe_out[0] >= 0);
	char buffer[4096];

	if (!setNonBlocking(cgi.pipe_in[1]) || !setNonBlocking(cgi.pipe_out[0]))
		return (false);
	while (input_open || output_open)
	{
		if (std::time(NULL) - start > timeout_seconds)
		{
			if (cgi.getCgiPid() > 0)
				::kill(cgi.getCgiPid(), SIGKILL);
			return (false);
		}

		fd_set readfds;
		fd_set writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		int max_fd = -1;
		if (output_open)
		{
			FD_SET(cgi.pipe_out[0], &readfds);
			max_fd = std::max(max_fd, cgi.pipe_out[0]);
		}
		if (input_open && written < body.length())
		{
			FD_SET(cgi.pipe_in[1], &writefds);
			max_fd = std::max(max_fd, cgi.pipe_in[1]);
		}
		else if (input_open)
		{
			::close(cgi.pipe_in[1]);
			cgi.pipe_in[1] = -1;
			input_open = false;
			continue;
		}

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		const int ready = ::select(max_fd + 1, &readfds, &writefds, NULL, &tv);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			return (false);
		}
		if (ready == 0)
			continue;

		if (input_open && FD_ISSET(cgi.pipe_in[1], &writefds))
		{
			const ssize_t n = ::write(cgi.pipe_in[1], body.c_str() + written,
				body.length() - written);
			if (n > 0)
				written += static_cast<size_t>(n);
			else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
				return (false);
		}
		if (output_open && FD_ISSET(cgi.pipe_out[0], &readfds))
		{
			const ssize_t n = ::read(cgi.pipe_out[0], buffer, sizeof(buffer));
			if (n > 0)
				cgi_output.append(buffer, static_cast<size_t>(n));
			else if (n == 0)
			{
				::close(cgi.pipe_out[0]);
				cgi.pipe_out[0] = -1;
				output_open = false;
			}
			else if (errno != EAGAIN && errno != EWOULDBLOCK)
				return (false);
		}
	}
	return (true);
}

/* Sucht eine statische Datei im Document Root und baut daraus eine HTTP-Antwort. */
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

/* Startet ein konfiguriertes CGI-Skript und verpackt dessen Ausgabe als HTTP-Antwort. */
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

	std::string cgi_output;
	if (!runCgiWithSelect(cgi, request.getBody(), cgi_output))
	{
		int status = 0;
		::waitpid(cgi.getCgiPid(), &status, 0);
		return (buildHttpResponse(504, "text/html", getErrorPage(504)));
	}

	int status = 0;
	::waitpid(cgi.getCgiPid(), &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		return (buildHttpResponse(502, "text/html", getErrorPage(502)));
	if (!cgi_output.empty() && cgi_output.find("HTTP/1.1") == 0)
		return (cgi_output);
	return (normalizeCgiOutput(cgi_output));
}

/* Wandelt CGI-Ausgabe mit Header/Body-Trennung in eine vollständige HTTP/1.1-Antwort um. */
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

/* Baut eine normale HTTP-Antwort inklusive Statuszeile, Content-Type, Content-Length und Body. */
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

/* Baut eine einfache 302-Weiterleitung mit Location-Header. */
std::string ServerManager::buildRedirectResponse(const std::string &location) const
{
	std::stringstream response;
	response << "HTTP/1.1 302 Found\r\n";
	response << "Location: " << location << "\r\n";
	response << "Content-Length: 0\r\n";
	response << "Connection: close\r\n\r\n";
	return (response.str());
}

/* Prüft, ob die HTTP-Header vollständig sind, und gibt den Header-Endmarker zurück. */
std::string ServerManager::findHeaderEnd(const std::string &raw_request) const
{
	const size_t pos = raw_request.find("\r\n\r\n");
	if (pos == std::string::npos)
		return ("");
	return (raw_request.substr(pos, 4));
}

/* Dekodiert einfache Prozentkodierung in URLs, z.B. %20 zu Leerzeichen. */
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

/* Entfernt die Query aus einem Request-Target und gibt nur den URL-Pfad zurück. */
std::string ServerManager::getPathWithoutQuery(const std::string &target) const
{
	const size_t query = target.find('?');
	if (query == std::string::npos)
		return (urlDecode(target));
	return (urlDecode(target.substr(0, query)));
}

/* Gibt den Query-String ohne führendes '?' zurück, oder einen leeren String. */
std::string ServerManager::getQueryFromTarget(const std::string &target) const
{
	const size_t query = target.find('?');
	if (query == std::string::npos)
		return ("");
	return (target.substr(query + 1));
}

/* Übersetzt einen URL-Pfad in einen Dateipfad unterhalb des konfigurierten Server-Roots. */
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

/* Prüft, ob der URL-Pfad zu einer CGI-Location passt. */
bool ServerManager::isCgiRequest(const ServerConfig &server, const std::string &url_path) const
{
	return (findCgiLocation(server, url_path) != NULL);
}

/* Findet die erste Location, deren Pfad und CGI-Endung zur Anfrage passen. */
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
			const size_t ext_pos = url_path.find(*ext);
			if (ext_pos != std::string::npos
				&& (ext_pos + ext->length() == url_path.length()
					|| url_path[ext_pos + ext->length()] == '/'))
				return (&(*it));
		}
	}
	return (NULL);
}

/* Sendet die komplette Antwort und wiederholt send(), falls nur ein Teil geschrieben wurde. */
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
