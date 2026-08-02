#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "server_config.hpp"
# include "http_request.hpp"
# include <poll.h>
# include <fcntl.h>
# include <sys/types.h>
# include <vector>

enum ClientState
{
	READING_REQUEST,
	CGI_RUNNING,
	WRITING_RESPONSE
};

struct ClientConnection
{
	int					fd;
	int					listener_fd;
	const ServerConfig	*server;
	std::string			in_buffer;
	std::string			out_buffer;
	size_t				bytes_sent;
	ClientState			state;
	bool				headers_parsed;
	size_t				header_end;
	size_t				expected_body_len;
	time_t 				last_activity;
	pid_t				cgi_pid;
	int					cgi_stdin_fd;
	int					cgi_stdout_fd;
	std::string			cgi_input;
	std::string			cgi_output;
	size_t				cgi_bytes_written;
	time_t				cgi_started;
	bool				cgi_child_exited;
	int					cgi_exit_status;

	ClientConnection() : fd(-1), listener_fd(-1), server(NULL), bytes_sent(0),
		state(READING_REQUEST), headers_parsed(false), header_end(0),
		expected_body_len(0), last_activity(0), cgi_pid(-1),
		cgi_stdin_fd(-1), cgi_stdout_fd(-1), cgi_bytes_written(0),
		cgi_started(0), cgi_child_exited(false), cgi_exit_status(0) {}
};

class CgiHandler;

class ServerManager
{
	private:
		std::vector<ServerConfig>							_servers;
		std::map<int, std::vector<const ServerConfig *> >	_listeners; // listen_fd -> server(s) on that port
		std::map<int, ClientConnection>					_clients;    // client_fd -> state
		std::vector<pid_t>								_terminated_cgi_pids;

		void		setupListeners();
		int			createListenSocket(const std::string &host, int port) const;
		static void	setNonBlocking(int fd);
		const ServerConfig	*selectServer(int listener_fd, const std::string &host) const;

		void		eventLoop();
		void		acceptNewClients(int listen_fd);
		void		handleClientReadable(ClientConnection &client);
		void		handleClientWritable(ClientConnection &client);
		bool		isRequestComplete(ClientConnection &client) const;
		size_t		extractContentLength(const std::string &headers) const;
		bool		parseContentLength(const std::string &headers, size_t &length) const;
		void		processRequest(ClientConnection &client);
		void		closeClient(int fd);
		HttpRequest	parseRequest(const std::string &raw_request) const;
		std::string	getHeaderValue(const std::string &headers, const std::string &name) const;
		bool		hasChunkedBody(const std::string &headers) const;
		bool		isChunkedBodyComplete(const std::string &raw_request) const;
		std::string	decodeChunkedBody(const std::string &body) const;
		void		startCgiRequest(ClientConnection &client, HttpRequest &request);
		void		handleCgiInputWritable(ClientConnection &client);
		void		handleCgiOutputReadable(ClientConnection &client);
		void		updateCgiProcesses();
		void		reapCgiChildren();
		void		finishCgiRequest(ClientConnection &client);
		void		failCgiRequest(ClientConnection &client, short status);
		void		cleanupCgi(ClientConnection &client, bool terminate_child);
		std::string	buildStaticResponse(const ServerConfig &server, const HttpRequest &request) const;
		std::string	normalizeCgiOutput(const std::string &cgi_output) const;
		std::string	buildHttpResponse(short status, const std::string &content_type,
						const std::string &body) const;
		std::string	buildRedirectResponse(const std::string &location) const;
		std::string	findHeaderEnd(const std::string &raw_request) const;
		std::string	urlDecode(const std::string &value) const;
		std::string	getPathWithoutQuery(const std::string &target) const;
		std::string	getQueryFromTarget(const std::string &target) const;
		std::string resolveStaticPath(const Location &location,const std::string &url_path) const;
		bool		isCgiRequest(const ServerConfig &server, const std::string &url_path) const;
		bool 		isPathTraversal(const std::string &url_path) const;
		const Location	*findCgiLocation(const ServerConfig &server, const std::string &url_path) const;
		const Location *findMatchingLocation(const ServerConfig &server,const std::string &url_path) const;
		void		sendAll(int client_fd, const std::string &response) const;
		std::string buildAutoindexPage(const std::string &dir_path, const std::string &url_path) const;
		std::string getConfiguredErrorPage(short status, const ServerConfig &server) const;
		std::string buildUploadResponse(const ServerConfig &server,const Location &location, const HttpRequest &request) const;
		std::string buildDeleteResponse(const ServerConfig &server,const Location &location, const HttpRequest &request) const;

	public:
		ServerManager();
		~ServerManager();

		void	setupServers(const std::vector<ServerConfig> &servers);
		void	runServers();
};

#endif
