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



void ServerManager::setNonBlocking(int fd)
{
	int flags = ::fcntl(fd, F_GETFL, 0);
	::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void ServerManager::setupListeners()
{
	for (std::vector<ServerConfig>::const_iterator it = _servers.begin();
		it != _servers.end(); ++it)
	{
		const int port = it->getPort();
		bool already_listening = false;
		for (std::map<int, std::vector<const ServerConfig *> >::iterator lit = _listeners.begin();
			lit != _listeners.end(); ++lit)
		{
			if (!lit->second.empty() && lit->second.front()->getPort() == port)
			{
				lit->second.push_back(&(*it));
				already_listening = true;
				break;
			}
		}
		if (already_listening)
			continue;

		const int fd = createListenSocket(port);
		if (fd < 0)
			continue;
		_listeners[fd].push_back(&(*it));
	}
}


void ServerManager::eventLoop()
{
	while (true)
	{
		std::vector<struct pollfd> poll_fds;

		for (std::map<int, std::vector<const ServerConfig *> >::iterator it = _listeners.begin();
			it != _listeners.end(); ++it)
		{
			struct pollfd pfd;
			pfd.fd = it->first;
			pfd.events = POLLIN;
			pfd.revents = 0;
			poll_fds.push_back(pfd);
		}

		for (std::map<int, ClientConnection>::iterator it = _clients.begin();
			it != _clients.end(); ++it)
		{
			struct pollfd pfd;
			pfd.fd = it->first;
			pfd.events = (it->second.state == READING_REQUEST) ? POLLIN : POLLOUT;
			pfd.revents = 0;
			poll_fds.push_back(pfd);
		}

		const int ready = ::poll(&poll_fds[0], poll_fds.size(), 1000);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}

		for (size_t i = 0; i < poll_fds.size(); ++i)
		{
			const int fd = poll_fds[i].fd;
			const short revents = poll_fds[i].revents;
			if (revents == 0)
				continue;

			if (_listeners.count(fd))
			{
				if (revents & POLLIN)
					acceptNewClients(fd);
				continue;
			}

			std::map<int, ClientConnection>::iterator cit = _clients.find(fd);
			if (cit == _clients.end())
				continue;

			if (revents & (POLLHUP | POLLERR))
			{
				closeClient(fd);
				continue;
			}
			if (revents & POLLIN)
				handleClientReadable(cit->second);
			else if (revents & POLLOUT)
				handleClientWritable(cit->second);
		}
		const time_t now = std::time(NULL);
		std::vector<int> timed_out;
		for (std::map<int, ClientConnection>::iterator it = _clients.begin();
			it != _clients.end(); ++it)
		{
			if (now - it->second.last_activity > CONNECTION_TIMEOUT)
				timed_out.push_back(it->first);
		}
		for (size_t i = 0; i < timed_out.size(); ++i)
		{
			Logger::logMsg(YELLOW, CONSOLE_OUTPUT,
				"Closing client fd=%d: idle timeout (%ld seconds)",
				timed_out[i], static_cast<long>(now - _clients[timed_out[i]].last_activity));
			closeClient(timed_out[i]);
		}
	}
}

void ServerManager::acceptNewClients(int listen_fd)
{
	while (true)
	{
		const int client_fd = ::accept(listen_fd, NULL, NULL);
		if (client_fd < 0)
			break; // EAGAIN/EWOULDBLOCK: no more pending clients right now

		setNonBlocking(client_fd);

		ClientConnection client;
		client.fd = client_fd;
		client.server = _listeners[listen_fd].front();
		client.last_activity = std::time(NULL);
		_clients[client_fd] = client;
		Logger::logMsg(CYAN, CONSOLE_OUTPUT, "New client connected: fd=%d", client_fd);
	}
}

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
	setupListeners();
	Logger::logMsg(YELLOW, CONSOLE_OUTPUT, "Server running (non-blocking, multi-port)");
	eventLoop();
}

