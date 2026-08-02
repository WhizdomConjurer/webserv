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
#include <netdb.h>
#include <limits>
#include <sstream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <ctime>
#include <unistd.h>



void ServerManager::setNonBlocking(int fd)
{
	::fcntl(fd, F_SETFL, O_NONBLOCK);
}

void ServerManager::setupListeners()
{
	for (std::vector<ServerConfig>::const_iterator it = _servers.begin();
		it != _servers.end(); ++it)
	{
		const int port = it->getPort();
		const std::string &host = it->getHostString();
		bool already_listening = false;
		for (std::map<int, std::vector<const ServerConfig *> >::iterator lit = _listeners.begin();
			lit != _listeners.end(); ++lit)
		{
			if (!lit->second.empty() && lit->second.front()->getPort() == port
				&& lit->second.front()->getHostString() == host)
			{
				lit->second.push_back(&(*it));
				already_listening = true;
				break;
			}
		}
		if (already_listening)
			continue;

		const int fd = createListenSocket(host, port);
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
			pfd.events = 0;
			if (it->second.state == READING_REQUEST)
				pfd.events = POLLIN;
			else if (it->second.state == WRITING_RESPONSE)
				pfd.events = POLLOUT;
			pfd.revents = 0;
			poll_fds.push_back(pfd);

			if (it->second.state == CGI_RUNNING && it->second.cgi_stdin_fd >= 0)
			{
				pfd.fd = it->second.cgi_stdin_fd;
				pfd.events = POLLOUT;
				pfd.revents = 0;
				poll_fds.push_back(pfd);
			}
			if (it->second.state == CGI_RUNNING && it->second.cgi_stdout_fd >= 0)
			{
				pfd.fd = it->second.cgi_stdout_fd;
				pfd.events = POLLIN;
				pfd.revents = 0;
				poll_fds.push_back(pfd);
			}
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
			if (cit != _clients.end())
			{
				if (revents & (POLLERR | POLLNVAL))
				{
					closeClient(fd);
					continue;
				}
				if ((revents & POLLIN) && cit->second.state == READING_REQUEST)
					handleClientReadable(cit->second);
				cit = _clients.find(fd);
				if (cit == _clients.end())
					continue;
				if ((revents & POLLOUT) && cit->second.state == WRITING_RESPONSE)
					handleClientWritable(cit->second);
				cit = _clients.find(fd);
				if (cit != _clients.end() && (revents & POLLHUP)
					&& cit->second.state == READING_REQUEST)
					closeClient(fd);
				continue;
			}

			ClientConnection *cgi_client = NULL;
			bool is_cgi_input = false;
			for (std::map<int, ClientConnection>::iterator it = _clients.begin();
				it != _clients.end(); ++it)
			{
				if (it->second.cgi_stdin_fd == fd)
				{
					cgi_client = &it->second;
					is_cgi_input = true;
					break;
				}
				if (it->second.cgi_stdout_fd == fd)
				{
					cgi_client = &it->second;
					break;
				}
			}
			if (!cgi_client)
				continue;
			if (is_cgi_input)
			{
				if (revents & POLLOUT)
					handleCgiInputWritable(*cgi_client);
				if (cgi_client->state == CGI_RUNNING
					&& (revents & (POLLHUP | POLLERR | POLLNVAL)))
					failCgiRequest(*cgi_client, 502);
			}
			else
			{
				if (revents & (POLLIN | POLLHUP))
					handleCgiOutputReadable(*cgi_client);
				if (cgi_client->state == CGI_RUNNING
					&& (revents & (POLLERR | POLLNVAL)))
					failCgiRequest(*cgi_client, 502);
			}
		}
		updateCgiProcesses();
		reapCgiChildren();
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
		client.listener_fd = listen_fd;
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
 * setupListeners() registriert alle konfigurierten Ports. eventLoop() überwacht
 * anschließend Listener, Clients und CGI-Pipes gemeinsam mit genau einem poll().
 */
void ServerManager::runServers()
{
	if (_servers.empty())
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "No server config loaded");
		return;
	}
	setupListeners();
	if (_listeners.empty())
		throw std::runtime_error("No listening socket could be created");
	Logger::logMsg(YELLOW, CONSOLE_OUTPUT, "Server running (non-blocking, multi-port)");
	eventLoop();
}

