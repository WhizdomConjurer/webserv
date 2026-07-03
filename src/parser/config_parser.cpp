#include "config_parser.hpp"

/* Initializes an empty parser before a config file is loaded. */
ConfigParser::ConfigParser()
{
	this->_nb_server = 0;
}

/* Destroys the parser and releases vector-owned configuration data. */
ConfigParser::~ConfigParser() { }

/* Prints parsed server configuration values for manual debugging. */
int ConfigParser::print()
{
	std::cout << "------------- Config -------------" << std::endl;
	for (size_t i = 0; i < _servers.size(); i++)
	{
		std::cout << "Server #" << i + 1 << std::endl;
		std::cout << "Server name: " << _servers[i].getServerName() << std::endl;
		std::cout << "Host: " << _servers[i].getHost() << std::endl;
		std::cout << "Root: " << _servers[i].getRoot() << std::endl;
		std::cout << "Index: " << _servers[i].getIndex() << std::endl;
		std::cout << "Port: " << _servers[i].getPort() << std::endl;
		std::cout << "Max BSize: " << _servers[i].getClientMaxBodySize() << std::endl;
		std::cout << "Error pages: " << _servers[i].getErrorPages().size() << std::endl;
		std::map<short, std::string>::const_iterator it = _servers[i].getErrorPages().begin();
		while (it != _servers[i].getErrorPages().end())
		{
			std::cout << (*it).first << " - " << it->second << std::endl;
			++it;
		}
		std::cout << "Locations: " << _servers[i].getLocations().size() << std::endl;
		std::vector<Location>::const_iterator itl = _servers[i].getLocations().begin();
		while (itl != _servers[i].getLocations().end())
		{
			std::cout << "name location: " << itl->getPath() << std::endl;
			std::cout << "methods: " << itl->getPrintMethods() << std::endl;
			std::cout << "index: " << itl->getIndexLocation() << std::endl;
			if (itl->getCgiPath().empty())
			{
				std::cout << "root: " << itl->getRootLocation() << std::endl;
				if (!itl->getReturn().empty())
					std::cout << "return: " << itl->getReturn() << std::endl;
				if (!itl->getAlias().empty())
					std::cout << "alias: " << itl->getAlias() << std::endl;
			}
			else
			{
				std::cout << "cgi root: " << itl->getRootLocation() << std::endl;
				std::cout << "cgi_path: " << itl->getCgiPath().size() << std::endl;
				std::cout << "cgi_ext: " << itl->getCgiExtension().size() << std::endl;
			}
			++itl;
		}
		itl = _servers[i].getLocations().begin();
		std::cout << "-----------------------------" << std::endl;
	}
	return (0);
}

/* Validates, reads, cleans, splits, and converts the config file into server objects. */
int ConfigParser::createCluster(const std::string &config_file)
{
	std::string		content;
	ConfigFile		file(config_file);

	if (file.getTypePath(file.getPath()) != 1)
		throw ErrorException("File is invalid");
	if (file.checkFile(file.getPath(), 4) == -1)
		throw ErrorException("File is not accessible");
	content = file.readFile(config_file);
	if (content.empty())
		throw ErrorException("File is empty");
	removeComments(content);
	removeWhiteSpace(content);
	splitServers(content);
	if (this->_server_config.size() != this->_nb_server)
		throw ErrorException("Server block count does not match parsed server data");
	for (size_t i = 0; i < this->_nb_server; i++)
	{
		ServerConfig server;
		createServer(this->_server_config[i], server);
		this->_servers.push_back(server);
	}
	if (this->_nb_server > 1)
		checkServers();
	return (0);
}

/* Removes comments beginning with # up to the end of the line. */
void ConfigParser::removeComments(std::string &content)
{
	size_t pos;

	pos = content.find('#');
	while (pos != std::string::npos)
	{
		size_t pos_end;
		pos_end = content.find('\n', pos);
		if (pos_end == std::string::npos)
			content.erase(pos);
		else
			content.erase(pos, pos_end - pos);
		pos = content.find('#');
	}
}

