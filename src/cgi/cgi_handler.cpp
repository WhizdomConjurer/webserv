#include "cgi_handler.hpp"

/* Initialisiert einen leeren CGI-Handler mit geschlossenen Pipes und ohne Kindprozess. */
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

/* Initialisiert den CGI-Handler direkt mit dem Skriptpfad, der später ausgeführt wird. */
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

/* Gibt argv/env-Speicher frei und schließt alle Pipes, die noch offen sind. */
CgiHandler::~CgiHandler()
{
	clear();
}

/* Kopiert nur Wertdaten; argv/env werden pro Anfrage neu gebaut, um Double-Free zu vermeiden. */
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

/* Weist sichere Wertdaten zu und räumt vorher eigene argv/env-Ressourcen auf. */
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

/* Speichert die PID des CGI-Kindprozesses, damit der Aufrufer später waitpid() nutzen kann. */
void CgiHandler::setCgiPid(pid_t cgi_pid)
{
	_cgi_pid = cgi_pid;
}

/* Speichert den absoluten oder relativen Skriptpfad, der an CGI übergeben wird. */
void CgiHandler::setCgiPath(const std::string &cgi_path)
{
	_cgi_path = cgi_path;
}

/* Gibt die CGI-Umgebung als Map zurück, bevor sie in char** für execve() umgewandelt wird. */
const std::map<std::string, std::string> &CgiHandler::getEnv() const
{
	return (_env);
}

/* Gibt die aktuelle CGI-Kindprozess-PID zurück, oder -1 wenn keiner aktiv ist. */
pid_t CgiHandler::getCgiPid() const
{
	return (_cgi_pid);
}

/* Gibt den Skriptpfad dieser CGI-Anfrage zurück. */
const std::string &CgiHandler::getCgiPath() const
{
	return (_cgi_path);
}

/* Wandelt die Environment-Map in ein NULL-terminiertes char** um, wie execve() es erwartet. */
void CgiHandler::buildEnvArray()
{
	freeEnvArray();
	_ch_env = static_cast<char **>(std::calloc(_env.size() + 1, sizeof(char *)));
	if (!_ch_env)
		throw std::bad_alloc();

	size_t index = 0;
	for (std::map<std::string, std::string>::const_iterator it = _env.begin();
		it != _env.end(); ++it)
	{
		const std::string entry = it->first + "=" + it->second;
		_ch_env[index] = ::strdup(entry.c_str());
		if (!_ch_env[index])
			throw std::bad_alloc();
		++index;
	}
}

/* Baut das NULL-terminierte argv-Array: argv[0] ist Interpreter, argv[1] das Skript. */
void CgiHandler::buildArgvArray(const std::string &exec_path)
{
	freeArgvArray();
	_argv = static_cast<char **>(std::calloc(3, sizeof(char *)));
	if (!_argv)
		throw std::bad_alloc();
	_argv[0] = ::strdup(exec_path.c_str());
	_argv[1] = ::strdup(_cgi_path.c_str());
	if (!_argv[0] || !_argv[1])
		throw std::bad_alloc();
}

/* Gibt das aktuell für execve() gebaute Environment-Array frei. */
void CgiHandler::freeEnvArray()
{
	if (!_ch_env)
		return;
	for (size_t i = 0; _ch_env[i]; ++i)
		std::free(_ch_env[i]);
	std::free(_ch_env);
	_ch_env = NULL;
}

/* Gibt das aktuell für execve() gebaute argv-Array frei. */
void CgiHandler::freeArgvArray()
{
	if (!_argv)
		return;
	for (size_t i = 0; _argv[i]; ++i)
		std::free(_argv[i]);
	std::free(_argv);
	_argv = NULL;
}

/* Schließt einen File Descriptor, falls er offen ist, und markiert ihn danach mit -1. */
void CgiHandler::closeFd(int &fd)
{
	if (fd >= 0)
		::close(fd);
	fd = -1;
}

/* Überträgt HTTP-Header in CGI-Variablen, z.B. user-agent -> HTTP_USER_AGENT. */
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