/* Erstellt einen TCP-Socket, bindet ihn an den konfigurierten Port und schaltet listen() ein. */
int ServerManager::createListenSocket(const std::string &host, int port) const
{
	struct addrinfo hints;
	struct addrinfo *addresses = NULL;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	const std::string port_text = toString(port);
	if (::getaddrinfo(host.empty() ? NULL : host.c_str(), port_text.c_str(),
		&hints, &addresses) != 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "Cannot resolve listen address %s:%d",
			host.c_str(), port);
		return (-1);
	}

	int fd = -1;
	for (struct addrinfo *it = addresses; it != NULL; it = it->ai_next)
	{
		fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (fd < 0)
			continue;
		const int enable = 1;
		::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
		if (::bind(fd, it->ai_addr, it->ai_addrlen) == 0)
			break;
		::close(fd);
		fd = -1;
	}
	::freeaddrinfo(addresses);
	if (fd < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "bind() failed on %s:%d: %s",
			host.c_str(), port, std::strerror(errno));
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

/* Wählt innerhalb eines gemeinsamen Interface:Port-Listeners den passenden Servernamen. */
const ServerConfig *ServerManager::selectServer(int listener_fd,
	const std::string &host_header) const
{
	std::map<int, std::vector<const ServerConfig *> >::const_iterator found
		= _listeners.find(listener_fd);
	if (found == _listeners.end() || found->second.empty())
		return (NULL);
	std::string host = host_header;
	const size_t colon = host.find(':');
	if (colon != std::string::npos)
		host.erase(colon);
	for (size_t i = 0; i < host.length(); ++i)
		host[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(host[i])));
	for (size_t i = 0; i < found->second.size(); ++i)
	{
		std::string name = found->second[i]->getServerName();
		for (size_t j = 0; j < name.length(); ++j)
			name[j] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[j])));
		if (!name.empty() && name == host)
			return (found->second[i]);
	}
	return (found->second.front());
}

size_t ServerManager::extractContentLength(const std::string &headers) const
{
	const std::string content_length = getHeaderValue(headers, "Content-Length");
	if (content_length.empty())
		return (0);
	return (static_cast<size_t>(std::atol(content_length.c_str())));
}

