#include "webserv.hpp"
#include "server/server_manager.hpp"

/* Catches SIGPIPE so that a disconnected client does not terminate the server process. */
void sigpipeHandle(int sig)
{
	(void)sig;
}

/* Program entry point: reads the configuration, creates ServerConfig objects from it, and starts the ServerManager. */
int main(int argc, char **argv)
{
	// Logger::setState(OFF);
	if (argc == 1 || argc == 2)
	{
		try
		{
			std::string config;
			ConfigParser cluster;
			ServerManager master;
			signal(SIGPIPE, sigpipeHandle);
			/* If no argument is provided, the default configuration is used; otherwise, the user-specified file is used. */
			config = (argc == 1 ? "configs/default.conf" : argv[1]);
			cluster.createCluster(config);
			// cluster.print(); // for checking (debug)
			master.setupServers(cluster.getServers());
			master.runServers();
		}
		catch (std::exception &e)
		{
			std::cerr << e.what() << std::endl;
			return (1);
		}
	}
	else
	{
		Logger::logMsg(RED, CONSOLE_OUTPUT, "Error: wrong arguments");
		return (1);
	}
	return (0);
}