/* Erstellt einen TCP-Socket, bindet ihn an den konfigurierten Port und schaltet listen() ein. */
int ServerManager::createListenSocket(int port) const
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
	addr.sin_port = htons(static_cast<uint16_t>(port));

	if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "bind() failed on port %d: %s",
			port, std::strerror(errno));
		::close(fd);
		return (-1);
	}
	if (::listen(fd, 32) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "listen() failed: %s", std::strerror(errno));
		::close(fd);
		return (-1);
	}
	setNonBlocking(fd);
	return (fd);
}

size_t ServerManager::extractContentLength(const std::string &headers) const
{
	const std::string content_length = getHeaderValue(headers, "Content-Length");
	if (content_length.empty())
		return (0);
	return (static_cast<size_t>(std::atol(content_length.c_str())));
}

void ServerManager::handleClientReadable(ClientConnection &client)
{
	client.last_activity = std::time(NULL);
	char buffer[4096];
	const ssize_t n = ::recv(client.fd, buffer, sizeof(buffer), 0);

	if (n == 0) { closeClient(client.fd); return; }
	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		closeClient(client.fd);
		return;
	}

	client.in_buffer.append(buffer, static_cast<size_t>(n));

	const bool complete = isRequestComplete(client);

	if (client.headers_parsed
		&& client.expected_body_len > client.server->getClientMaxBodySize())
	{
		client.out_buffer = buildHttpResponse(413, "text/html", getErrorPage(413));
		client.state = WRITING_RESPONSE;
		return;
	}

	if (client.in_buffer.size() > MAX_CONTENT_LENGTH)
	{
		client.out_buffer = buildHttpResponse(413, "text/html", getErrorPage(413));
		client.state = WRITING_RESPONSE;
		return;
	}

	if (complete)
		processRequest(client);
}

bool ServerManager::isRequestComplete(ClientConnection &client) const
{
	if (!client.headers_parsed)
	{
		const size_t header_end = client.in_buffer.find("\r\n\r\n");
		if (header_end == std::string::npos)
			return (false);
		client.header_end = header_end;
		client.expected_body_len = extractContentLength(client.in_buffer.substr(0, header_end));
		client.headers_parsed = true;
	}
	return (client.in_buffer.size() >= client.header_end + 4 + client.expected_body_len);
}

void ServerManager::handleClientWritable(ClientConnection &client)
{
	const ssize_t n = ::send(client.fd, client.out_buffer.c_str() + client.bytes_sent,
		client.out_buffer.size() - client.bytes_sent, 0);

	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		closeClient(client.fd);
		return;
	}
	client.bytes_sent += static_cast<size_t>(n);
	if (client.bytes_sent >= client.out_buffer.size())
		closeClient(client.fd);
}

void ServerManager::processRequest(ClientConnection &client)
{
	HttpRequest request = parseRequest(client.in_buffer);

	if (!request.isValid())
	{
		client.out_buffer = buildHttpResponse(400, "text/html", getErrorPage(400));
		client.state = WRITING_RESPONSE;
		return;
	}

	const std::string path = request.getPath();

	if (isCgiRequest(*client.server, path))
		client.out_buffer = buildCgiResponse(*client.server, request);
	else
		client.out_buffer = buildStaticResponse(*client.server, request);

	client.state = WRITING_RESPONSE;
}

