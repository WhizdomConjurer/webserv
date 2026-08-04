#include "logger.hpp"
#include <fstream>


std::string Logger::file_name = "logfile.txt";
LogPrio Logger::prio = ERROR;
L_State Logger::state = ON;

std::map<LogPrio, std::string> Logger::prio_str = initMap();

/* Creates the text prefixes for later priority-based log output. */
std::map<LogPrio, std::string> Logger::initMap()
{
    std::map<LogPrio, std::string> p_map;

    p_map[DEBUG] = "[DEBUG]   ";
    p_map[INFO] = "[INFO]    ";
    p_map[ERROR] = "[ERROR]   ";
    return p_map;
}

/* Formats a log line and writes it either to the console or to logs/<file_name>. */
void    Logger::logMsg(const char *color, Mode m, const char* msg, ...)
{
    char        output[8192];
    va_list     args;
    int         n;

    if (state == ON)
    {
        va_start(args, msg);
        n = vsnprintf(output, sizeof(output), msg, args);
        va_end(args);
        if (n < 0)
            return ;
        if (n >= static_cast<int>(sizeof(output)))
            n = static_cast<int>(sizeof(output)) - 1;
        std::string date = getCurrTime();
        if (m == FILE_OUTPUT)
        {
			std::ofstream file(file_name.c_str(), std::ios::out | std::ios::app);
			if (!file)
				return ;
			file << date << "   "
				<< std::string(output, static_cast<size_t>(n)) << "\n";
        }
        else if (m == CONSOLE_OUTPUT)
        {
            std::cout << color << getCurrTime() << output << RESET << std::endl;
        }
    }
}

/* Generates the timestamp that is prepended to each log line. */
std::string Logger::getCurrTime()
{
    char date[1000];
    time_t now = time(0);
    struct tm tm = *localtime(&now);
    strftime(date, sizeof(date), "[%Y-%m-%d  %H:%M:%S]   ", &tm);
    return (std::string(date));
}

/* Stores the current log priority for future priority-based output. */
void Logger::setPrio(LogPrio p)
{
    Logger::prio = p;
}

/* Sets the filename used when FILE_OUTPUT is enabled. */
void Logger::setFileName(const std::string &name)
{
    Logger::file_name = name;
}

/* Compatible wrapper for the old, misspelled setter name. */
void Logger::setFilenName(const std::string &name)
{
    setFileName(name);
}

/* Globally enables or disables all logger output. */
void Logger::setState(L_State s)
{
    Logger::state = s;
}
