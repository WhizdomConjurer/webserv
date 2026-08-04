#include "config_parser.hpp"

/* Checks wether the directive is valid */
static bool isValidDirective(const size_t &i, const std::vector<std::string> &parameters)
{
	if ((i + 1) < parameters.size())
		return (true);
	else
		return (false);
}

/* Splits a directive string at every character contained in separator. */
static std::vector<std::string> splitParameters(const std::string &line, const std::string &separator)
{
	std::vector<std::string> str;
	std::string::size_type start = 0, end = 0;

	while (1)
	{
		end = line.find_first_of(separator, start);
		if (end == std::string::npos)
		{
			str.push_back(line.substr(start));
			break;
		}
		std::string tmp = line.substr(start, end - start);
		str.push_back(tmp);
		start = line.find_first_not_of(separator, end);
		if (start == std::string::npos)
			break;
	}
	return (str);
}

/* Builds a ServerConfig from a server{} block and checks required fields/duplicates. */
void ConfigParser::createServer(std::string &config, ServerConfig &server)
{
	std::vector<std::string> parameters;
	int flag_loc = 1;
	bool flag_autoindex = false;
	bool flag_max_size = false;

	parameters = splitParameters(config, std::string(" \n\t"));
	if (parameters.size() < 3)
		throw ErrorException("Failed server validation");
	for (size_t i = 0; i < parameters.size(); i++)
	{
		if (parameters[i] == "listen" && isValidDirective(i, parameters) && flag_loc)
		{
			if (server.getPort())
				i++;
			else if (i += server.checkPort(parameters, i + 1))
				server.setPort(parameters[i]);
		}
		else if (parameters[i] == "location" && isValidDirective(i, parameters))
		{
			std::string path;
			i++;
			if (parameters[i] == "{" || parameters[i] == "}")
				throw ErrorException("Wrong character in server scope{}");
			path = parameters[i];
			std::vector<std::string> codes;
			if (parameters[++i] != "{")
				throw ErrorException("Wrong character in server scope{}");
			i++;
			while (i < parameters.size() && parameters[i] != "}")
				codes.push_back(parameters[i++]);
			server.setLocation(path, codes);
			if (i < parameters.size() && parameters[i] != "}")
				throw ErrorException("Wrong character in server scope{}");
			flag_loc = 0;
		}
		else if (parameters[i] == "host" && isValidDirective(i, parameters) && flag_loc)
		{
			if (server.getHost())
				throw ErrorException("Host is duplicated");
			server.setHost(parameters[++i]);
		}
		else if (parameters[i] == "root" && isValidDirective(i, parameters) && flag_loc)
		{
			if (!server.getRoot().empty())
				throw ErrorException("Root is duplicated");
			server.setRoot(parameters[++i]);
		}
		else if (parameters[i] == "error_page" && isValidDirective(i, parameters) && flag_loc)
		{
			std::vector<std::string> directive_tokens;
			while (++i < parameters.size())
			{
				directive_tokens.push_back(parameters[i]);
				if (parameters[i].find(';') != std::string::npos)
					break;
				if (i + 1 >= parameters.size())
					throw ErrorException("Wrong character out of server scope{}");
			}
			server.setErrorPages(directive_tokens);
		}
		else if (parameters[i] == "client_max_body_size" && isValidDirective(i, parameters) && flag_loc)
		{
			if (flag_max_size)
				throw ErrorException("Client_max_body_size is duplicated");
			server.setClientMaxBodySize(parameters[++i]);
			flag_max_size = true;
		}
		else if (parameters[i] == "server_name" && isValidDirective(i, parameters) && flag_loc)
		{
			if (!server.getServerName().empty())
				throw ErrorException("Server_name is duplicated");
			server.setServerName(parameters[++i]);
		}
		else if (parameters[i] == "index" && isValidDirective(i, parameters) && flag_loc)
		{
			if (!server.getIndex().empty())
				throw ErrorException("Index is duplicated");
			server.setIndex(parameters[++i]);
		}
		else if (parameters[i] == "autoindex" && isValidDirective(i, parameters) && flag_loc)
		{
			if (flag_autoindex)
				throw ErrorException("Autoindex of server is duplicated");
			server.setAutoindex(parameters[++i]);
			flag_autoindex = true;
		}
		else if (parameters[i] != "}" && parameters[i] != "{")
		{
			if (!flag_loc)
				throw ErrorException("Parameters after location");
			else
				throw ErrorException("Unsupported directive");
		}
	}

	// Default directives if they are not provided by config file.
	if (server.getRoot().empty())
		server.setRoot("/;");
	if (server.getHost() == 0)
		server.setHost("localhost;");
	if (server.getIndex().empty())
		server.setIndex("index.html;");

	// Verfiies that all neccessary directives are present.
	if (ConfigFile::isFileExistAndReadable(server.getRoot(), server.getIndex()))
		throw ErrorException("Index from config file not found or unreadable");
	if (server.checkLocaitons())
		throw ErrorException("Location is duplicated");
	if (!server.getPort())
		throw ErrorException("Port not found");
	if (!server.isValidErrorPages())
		throw ErrorException("Incorrect path for error page or number of error");
}