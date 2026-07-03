#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "server_config.hpp"
# include "http_request.hpp"
# include <vector>

class ServerManager
{
	private:
		std::vector<ServerConfig>	_servers;

		int			createListenSocket(const ServerConfig &server) const;
		void		serveLoop(int listen_fd, const ServerConfig &server);
		void		handleClient(int client_fd, const ServerConfig &server);
		bool		readRequest(int client_fd, std::string &raw_request) const;
		HttpRequest	parseRequest(const std::string &raw_request) const;
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