/* Parst Content-Length strikt dezimal und schützt size_t vor Überlauf. */
bool ServerManager::parseContentLength(const std::string &headers, size_t &length) const
{
	const std::string value = getHeaderValue(headers, "Content-Length");
	length = 0;
	if (value.empty())
		return (true);
	for (size_t i = 0; i < value.length(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			return (false);
		const size_t digit = static_cast<size_t>(value[i] - '0');
		if (length > (std::numeric_limits<size_t>::max() - digit) / 10)
			return (false);
		length = length * 10 + digit;
	}
	return (true);
}

void ServerManager::handleClientReadable(ClientConnection &client)
{
	client.last_activity = std::time(NULL);
	char buffer[4096];
	const ssize_t n = ::recv(client.fd, buffer, sizeof(buffer), 0);

	if (n <= 0)
	{
		closeClient(client.fd);
		return;
	}

	client.in_buffer.append(buffer, static_cast<size_t>(n));

	const bool complete = isRequestComplete(client);

	if (client.headers_parsed
		&& client.expected_body_len > client.server->getClientMaxBodySize())
	{
		client.out_buffer = buildHttpResponse(413, "text/html", getConfiguredErrorPage(413, *client.server));
		client.state = WRITING_RESPONSE;
		return;
	}

	if (client.in_buffer.size() > MAX_CONTENT_LENGTH)
	{
		client.out_buffer = buildHttpResponse(413, "text/html", getConfiguredErrorPage(413, *client.server));
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
		const std::string headers = client.in_buffer.substr(0, header_end);
		client.server = selectServer(client.listener_fd,
			getHeaderValue(headers, "Host"));
		if (!parseContentLength(headers, client.expected_body_len))
		{
			client.headers_parsed = true;
			return (true);
		}
		client.headers_parsed = true;
		if (hasChunkedBody(headers))
			return (isChunkedBodyComplete(client.in_buffer));
	}
	if (hasChunkedBody(client.in_buffer.substr(0, client.header_end)))
		return (isChunkedBodyComplete(client.in_buffer));
	return (client.in_buffer.size() >= client.header_end + 4 + client.expected_body_len);
}

void ServerManager::handleClientWritable(ClientConnection &client)
{
	const ssize_t n = ::send(client.fd, client.out_buffer.c_str() + client.bytes_sent,
		client.out_buffer.size() - client.bytes_sent, 0);

	if (n <= 0)
	{
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
		short status = 400;
		if (request.getVersion().compare(0, 5, "HTTP/") == 0
			&& request.getVersion() != "HTTP/1.0"
			&& request.getVersion() != "HTTP/1.1")
			status = 505;
		client.out_buffer = buildHttpResponse(status, "text/html",
			getConfiguredErrorPage(status, *client.server));
		client.state = WRITING_RESPONSE;
		return;
	}

	const std::string path = request.getPath();

	if (isCgiRequest(*client.server, path))
	{
		startCgiRequest(client, request);
		return;
	}
	else
		client.out_buffer = buildStaticResponse(*client.server, request);

	client.state = WRITING_RESPONSE;
}

void ServerManager::closeClient(int fd)
{
	Logger::logMsg(CYAN, CONSOLE_OUTPUT, "Closing client fd=%d", fd);
	std::map<int, ClientConnection>::iterator it = _clients.find(fd);
	if (it != _clients.end())
		cleanupCgi(it->second, true);
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

	request.setVersion(version);
	/* Only these two versions are understood; anything else is rejected. */
	if (version != "HTTP/1.1" && version != "HTTP/1.0")
	{
		request.setValid(false);
		return (request);
	}

	if (method.empty() || target.empty() || target[0] != '/')
	{
		request.setValid(false);
		return (request);
	}

	request.setMethodStr(method);
	request.setPath(getPathWithoutQuery(target));
	request.setQuery(getQueryFromTarget(target));

	size_t pos = line_end + 2;
	std::set<std::string> seen_headers;
	while (pos < raw_request.length())
	{
		const size_t next = raw_request.find("\r\n", pos);
		if (next == std::string::npos || next == pos)
			break;
		const std::string header = raw_request.substr(pos, next - pos);
		const size_t colon = header.find(':');
		if (colon == std::string::npos || colon == 0)
		{
			request.setValid(false);
			return (request);
		}
		std::string name = header.substr(0, colon);
		for (size_t i = 0; i < name.length(); ++i)
		{
			if (std::isspace(static_cast<unsigned char>(name[i])))
			{
				request.setValid(false);
				return (request);
			}
			name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
		}
		if (seen_headers.count(name))
		{
			request.setValid(false);
			return (request);
		}
		seen_headers.insert(name);
		size_t value = colon + 1;
		while (value < header.length()
			&& std::isspace(static_cast<unsigned char>(header[value])))
			++value;
		request.setHeader(name, header.substr(value));
		pos = next + 2;
	}
	if (raw_request.find("\r\n\r\n") == std::string::npos)
	{
		request.setValid(false);
		return (request);
	}

	size_t declared_length = 0;
	if (!parseContentLength(raw_request.substr(0, raw_request.find("\r\n\r\n")),
		declared_length))
	{
		request.setValid(false);
		return (request);
	}
	std::string transfer_encoding = request.getHeader("transfer-encoding");
	for (size_t i = 0; i < transfer_encoding.length(); ++i)
		transfer_encoding[i] = static_cast<char>(
			std::tolower(static_cast<unsigned char>(transfer_encoding[i])));
	const bool chunked = (transfer_encoding.find("chunked") != std::string::npos);
	if (chunked && seen_headers.count("content-length"))
	{
		request.setValid(false);
		return (request);
	}

	const size_t header_end = raw_request.find("\r\n\r\n");
	if (header_end != std::string::npos)
	{
		if (chunked)
		{
			request.setBody(decodeChunkedBody(raw_request.substr(header_end + 4)));
			request.setHeader("content-length", toString(request.getBody().length()));
		}
		else
			request.setBody(raw_request.substr(header_end + 4, declared_length));
	}
	if (request.getVersion() == "HTTP/1.1" && request.getHeader("host").empty())
	{
		request.setValid(false);
		return (request);
	}

	return (request);
}

std::string ServerManager::buildUploadResponse(const ServerConfig &server,
	const Location &location, const HttpRequest &request) const
{
	if (!location.getUploadEnabled())
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	std::string filename = request.getPath();
	const size_t last_slash = filename.find_last_of('/');
	if (last_slash != std::string::npos)
		filename = filename.substr(last_slash + 1);

	if (filename.empty() || isPathTraversal(request.getPath()))
		return (buildHttpResponse(400, "text/html", getConfiguredErrorPage(400, server)));

	std::string upload_dir = location.getUploadPath();
	if (!upload_dir.empty() && upload_dir[upload_dir.length() - 1] != '/')
		upload_dir += "/";

	const std::string full_path = upload_dir + filename;

	std::ofstream file(full_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file)
		return (buildHttpResponse(500, "text/html", getConfiguredErrorPage(500, server)));

	file << request.getBody();
	file.close();

	if (file.fail())
		return (buildHttpResponse(500, "text/html", getConfiguredErrorPage(500, server)));

	return (buildHttpResponse(201, "text/plain", "Upload successful: " + filename + "\n"));
}

std::string ServerManager::buildDeleteResponse(const ServerConfig &server,
	const Location &location, const HttpRequest &request) const
{
	if (isPathTraversal(request.getPath()))
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	const std::string file_path = resolveStaticPath(location, request.getPath());
	struct stat st;

	if (::stat(file_path.c_str(), &st) != 0)
		return (buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server)));

	if (!S_ISREG(st.st_mode))
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	if (!removeRegularFile(file_path))
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	return (buildHttpResponse(204, "text/plain", ""));
}

