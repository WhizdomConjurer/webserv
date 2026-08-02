#ifndef CGI_HANDLER_HPP
# define CGI_HANDLER_HPP

# include "../webserv.hpp"

class HttpRequest;
class Location;

class CgiHandler
{
	private:
		std::map<std::string, std::string>	_env;
		char								**_ch_env;
		char								**_argv;
		int									_exit_status;
		std::string							_cgi_path;
		pid_t								_cgi_pid;

		void	buildEnvArray();
		void	buildArgvArray(const std::string &exec_path);
		void	freeEnvArray();
		void	freeArgvArray();
		char	*duplicateString(const std::string &value) const;
		void	closeFd(int &fd);
		void	addRequestHeaders(HttpRequest &req);

	public:
		int	pipe_in[2];
		int	pipe_out[2];

		CgiHandler();
		explicit CgiHandler(const std::string &path);
		CgiHandler(const CgiHandler &other);
		~CgiHandler();
		CgiHandler &operator=(const CgiHandler &rhs);

		void	initEnv(HttpRequest &req, const std::vector<Location>::iterator it_loc);
		void	initEnvCgi(HttpRequest &req, const std::vector<Location>::iterator it_loc);
		void	execute(short &error_code);
		void	clear();

		void	setCgiPid(pid_t cgi_pid);
		void	setCgiPath(const std::string &cgi_path);

		const std::map<std::string, std::string>	&getEnv() const;
		pid_t									getCgiPid() const;
		const std::string						&getCgiPath() const;

		int			findStart(const std::string &path, const std::string &delim) const;
		std::string	decode(std::string &path) const;
		std::string	getPathInfo(const std::string &path,
						const std::vector<std::string> &extensions) const;
};

#endif
