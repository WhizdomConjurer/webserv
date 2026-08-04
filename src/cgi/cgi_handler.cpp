#include "cgi_handler.hpp"

/* Initializes an empty CGI handler with closed pipes and no child process. */
CgiHandler::CgiHandler()
	: _ch_env(NULL),
	  _argv(NULL),
	  _exit_status(0),
	  _cgi_path(""),
	  _cgi_pid(-1)
{
	pipe_in[0] = -1;
	pipe_in[1] = -1;
	pipe_out[0] = -1;
	pipe_out[1] = -1;
}

/* Initializes the CGI handler directly with the script path to be executed later. */
CgiHandler::CgiHandler(const std::string &path)
	: _ch_env(NULL),
	  _argv(NULL),
	  _exit_status(0),
	  _cgi_path(path),
	  _cgi_pid(-1)
{
	pipe_in[0] = -1;
	pipe_in[1] = -1;
	pipe_out[0] = -1;
	pipe_out[1] = -1;
}

/* Frees argv/env storage and closes all pipes that are still open. */
CgiHandler::~CgiHandler()
{
	clear();
}

/* Copies only value data; argv/env are rebuilt per request to avoid double-free. */
CgiHandler::CgiHandler(const CgiHandler &other)
	: _env(other._env),
	  _ch_env(NULL),
	  _argv(NULL),
	  _exit_status(other._exit_status),
	  _cgi_path(other._cgi_path),
	  _cgi_pid(other._cgi_pid)
{
	pipe_in[0] = -1;
	pipe_in[1] = -1;
	pipe_out[0] = -1;
	pipe_out[1] = -1;
}

/* Assigns safe value data and cleans up owned argv/env resources beforehand. */
CgiHandler &CgiHandler::operator=(const CgiHandler &rhs)
{
	if (this != &rhs)
	{
		clear();
		_env = rhs._env;
		_exit_status = rhs._exit_status;
		_cgi_path = rhs._cgi_path;
		_cgi_pid = rhs._cgi_pid;
	}
	return (*this);
}

/* Stores the CGI child process PID so the caller can use waitpid() later. */
void CgiHandler::setCgiPid(pid_t cgi_pid)
{
	_cgi_pid = cgi_pid;
}

/* Stores the absolute or relative script path passed to CGI. */
void CgiHandler::setCgiPath(const std::string &cgi_path)
{
	_cgi_path = cgi_path;
}

/* Returns the CGI environment as a map before it is converted to char** for execve(). */
const std::map<std::string, std::string> &CgiHandler::getEnv() const
{
	return (_env);
}

/* Returns the current CGI child process PID, or -1 if none is active. */
pid_t CgiHandler::getCgiPid() const
{
	return (_cgi_pid);
}

/* Returns the script path of this CGI request. */
const std::string &CgiHandler::getCgiPath() const
{
	return (_cgi_path);
}

/* Creates a C++-managed, null-terminated copy for execve() arrays. */
char *CgiHandler::duplicateString(const std::string &value) const
{
	char *copy = new char[value.length() + 1];
	std::memcpy(copy, value.c_str(), value.length() + 1);
	return (copy);
}

/* Converts the environment map into a NULL-terminated char** as expected by execve(). */
void CgiHandler::buildEnvArray()
{
	freeEnvArray();
	_ch_env = new char *[_env.size() + 1];
	for (size_t i = 0; i <= _env.size(); ++i)
		_ch_env[i] = NULL;

	size_t index = 0;
	for (std::map<std::string, std::string>::const_iterator it = _env.begin();
		it != _env.end(); ++it)
	{
		const std::string entry = it->first + "=" + it->second;
		_ch_env[index] = duplicateString(entry);
		++index;
	}
}

/* Builds the NULL-terminated argv array: argv[0] is the interpreter, argv[1] is the script. */
void CgiHandler::buildArgvArray(const std::string &exec_path)
{
	freeArgvArray();
	_argv = new char *[3];
	_argv[0] = NULL;
	_argv[1] = NULL;
	_argv[2] = NULL;
	_argv[0] = duplicateString(exec_path);
	_argv[1] = duplicateString(_cgi_path);
}

/* Frees the environment array currently built for execve(). */
void CgiHandler::freeEnvArray()
{
	if (!_ch_env)
		return;
	for (size_t i = 0; _ch_env[i]; ++i)
		delete [] _ch_env[i];
	delete [] _ch_env;
	_ch_env = NULL;
}