std::string ServerManager::buildStaticResponse(const ServerConfig &server,
	const HttpRequest &request) const
{
	const Location *location = findMatchingLocation(server, request.getPath());
	if (!location)
		return (buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server)));

	if (isPathTraversal(request.getPath()))
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	if (!location->getReturn().empty())
		return (buildRedirectResponse(location->getReturn()));

	if (!location->acceptsMethod(request.getMethodStr()))
		return (buildHttpResponse(405, "text/html", getConfiguredErrorPage(405, server)));

	if (request.getMethodStr() == "POST")
		return (buildUploadResponse(server, *location, request));

	if (request.getMethodStr() == "DELETE")
		return (buildDeleteResponse(server, *location, request));

	if (request.getMethodStr() != "GET")
		return (buildHttpResponse(405, "text/html", getConfiguredErrorPage(405, server)));

	std::string file_path = resolveStaticPath(*location, request.getPath());
	struct stat st;

	if (::stat(file_path.c_str(), &st) != 0)
	{
		if (errno == EACCES)
			return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));
		return (buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server)));
	}

	if (S_ISDIR(st.st_mode))
	{
		std::string dir_path = file_path;
		if (!dir_path.empty() && dir_path[dir_path.length() - 1] != '/')
			dir_path += "/";

		const std::string index_path = dir_path + location->getIndexLocation();
		struct stat index_st;
		if (::stat(index_path.c_str(), &index_st) == 0 && S_ISREG(index_st.st_mode))
			file_path = index_path;
		else if (location->getAutoindex())
		{
			const std::string listing = buildAutoindexPage(dir_path, request.getPath());
			if (listing.empty())
				return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));
			return (buildHttpResponse(200, "text/html", listing));
		}
		else
			return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));
	}

	else if (!S_ISREG(st.st_mode))
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	if (::access(file_path.c_str(), R_OK) != 0)
		return (buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server)));

	std::ifstream file(file_path.c_str(), std::ios::in | std::ios::binary);
	if (!file)
		return (buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server)));

	std::stringstream body;
	body << file.rdbuf();
	return (buildHttpResponse(200, Mime::getType(file_path), body.str()));
}

std::string ServerManager::buildAutoindexPage(const std::string &dir_path,
	const std::string &url_path) const
{
	DIR *dir = ::opendir(dir_path.c_str());
	if (!dir)
		return ("");

	std::stringstream html;
	html << "<html><head><title>Index of " << url_path << "</title></head>\n";
	html << "<body><h1>Index of " << url_path << "</h1><ul>\n";

	struct dirent *entry;
	while ((entry = ::readdir(dir)) != NULL)
	{
		const std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;
		html << "<li><a href=\"" << name << "\">" << name << "</a></li>\n";
	}
	::closedir(dir);

	html << "</ul></body></html>\n";
	return (html.str());
}

