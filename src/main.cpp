#include "webserv.hpp"
#include "server/server_manager.hpp"

/* Fängt SIGPIPE ab, damit ein abgebrochener Client den Serverprozess nicht beendet. */
void sigpipeHandle(int sig)
{
	(void)sig;
}

/* Programmeinstieg: liest die Config, baut daraus ServerConfig-Objekte und startet den ServerManager. */
int main(int argc, char **argv) 
{
	// Logger::setState(OFF);
	if (argc == 1 || argc == 2) {
		try 
		{
			std::string		config;
			ConfigParser	cluster;
			ServerManager 	master;
			signal(SIGPIPE, sigpipeHandle);
			/* Ohne Argument wird die Default-Config genutzt, sonst die vom Benutzer angegebene Datei. */
			config = (argc == 1 ? "configs/default.conf" : argv[1]);
			cluster.createCluster(config);
			// cluster.print(); // for checking
			master.setupServers(cluster.getServers());
			master.runServers();
		}
		catch (std::exception &e) {
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