/* Baut eine einfache CGI-Umgebung aus Requestdaten und Location-CGI-Einstellungen. */
void CgiHandler::initEnvCgi(HttpRequest &req, const std::vector<Location>::iterator it_loc)
{
	if (_cgi_path.empty())
		return;

	const std::string cgi_exec = "cgi-bin/" + it_loc->getCgiPath()[0];
	char cwd_buffer[4096];
	if (_cgi_path[0] != '/' && ::getcwd(cwd_buffer, sizeof(cwd_buffer)))
		_cgi_path = std::string(cwd_buffer) + "/" + _cgi_path;

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

/* Baut die CGI/1.1-Umgebung für Endungs-Mapping, z.B. .py -> Python-Interpreter. */
void CgiHandler::initEnv(HttpRequest &req, const std::vector<Location>::iterator it_loc)
{
	const size_t dot = _cgi_path.rfind('.');
	if (dot == std::string::npos)
		return;

	const std::string extension = _cgi_path.substr(dot);
	std::map<std::string, std::string>::iterator it_path = it_loc->_ext_path.find(extension);
	if (it_path == it_loc->_ext_path.end())
		return;

	const int host_sep = findStart(req.getHeader("host"), ":");
	std::string query = req.getQuery();

	_env["AUTH_TYPE"] = "Basic";
	_env["CONTENT_LENGTH"] = req.getHeader("content-length");
	_env["CONTENT_TYPE"] = req.getHeader("content-type");
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SCRIPT_NAME"] = _cgi_path;
	_env["SCRIPT_FILENAME"] = _cgi_path;
	_env["PATH_INFO"] = getPathInfo(req.getPath(), it_loc->getCgiExtension());
	_env["PATH_TRANSLATED"] = it_loc->getRootLocation()
		+ (_env["PATH_INFO"].empty() ? "/" : _env["PATH_INFO"]);
	_env["QUERY_STRING"] = decode(query);
	_env["REMOTE_ADDR"] = req.getHeader("host");
	_env["SERVER_NAME"] = (host_sep > 0 ? req.getHeader("host").substr(0, host_sep) : req.getHeader("host"));
	_env["SERVER_PORT"] = (host_sep > 0 ? req.getHeader("host").substr(host_sep + 1) : "");
	_env["REQUEST_METHOD"] = req.getMethodStr();
	_env["HTTP_COOKIE"] = req.getHeader("cookie");
	_env["DOCUMENT_ROOT"] = it_loc->getRootLocation();
	_env["REQUEST_URI"] = req.getPath() + req.getQuery();
	_env["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env["REDIRECT_STATUS"] = "200";
	_env["SERVER_SOFTWARE"] = "webserv";
	addRequestHeaders(req);
	buildEnvArray();
	buildArgvArray(it_path->second);
}

/*
 * Startet das CGI-Programm:
 * - pipe_in verbindet Server -> CGI-stdin.
 * - pipe_out verbindet CGI-stdout -> Server.
 * - fork() erzeugt den Kindprozess.
 * - Im Kindprozess ersetzt execve() das Kind durch den Interpreter/Skript-Aufruf.
 */
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
		std::free(_argv[1]);
		_argv[1] = ::strdup(script_arg.c_str());
		if (!_argv[1])
			std::exit(127);
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

/* Sucht delim in path und gibt die Position zurück, oder -1 wenn nichts gefunden wurde. */
int CgiHandler::findStart(const std::string &path, const std::string &delim) const
{
	const size_t pos = path.find(delim);
	if (pos == std::string::npos)
		return (-1);
	return (static_cast<int>(pos));
}

/* Dekodiert Prozentkodierung in URL/Query-Strings direkt im übergebenen String. */
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

/* Extrahiert PATH_INFO: alles nach der CGI-Endung, aber ohne Query-String. */
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

/* Setzt den Handler zurück und gibt alle eigenen Prozess-/Pipe-/Environment-Ressourcen frei. */
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
