#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "server_config.hpp"
# include "http_request.hpp"
# include <poll.h>
# include <fcntl.h>
# include <vector>

enum ClientState
{
	READING_REQUEST,
	WRITING_RESPONSE
};

struct ClientConnection
{
	int					fd;
	const ServerConfig	*server;
	std::string			in_buffer;
	std::string			out_buffer;
	size_t				bytes_sent;
	ClientState			state;
	bool				headers_parsed;
	size_t				header_end;
	size_t				expected_body_len;

	ClientConnection() : fd(-1), server(NULL), bytes_sent(0),
		state(READING_REQUEST), headers_parsed(false), header_end(0),
		expected_body_len(0) {}
};

class CgiHandler;

class ServerManager
{
	private:
		std::vector<ServerConfig>							_servers;
		std::map<int, std::vector<const ServerConfig *> >	_listeners; // listen_fd -> server(s) on that port
		std::map<int, ClientConnection>					_clients;    // client_fd -> state

		void		setupListeners();
		int			createListenSocket(int port) const;
		static void	setNonBlocking(int fd);

		void		eventLoop();
		void		acceptNewClients(int listen_fd);
		void		handleClientReadable(ClientConnection &client);
		void		handleClientWritable(ClientConnection &client);
		bool		isRequestComplete(ClientConnection &client) const;
		size_t		extractContentLength(const std::string &headers) const;
		void		processRequest(ClientConnection &client);
		void		closeClient(int fd);
		HttpRequest	parseRequest(const std::string &raw_request) const;
		std::string	getHeaderValue(const std::string &headers, const std::string &name) const;
		bool		hasChunkedBody(const std::string &headers) const;
		bool		isChunkedBodyComplete(const std::string &raw_request) const;
		std::string	decodeChunkedBody(const std::string &body) const;
		//void		setNonBlocking(int fd) const;
		bool		runCgiWithSelect(CgiHandler &cgi, const std::string &body,
						std::string &cgi_output) const;
		std::string	buildStaticResponse(const ServerConfig &server, const HttpRequest &request) const;
		std::string	buildCgiResponse(const ServerConfig &server, HttpRequest &request) const;
		std::string	normalizeCgiOutput(const std::string &cgi_output) const;
		std::string	buildHttpResponse(short status, const std::string &content_type,
						const std::string &body) const;
		std::string	buildRedirectResponse(const std::string &location) const;
		std::string	findHeaderEnd(const std::string &raw_request) const;
		std::string	urlDecode(const std::string &value) const;
		std::string	getPathWithoutQuery(const std::string &target) const;
		std::string	getQueryFromTarget(const std::string &target) const;
		std::string	resolveStaticPath(const ServerConfig &server, const std::string &url_path) const;
		bool		isCgiRequest(const ServerConfig &server, const std::string &url_path) const;
		const Location	*findCgiLocation(const ServerConfig &server, const std::string &url_path) const;
		void		sendAll(int client_fd, const std::string &response) const;

	public:
		ServerManager();
		~ServerManager();

		void	setupServers(const std::vector<ServerConfig> &servers);
		void	runServers();
};

#endif