std::string ServerManager::getConfiguredErrorPage(short status, const ServerConfig &server) const
{
	const std::map<short, std::string> &pages = server.getErrorPages();
	std::map<short, std::string>::const_iterator it = pages.find(status);

	if (it != pages.end())
	{
		std::string custom_path = it->second;
		while (!custom_path.empty() && custom_path[0] == '/')
			custom_path.erase(0, 1);

		std::ifstream file((server.getRoot() + custom_path).c_str(),
			std::ios::in | std::ios::binary);
		if (file)
		{
			std::stringstream body;
			body << file.rdbuf();
			return (body.str());
		}
	}
	return (getErrorPage(status));
}

/* Startet CGI und übergibt seine Pipes an die zentrale poll()-State-Machine. */
void ServerManager::startCgiRequest(ClientConnection &client, HttpRequest &request)
{
	const ServerConfig &server = *client.server;
	const Location *location = findCgiLocation(server, request.getPath());
	if (!location)
	{
		client.out_buffer = buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server));
		client.state = WRITING_RESPONSE;
		return;
	}
	if (!location->acceptsMethod(request.getMethodStr()))
	{
		client.out_buffer = buildHttpResponse(405, "text/html", getConfiguredErrorPage(405, server));
		client.state = WRITING_RESPONSE;
		return;
	}

	std::string script_path = request.getPath();
	size_t script_end = std::string::npos;
	for (std::vector<std::string>::const_iterator ext = location->getCgiExtension().begin();
		ext != location->getCgiExtension().end(); ++ext)
	{
		const size_t pos = script_path.find(*ext);
		if (pos != std::string::npos
			&& (pos + ext->length() == script_path.length()
				|| script_path[pos + ext->length()] == '/'))
		{
			script_end = pos + ext->length();
			break;
		}
	}
	if (script_end == std::string::npos)
	{
		client.out_buffer = buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server));
		client.state = WRITING_RESPONSE;
		return;
	}
	script_path = resolveStaticPath(*location, script_path.substr(0, script_end));
	struct stat script_stat;
	if (::stat(script_path.c_str(), &script_stat) != 0 || !S_ISREG(script_stat.st_mode))
	{
		client.out_buffer = buildHttpResponse(404, "text/html", getConfiguredErrorPage(404, server));
		client.state = WRITING_RESPONSE;
		return;
	}
	if (::access(script_path.c_str(), R_OK) != 0)
	{
		client.out_buffer = buildHttpResponse(403, "text/html", getConfiguredErrorPage(403, server));
		client.state = WRITING_RESPONSE;
		return;
	}

	CgiHandler cgi(script_path);
	short error = 0;
	std::vector<Location> temp_locations;
	temp_locations.push_back(*location);
	std::vector<Location>::iterator it = temp_locations.begin();
	cgi.initEnv(request, it);
	cgi.execute(error);
	if (error)
	{
		client.out_buffer = buildHttpResponse(error, "text/html", getConfiguredErrorPage(error, server));
		client.state = WRITING_RESPONSE;
		return;
	}

	client.cgi_pid = cgi.getCgiPid();
	client.cgi_stdin_fd = cgi.pipe_in[1];
	client.cgi_stdout_fd = cgi.pipe_out[0];
	cgi.pipe_in[1] = -1;
	cgi.pipe_out[0] = -1;
	setNonBlocking(client.cgi_stdin_fd);
	setNonBlocking(client.cgi_stdout_fd);
	client.cgi_input = request.getBody();
	client.cgi_output.clear();
	client.cgi_bytes_written = 0;
	client.cgi_started = std::time(NULL);
	client.cgi_child_exited = false;
	client.cgi_exit_status = 0;
	client.state = CGI_RUNNING;
	client.last_activity = client.cgi_started;
	if (client.cgi_input.empty())
	{
		::close(client.cgi_stdin_fd);
		client.cgi_stdin_fd = -1;
	}
}

/* Schreibt den Request-Body nur dann, wenn poll() die CGI-stdin-Pipe freigibt. */
void ServerManager::handleCgiInputWritable(ClientConnection &client)
{
	const ssize_t n = ::write(client.cgi_stdin_fd,
		client.cgi_input.c_str() + client.cgi_bytes_written,
		client.cgi_input.length() - client.cgi_bytes_written);
	if (n <= 0)
	{
		failCgiRequest(client, 502);
		return;
	}
	client.cgi_bytes_written += static_cast<size_t>(n);
	client.last_activity = std::time(NULL);
	if (client.cgi_bytes_written == client.cgi_input.length())
	{
		::close(client.cgi_stdin_fd);
		client.cgi_stdin_fd = -1;
	}
}