/* Trims leading and trailing whitespace from the full config content. */
void ConfigParser::removeWhiteSpace(std::string &content)
{
	size_t	i = 0;

	if (content.empty())
		return ;
	while (content[i] && isspace(content[i]))
		i++;
	content = content.substr(i);
	i = content.length() - 1;
	while (i > 0 && isspace(content[i]))
		i--;
	content = content.substr(0, i + 1);
}

/* Splits the file content into one raw string per server block. */
void ConfigParser::splitServers(std::string &content)
{
	size_t start = 0;
	size_t end = 1;

	if (content.find("server", 0) == std::string::npos)
		throw ErrorException("Server did not find");
	while (start != end && start < content.length())
	{
		start = findStartServer(start, content);
		end = findEndServer(start, content);
		if (start == end)
			throw ErrorException("problem with scope");
		this->_server_config.push_back(content.substr(start, end - start + 1));
		this->_nb_server++;
		start = end + 1;
	}
}

/* Finds the opening brace of the next server block. */
size_t ConfigParser::findStartServer (size_t start, std::string &content)
{
	size_t i;

	for (i = start; content[i]; i++)
	{
		if (content[i] == 's')
			break ;
		if (!isspace(content[i]))
			throw  ErrorException("Wrong character out of server scope{}");
	}
	if (!content[i])
		return (start);
	if (content.compare(i, 6, "server") != 0)
		throw ErrorException("Wrong character out of server scope{}");
	i += 6;
	while (content[i] && isspace(content[i]))
		i++;
	if (content[i] == '{')
		return (i);
	else
		throw  ErrorException("Wrong character out of server scope{}");

}

/* Finds the matching closing brace for a server block. */
size_t ConfigParser::findEndServer (size_t start, std::string &content)
{
	size_t	i;
	size_t	scope;
	
	scope = 0;
	for (i = start + 1; content[i]; i++)
	{
		if (content[i] == '{')
			scope++;
		if (content[i] == '}')
		{
			if (!scope)
				return (i);
			scope--;
		}
	}
	return (start);
}

/* Splits a directive string on any separator character in sep. */
std::vector<std::string> splitParameters(const std::string &line, const std::string &sep)
{
	std::vector<std::string>	str;
	std::string::size_type		start, end;

	start = end = 0;
	while (1)
	{
		end = line.find_first_of(sep, start);
		if (end == std::string::npos)
			break;
		std::string tmp = line.substr(start, end - start);
		str.push_back(tmp);
		start = line.find_first_not_of(sep, end);
		if (start == std::string::npos)
			break;
	}
	return (str);
}