/* Frees the argv array currently built for execve(). */
void CgiHandler::freeArgvArray()
{
	if (!_argv)
		return;
	for (size_t i = 0; _argv[i]; ++i)
		delete [] _argv[i];
	delete [] _argv;
	_argv = NULL;
}

/* Closes a file descriptor if it is open and marks it as -1 afterwards. */
void CgiHandler::closeFd(int &fd)
{
	if (fd >= 0)
		::close(fd);
	fd = -1;
}

/* Converts HTTP headers into CGI variables, e.g. user-agent -> HTTP_USER_AGENT. */
void CgiHandler::addRequestHeaders(HttpRequest &req)
{
	const std::map<std::string, std::string> headers = req.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		it != headers.end(); ++it)
	{
		std::string name = it->first;
		for (std::string::iterator ch = name.begin(); ch != name.end(); ++ch)
		{
			if (*ch == '-')
				*ch = '_';
			else
				*ch = static_cast<char>(std::toupper(static_cast<unsigned char>(*ch)));
		}
		_env["HTTP_" + name] = it->second;
	}
}

/* Builds a simple CGI environment from request data and location CGI settings. */
void CgiHandler::initEnvCgi(HttpRequest &req, const std::vector<Location>::iterator it_loc)
{
	if (_cgi_path.empty())
		return;

	const std::string cgi_exec = "cgi-bin/" + it_loc->getCgiPath()[0];

	if (req.getMethod() == POST)
	{
		_env["CONTENT_LENGTH"] = toString(req.getBody().length());
		_env["CONTENT_TYPE"] = req.getHeader("content-type");
	}

	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SCRIPT_NAME"] = cgi_exec;
	_env["SCRIPT_FILENAME"] = _cgi_path;
	_env["PATH_INFO"] = _cgi_path;
	_env["PATH_TRANSLATED"] = _cgi_path;
	_env["REQUEST_URI"] = _cgi_path;
	_env["SERVER_NAME"] = req.getHeader("host");
	_env["SERVER_PORT"] = "8002";
	_env["REQUEST_METHOD"] = req.getMethodStr();
	_env["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env["REDIRECT_STATUS"] = "200";
	_env["SERVER_SOFTWARE"] = "webserv";
	addRequestHeaders(req);
	buildEnvArray();
	buildArgvArray(cgi_exec);
}

/* Builds the CGI/1.1 environment for extension mapping, e.g. .py -> Python interpreter. */
void CgiHandler::initEnv(HttpRequest &req, const std::vector<Location>::iterator it_loc)
{
	std::map<std::string, std::string>::iterator it_path = it_loc->_ext_path.end();
	for (std::map<std::string, std::string>::iterator it = it_loc->_ext_path.begin();
		it != it_loc->_ext_path.end(); ++it)
	{
		const size_t ext_pos = _cgi_path.find(it->first);
		if (ext_pos != std::string::npos
			&& (ext_pos + it->first.length() == _cgi_path.length()
				|| _cgi_path[ext_pos + it->first.length()] == '/'))
		{
			it_path = it;
			break;
		}
	}
	if (it_path == it_loc->_ext_path.end())
		return;

	const int host_sep = findStart(req.getHeader("host"), ":");
	std::string query = req.getQuery();
	std::string script_name = req.getPath();
	for (std::vector<std::string>::const_iterator ext = it_loc->getCgiExtension().begin();
		ext != it_loc->getCgiExtension().end(); ++ext)
	{
		const size_t pos = script_name.find(*ext);
		if (pos != std::string::npos)
		{
			script_name.erase(pos + ext->length());
			break;
		}
	}
	const std::string path_info = getPathInfo(req.getPath(), it_loc->getCgiExtension());
	std::string translated_info = path_info;
	while (!translated_info.empty() && translated_info[0] == '/')
		translated_info.erase(0, 1);

	_env["CONTENT_LENGTH"] = req.getHeader("content-length");
	_env["CONTENT_TYPE"] = req.getHeader("content-type");
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SCRIPT_NAME"] = script_name;
	_env["SCRIPT_FILENAME"] = _cgi_path;
	_env["PATH_INFO"] = path_info;
	_env["PATH_TRANSLATED"] = it_loc->getRootLocation() + translated_info;
	_env["QUERY_STRING"] = decode(query);
	_env["REMOTE_ADDR"] = req.getHeader("host");
	_env["SERVER_NAME"] = (host_sep > 0 ? req.getHeader("host").substr(0, host_sep) : req.getHeader("host"));
	_env["SERVER_PORT"] = (host_sep > 0 ? req.getHeader("host").substr(host_sep + 1) : "");
	_env["REQUEST_METHOD"] = req.getMethodStr();
	_env["HTTP_COOKIE"] = req.getHeader("cookie");
	_env["DOCUMENT_ROOT"] = it_loc->getRootLocation();
	_env["REQUEST_URI"] = req.getPath()
		+ (req.getQuery().empty() ? "" : "?" + req.getQuery());
	_env["SERVER_PROTOCOL"] = req.getVersion();
	_env["REDIRECT_STATUS"] = "200";
	_env["SERVER_SOFTWARE"] = "webserv";
	addRequestHeaders(req);
	buildEnvArray();
	buildArgvArray(it_path->second);
}

/* Starts the CGI program:
 * pipe_in connects server -> CGI stdin.
 * pipe_out connects CGI stdout -> server.
 * fork() creates the child process.
 * In the child process, execve() replaces the child with the interpreter/script invocation. */
void CgiHandler::execute(short &error_code)
{
	if (!_argv || !_argv[0] || !_argv[1])
	{
		error_code = 500;
		return;
	}
	if (::pipe(pipe_in) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "pipe() failed");
		error_code = 500;
		return;
	}
	if (::pipe(pipe_out) < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "pipe() failed");
		closeFd(pipe_in[0]);
		closeFd(pipe_in[1]);
		error_code = 500;
		return;
	}

	_cgi_pid = ::fork();
	if (_cgi_pid == 0)
	{
		std::string script_arg = _cgi_path;
		const size_t slash = _cgi_path.rfind('/');
		if (slash != std::string::npos)
		{
			const std::string dir = _cgi_path.substr(0, slash);
			script_arg = _cgi_path.substr(slash + 1);
			if (!dir.empty())
				::chdir(dir.c_str());
		}
		delete [] _argv[1];
		_argv[1] = duplicateString(script_arg);
		::dup2(pipe_in[0], STDIN_FILENO);
		::dup2(pipe_out[1], STDOUT_FILENO);
		closeFd(pipe_in[0]);
		closeFd(pipe_in[1]);
		closeFd(pipe_out[0]);
		closeFd(pipe_out[1]);
		::execve(_argv[0], _argv, _ch_env);
		std::exit(127);
	}
	if (_cgi_pid < 0)
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "fork() failed");
		closeFd(pipe_in[0]);
		closeFd(pipe_in[1]);
		closeFd(pipe_out[0]);
		closeFd(pipe_out[1]);
		error_code = 500;
		return;
	}
	closeFd(pipe_in[0]);
	closeFd(pipe_out[1]);
}