void ServerManager::closeClient(int fd)
{
	Logger::logMsg(CYAN, CONSOLE_OUTPUT, "Closing client fd=%d", fd);
	::close(fd);
	_clients.erase(fd);
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

	/* No CRLF at all means we never had a complete, well-formed request line. */
	const size_t line_end = raw_request.find("\r\n");
	if (line_end == std::string::npos)
	{
		request.setValid(false);
		return (request);
	}

	const std::string request_line = raw_request.substr(0, line_end);
	std::stringstream line(request_line);
	std::string method;
	std::string target;
	std::string version;
	std::string extra;

	/*
	 * Extraction must yield exactly three tokens (method, target, version)
	 * with nothing left over -- "GARBAGE" alone, or "GET / HTTP/1.1 junk",
	 * are both malformed, not just a request line missing a piece.
	 */
	if (!(line >> method >> target >> version) || (line >> extra))
	{
		request.setValid(false);
		return (request);
	}

	/* Only these two versions are understood; anything else is rejected. */
	if (version != "HTTP/1.1" && version != "HTTP/1.0")
	{
		request.setValid(false);
		return (request);
	}
	request.setVersion(version);

	if (method.empty() || target.empty() || target[0] != '/')
	{
		request.setValid(false);
		return (request);
	}

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
if (request.getVersion() == "HTTP/1.1" && request.getHeader("host").empty())
	{
		request.setValid(false);
		return (request);
	}

	return (request);
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

	// if (!setNonBlocking(cgi.pipe_in[1]) || !setNonBlocking(cgi.pipe_out[0]))
	// 	return (false);
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

std::string ServerManager::buildStaticResponse(const ServerConfig &server,
	const HttpRequest &request) const
{
	const Location *location = findMatchingLocation(server, request.getPath());
	if (!location)
		return (buildHttpResponse(404, "text/html", getErrorPage(404)));

	if (isPathTraversal(request.getPath()))
		return (buildHttpResponse(403, "text/html", getErrorPage(403)));
	
	if (!location->getReturn().empty())
		return (buildRedirectResponse(location->getReturn()));

	if (!location->acceptsMethod(request.getMethodStr()))
		return (buildHttpResponse(405, "text/html", getErrorPage(405)));

	if (request.getMethodStr() != "GET")
		return (buildHttpResponse(405, "text/html", getErrorPage(405)));

	std::string file_path = resolveStaticPath(*location, request.getPath());
	struct stat st;
	if (::stat(file_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		if (!file_path.empty() && file_path[file_path.length() - 1] != '/')
			file_path += "/";
		file_path += location->getIndexLocation();
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
std::string ServerManager::resolveStaticPath(const Location &location,
	const std::string &url_path) const
{
	std::string relative_path = url_path;
	const std::string &loc_path = location.getPath();

	if (relative_path.compare(0, loc_path.length(), loc_path) == 0)
		relative_path.erase(0, loc_path.length());

	while (!relative_path.empty() && relative_path[0] == '/')
		relative_path.erase(0, 1);

	if (relative_path.empty())
		relative_path = location.getIndexLocation();

	return (location.getRootLocation() + relative_path);
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

const Location *ServerManager::findMatchingLocation(const ServerConfig &server,
	const std::string &url_path) const
{
	const std::vector<Location> &locations = server.getLocations();
	const Location *best_match = NULL;
	size_t best_length = 0;

	for (std::vector<Location>::const_iterator it = locations.begin();
		it != locations.end(); ++it)
	{
		const std::string &loc_path = it->getPath();

		if (url_path.compare(0, loc_path.length(), loc_path) != 0)
			continue; // request path doesn't start with this location's path at all

		const bool exact = (url_path.length() == loc_path.length());
		const bool boundary = !exact && (loc_path[loc_path.length() - 1] == '/' || url_path[loc_path.length()] == '/');
		
		if (!exact && !boundary)
			continue; // e.g. "/cgi" matching "/cgi-bin/..." -- reject

		if (loc_path.length() > best_length)
		{
			best_length = loc_path.length();
			best_match = &(*it);
		}
	}
	return (best_match);
}

bool ServerManager::isPathTraversal(const std::string &url_path) const
{
	size_t pos = 0;
	while (pos < url_path.length())
	{
		const size_t next = url_path.find('/', pos);
		const std::string segment = url_path.substr(pos,
			(next == std::string::npos ? url_path.length() : next) - pos);
		if (segment == "..")
			return (true);
		if (next == std::string::npos)
			break;
		pos = next + 1;
	}
	return (false);
}