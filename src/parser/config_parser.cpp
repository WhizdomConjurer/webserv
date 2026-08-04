#include "config_parser.hpp"


ConfigParser::ConfigParser()
{
	this->_nb_server = 0;
}

/* Destroys the parser; the stored vectors clean up their contents automatically. */
ConfigParser::~ConfigParser() {}

/* Prints the parsed server data to stdout to manually check config issues. (debug) */ 
int ConfigParser::print()
{
	std::cout << "------------- Config -------------" << std::endl;
	for (size_t i = 0; i < _servers.size(); i++)
	{
		std::cout << "Server #" 		<< i + 1 								<< std::endl;
		std::cout << "Server name: " 	<< _servers[i].getServerName() 			<< std::endl;
		std::cout << "Host: " 			<< _servers[i].getHost() 				<< std::endl;
		std::cout << "Root: " 			<< _servers[i].getRoot() 				<< std::endl;
		std::cout << "Index: " 			<< _servers[i].getIndex() 				<< std::endl;
		std::cout << "Port: " 			<< _servers[i].getPort() 				<< std::endl;
		std::cout << "Max BSize: " 		<< _servers[i].getClientMaxBodySize() 	<< std::endl;
		std::cout << "Error pages: " 	<< _servers[i].getErrorPages().size() 	<< std::endl;
		
		std::map<short, std::string>::const_iterator it = _servers[i].getErrorPages().begin();
		while (it != _servers[i].getErrorPages().end())
		{
			std::cout << (*it).first << " - " << it->second << std::endl;
			++it;
		}
		std::cout << "Locations: " 		<< _servers[i].getLocations().size() 	<< std::endl;
		std::vector<Location>::const_iterator itl = _servers[i].getLocations().begin();
		while (itl != _servers[i].getLocations().end())
		{
			std::cout << "name location: " 	<< itl->getPath() 			<< std::endl;
			std::cout << "methods: " 		<< itl->getPrintMethods() 	<< std::endl;
			std::cout << "index: " 			<< itl->getIndexLocation() 	<< std::endl;
			if (itl->getCgiPath().empty())
			{
				std::cout 		<< "root: "		<< itl->getRootLocation() 	<< std::endl;
				if (!itl->getReturn().empty())
					std::cout 	<< "return: " 	<< itl->getReturn() 		<< std::endl;
				if (!itl->getAlias().empty())
					std::cout 	<< "alias: "	<< itl->getAlias() 			<< std::endl;
			}
			else
			{
				std::cout << "cgi root: " << itl->getRootLocation() 		<< std::endl;
				std::cout << "cgi_path: " << itl->getCgiPath().size() 		<< std::endl;
				std::cout << "cgi_ext: "  << itl->getCgiExtension().size() 	<< std::endl;
			}
			++itl;
		}
		itl = _servers[i].getLocations().begin();
		std::cout << "-----------------------------" << std::endl;
	}
	return (0);
}

/* Validates and parses the config file, removes comments, and creates ServerConfig objects. */
int ConfigParser::createCluster(const std::string &config_file)
{
	std::string content;
	ConfigFile file(config_file);

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

/* Removes comments starting with '#' and extending to the end of the line. */
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

/* Trims leading and trailing whitespace from the entire config. */
void ConfigParser::removeWhiteSpace(std::string &content)
{
	size_t i = 0;

	if (content.empty())
		return;
	while (content[i] && isspace(content[i]))
		i++;
	content = content.substr(i);
	i = content.length() - 1;
	while (i > 0 && isspace(content[i]))
		i--;
	content = content.substr(0, i + 1);
}

/* Splits the config content into individual raw server{} sections. */
void ConfigParser::splitServers(std::string &content)
{
	size_t start = 0;
	size_t end = 1;

	if (content.find("server", 0) == std::string::npos)
		throw ErrorException("Did not find \"Server\"");
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
size_t ConfigParser::findStartServer(size_t start, std::string &content)
{
	size_t i;

	for (i = start; content[i]; i++)
	{
		if (content[i] == 's')
			break;
		if (!isspace(content[i]))
			throw ErrorException("Wrong character, out of server scope{}");
	}
	if (!content[i])
		return (start);
	if (content.compare(i, 6, "server") != 0)
		throw ErrorException("Wrong character, out of server scope{}");
	i += 6;
	while (content[i] && isspace(content[i]))
		i++;
	if (content[i] == '{')
		return (i);
	else
		throw ErrorException("Wrong character, out of server scope{}");
}

/* Finds the matching closing brace for a server{} block. */
size_t ConfigParser::findEndServer(size_t start, std::string &content)
{
	size_t i;
	size_t scope;

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

/* Compares str2 with str1 at the given position and only matches complete words. */
int ConfigParser::stringCompare(const std::string &str1, const std::string &str2, size_t pos)
{
	size_t i;

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

/* Prevents duplicate servers with the same host/port/ServerName combination. */
void ConfigParser::checkServers()
{
	std::vector<ServerConfig>::iterator it1;
	std::vector<ServerConfig>::iterator it2;

	for (it1 = this->_servers.begin(); it1 != this->_servers.end() - 1; it1++)
	{
		for (it2 = it1 + 1; it2 != this->_servers.end(); it2++)
		{
			if (it1->getPort() == it2->getPort() && it1->getHostString() == it2->getHostString() && it1->getServerName() == it2->getServerName())
				throw ErrorException("Failed server validation");
		}
	}
}

/* Returns a reference to the parsed ServerConfig objects without copying them. */
const std::vector<ServerConfig> &ConfigParser::getServers() const
{
	return (this->_servers);
}