/* Liest CGI-Ausgabe non-blocking; EOF schließt nur diese Pipe, nicht den Client. */
void ServerManager::handleCgiOutputReadable(ClientConnection &client)
{
	char buffer[4096];
	const ssize_t n = ::read(client.cgi_stdout_fd, buffer, sizeof(buffer));
	if (n > 0)
	{
		client.cgi_output.append(buffer, static_cast<size_t>(n));
		client.last_activity = std::time(NULL);
		return;
	}
	if (n == 0)
	{
		::close(client.cgi_stdout_fd);
		client.cgi_stdout_fd = -1;
		return;
	}
	failCgiRequest(client, 502);
}

/* Prüft Prozessende und Timeout ohne auf einen laufenden CGI-Prozess zu warten. */
void ServerManager::updateCgiProcesses()
{
	const time_t now = std::time(NULL);
	for (std::map<int, ClientConnection>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		ClientConnection &client = it->second;
		if (client.state != CGI_RUNNING)
			continue;
		if (now - client.cgi_started > 5)
		{
			failCgiRequest(client, 504);
			continue;
		}
		if (!client.cgi_child_exited)
		{
			int status = 0;
			const pid_t result = ::waitpid(client.cgi_pid, &status, WNOHANG);
			if (result == client.cgi_pid)
			{
				client.cgi_child_exited = true;
				client.cgi_exit_status = status;
				client.cgi_pid = -1;
			}
			else if (result < 0)
			{
				failCgiRequest(client, 502);
				continue;
			}
		}
		if (client.cgi_child_exited && client.cgi_stdout_fd < 0)
			finishCgiRequest(client);
	}
}

/* Erntet beendete CGI-Kinder ausschließlich mit non-blocking waitpid(). */
void ServerManager::reapCgiChildren()
{
	for (std::vector<pid_t>::iterator it = _terminated_cgi_pids.begin();
		it != _terminated_cgi_pids.end(); )
	{
		int status = 0;
		const pid_t result = ::waitpid(*it, &status, WNOHANG);
		if (result == 0)
			++it;
		else
			it = _terminated_cgi_pids.erase(it);
	}
}

/* Baut erst nach Prozessende und vollständig gelesener stdout-Pipe die HTTP-Antwort. */
void ServerManager::finishCgiRequest(ClientConnection &client)
{
	if (!WIFEXITED(client.cgi_exit_status) || WEXITSTATUS(client.cgi_exit_status) != 0)
		client.out_buffer = buildHttpResponse(502, "text/html",
			getConfiguredErrorPage(502, *client.server));
	else if (!client.cgi_output.empty() && client.cgi_output.find("HTTP/1.1") == 0)
		client.out_buffer = client.cgi_output;
	else
		client.out_buffer = normalizeCgiOutput(client.cgi_output);
	cleanupCgi(client, false);
	client.state = WRITING_RESPONSE;
	client.last_activity = std::time(NULL);
}

/* Beendet eine fehlerhafte oder abgelaufene CGI-Anfrage und liefert 502/504. */
void ServerManager::failCgiRequest(ClientConnection &client, short status)
{
	cleanupCgi(client, true);
	client.out_buffer = buildHttpResponse(status, "text/html",
		getConfiguredErrorPage(status, *client.server));
	client.state = WRITING_RESPONSE;
	client.last_activity = std::time(NULL);
}

/* Schließt alle CGI-FDs; bei Clientabbruch/Timeout wird auch das Kind beendet und geerntet. */
void ServerManager::cleanupCgi(ClientConnection &client, bool terminate_child)
{
	if (client.cgi_stdin_fd >= 0)
		::close(client.cgi_stdin_fd);
	if (client.cgi_stdout_fd >= 0)
		::close(client.cgi_stdout_fd);
	client.cgi_stdin_fd = -1;
	client.cgi_stdout_fd = -1;
	if (client.cgi_pid > 0)
	{
		if (terminate_child)
			::kill(client.cgi_pid, SIGKILL);
		int status = 0;
		if (::waitpid(client.cgi_pid, &status, WNOHANG) == 0)
			_terminated_cgi_pids.push_back(client.cgi_pid);
		client.cgi_pid = -1;
	}
	client.cgi_input.clear();
	client.cgi_output.clear();
	client.cgi_bytes_written = 0;
	client.cgi_child_exited = false;
	client.cgi_exit_status = 0;
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