/* Searches for delim in path and returns the position, or -1 if not found. */
int CgiHandler::findStart(const std::string &path, const std::string &delim) const
{
	const size_t pos = path.find(delim);
	if (pos == std::string::npos)
		return (-1);
	return (static_cast<int>(pos));
}

/* Decodes percent-encoding in URL/query strings directly in the provided string. */
std::string CgiHandler::decode(std::string &path) const
{
	size_t token = path.find('%');
	while (token != std::string::npos)
	{
		if (path.length() < token + 3)
			break;
		const char decimal = static_cast<char>(fromHexToDec(path.substr(token + 1, 2)));
		path.replace(token, 3, toString(decimal));
		token = path.find('%', token + 1);
	}
	return (path);
}

/* Extracts PATH_INFO: everything after the CGI extension, excluding the query string. */
std::string CgiHandler::getPathInfo(const std::string &path,
	const std::vector<std::string> &extensions) const
{
	size_t start = std::string::npos;
	size_t ext_len = 0;

	for (std::vector<std::string>::const_iterator it = extensions.begin();
		it != extensions.end(); ++it)
	{
		start = path.find(*it);
		if (start != std::string::npos)
		{
			ext_len = it->length();
			break;
		}
	}
	if (start == std::string::npos || start + ext_len >= path.size())
		return ("");

	const std::string path_info = path.substr(start + ext_len);
	if (path_info.empty() || path_info[0] != '/')
		return ("");

	const size_t query = path_info.find('?');
	if (query == std::string::npos)
		return (path_info);
	return (path_info.substr(0, query));
}

/* Resets the handler and frees all owned process/pipe/environment resources. */
void CgiHandler::clear()
{
	freeEnvArray();
	freeArgvArray();
	closeFd(pipe_in[0]);
	closeFd(pipe_in[1]);
	closeFd(pipe_out[0]);
	closeFd(pipe_out[1]);
	_cgi_pid = -1;
	_exit_status = 0;
	_cgi_path.clear();
	_env.clear();
}
