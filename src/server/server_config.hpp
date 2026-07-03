#ifndef SERVER_CONFIG_HPP
# define SERVER_CONFIG_HPP

# include "location.hpp"
# include <map>
# include <string>
# include <vector>

class ServerConfig
{
	private:
		std::string						_server_name;
		std::string						_root;
		std::string						_index;
		std::string						_host_string;
		int								_host;
		int								_port;
		size_t							_client_max_body_size;
		bool							_autoindex;
		std::map<short, std::string>		_error_pages;
		std::vector<Location>			_locations;

		std::string	cleanToken(const std::string &value) const;

	public:
		ServerConfig();
		~ServerConfig();

		void	setPort(const std::string &port);
		void	setHost(const std::string &host);
		void	setRoot(const std::string &root);
		void	setIndex(const std::string &index);
		void	setServerName(const std::string &name);
		void	setClientMaxBodySize(const std::string &size);
		void	setAutoindex(const std::string &value);
		void	setLocation(const std::string &path, const std::vector<std::string> &tokens);
		void	setErrorPages(const std::vector<std::string> &tokens);

		const std::string					&getServerName() const;
		int									getHost() const;
		const std::string					&getHostString() const;
		const std::string					&getRoot() const;
		const std::string					&getIndex() const;
		int									getPort() const;
		size_t								getClientMaxBodySize() const;
		const std::map<short, std::string>	&getErrorPages() const;
		const std::vector<Location>			&getLocations() const;
		std::vector<Location>				&getLocations();

		bool	checkLocaitons() const;
		bool	isValidErrorPages() const;
};

#endif