/* Creates a ServerConfig from one server block and validates required directives. */
void ConfigParser::createServer(std::string &config, ServerConfig &server)
{
	std::vector<std::string>	parametrs;
	std::vector<std::string>	error_codes;
	int		flag_loc = 1;
	bool	flag_autoindex = false;
	bool	flag_max_size = false;

	parametrs = splitParameters(config += ' ', std::string(" \n\t"));
	if (parametrs.size() < 3)
		throw  ErrorException("Failed server validation");
	for (size_t i = 0; i < parametrs.size(); i++)
	{
		if (parametrs[i] == "listen" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (server.getPort())
				throw  ErrorException("Port is duplicated");
			server.setPort(parametrs[++i]);
		}
		else if (parametrs[i] == "location" && (i + 1) < parametrs.size())
		{
			std::string	path;
			i++;
			if (parametrs[i] == "{" || parametrs[i] == "}")
				throw  ErrorException("Wrong character in server scope{}");
			path = parametrs[i];
			std::vector<std::string> codes;
			if (parametrs[++i] != "{")
				throw  ErrorException("Wrong character in server scope{}");
			i++;
			while (i < parametrs.size() && parametrs[i] != "}")
				codes.push_back(parametrs[i++]);
			server.setLocation(path, codes);
			if (i < parametrs.size() && parametrs[i] != "}")
				throw  ErrorException("Wrong character in server scope{}");
			flag_loc = 0;
		}
		else if (parametrs[i] == "host" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (server.getHost())
				throw  ErrorException("Host is duplicated");
			server.setHost(parametrs[++i]);
		}
		else if (parametrs[i] == "root" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (!server.getRoot().empty())
				throw  ErrorException("Root is duplicated");
			server.setRoot(parametrs[++i]);
		}
		else if (parametrs[i] == "error_page" && (i + 1) < parametrs.size() && flag_loc)
		{
			while (++i < parametrs.size())
			{
				error_codes.push_back(parametrs[i]);
				if (parametrs[i].find(';') != std::string::npos)
					break ;
				if (i + 1 >= parametrs.size())
					throw ErrorException("Wrong character out of server scope{}");
			}
		}
		else if (parametrs[i] == "client_max_body_size" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (flag_max_size)
				throw  ErrorException("Client_max_body_size is duplicated");
			server.setClientMaxBodySize(parametrs[++i]);
			flag_max_size = true;
		}
		else if (parametrs[i] == "server_name" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (!server.getServerName().empty())
				throw  ErrorException("Server_name is duplicated");
			server.setServerName(parametrs[++i]);
		}
		else if (parametrs[i] == "index" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (!server.getIndex().empty())
				throw  ErrorException("Index is duplicated");
			server.setIndex(parametrs[++i]);
		}
		else if (parametrs[i] == "autoindex" && (i + 1) < parametrs.size() && flag_loc)
		{
			if (flag_autoindex)
				throw ErrorException("Autoindex of server is duplicated");
			server.setAutoindex(parametrs[++i]);
			flag_autoindex = true;
		}
		else if (parametrs[i] != "}" && parametrs[i] != "{")
		{
			if (!flag_loc)
				throw  ErrorException("Parameters after location");
			else
				throw  ErrorException("Unsupported directive");
		}
	}
	if (server.getRoot().empty())
		server.setRoot("/;");
	if (server.getHost() == 0)
		server.setHost("localhost;");
	if (server.getIndex().empty())
		server.setIndex("index.html;");
	if (ConfigFile::isFileExistAndReadable(server.getRoot(), server.getIndex()))
		throw ErrorException("Index from config file not found or unreadable");
	if (server.checkLocaitons())
		throw ErrorException("Location is duplicated");
	if (!server.getPort())
		throw ErrorException("Port not found");
	server.setErrorPages(error_codes);
	if (!server.isValidErrorPages())
		throw ErrorException("Incorrect path for error page or number of error");
}

/* Compares str2 against str1 at pos and ensures the match ends at whitespace/end. */
int	ConfigParser::stringCompare(const std::string &str1, const std::string &str2, size_t pos)
{
	size_t	i;

	i = 0;
	while (pos < str1.length() && i < str2.length() && str1[pos] == str2[i])
	{
		pos++;
		i++;
	}
	if (i == str2.length() && pos <= str1.length() && (str1.length() == pos || isspace(str1[pos])))
		return (0);
	return (1);
}

/* Rejects duplicate servers with identical host, port, and server_name. */
void ConfigParser::checkServers()
{
	std::vector<ServerConfig>::iterator it1;
	std::vector<ServerConfig>::iterator it2;

	for (it1 = this->_servers.begin(); it1 != this->_servers.end() - 1; it1++)
	{
		for (it2 = it1 + 1; it2 != this->_servers.end(); it2++)
		{
			if (it1->getPort() == it2->getPort() && it1->getHost() == it2->getHost() && it1->getServerName() == it2->getServerName())
				throw ErrorException("Failed server validation");
		}
	}
}

/* Returns the parsed server list without copying it. */
const std::vector<ServerConfig>	&ConfigParser::getServers() const
{
	return (this->_servers);
}
