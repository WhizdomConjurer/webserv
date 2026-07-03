#include "logger.hpp"


std::string Logger::file_name = "logfile.txt";
LogPrio Logger::prio = ERROR;
L_State Logger::state = ON;

std::map<LogPrio, std::string> Logger::prio_str = initMap();


std::map<LogPrio, std::string> Logger::initMap()
{
    std::map<LogPrio, std::string> p_map;

    // Builds the printable prefix table for future priority-based logging.
    p_map[DEBUG] = "[DEBUG]   ";
    p_map[INFO] = "[INFO]    ";
    p_map[ERROR] = "[ERROR]   ";
    return p_map;
}

/* Formats a log line and writes it either to stdout or to logs/<file_name>. */
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
            if (mkdir("logs", 0777) < 0 && errno != EEXIST)
            {
                std::cerr << "mkdir() Error: " << strerror(errno) << std::endl;
                return ;
            }
            int fd = open(("logs/" + file_name).c_str(), O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR);
            if (fd < 0)
            {
                std::cerr << "open() Error: " << strerror(errno) << std::endl;
                return ;
            }
            write(fd, date.c_str(), date.length());
            write(fd, "   ", 3);
            write(fd, output, n);
            write(fd, "\n", 1);
            close(fd);
        }
        else if (m == CONSOLE_OUTPUT)
        {
            std::cout << color << getCurrTime() << output << RESET << std::endl;
        }
    }
}

/* Returns a compact timestamp used as a prefix for every log message. */
std::string Logger::getCurrTime()
{
    char date[1000];
    time_t now = time(0);
    struct tm tm = *localtime(&now);
    strftime(date, sizeof(date), "[%Y-%m-%d  %H:%M:%S]   ", &tm);
    return (std::string(date));
}

/* Sets the currently stored priority for future priority-aware logging. */
void Logger::setPrio(LogPrio p)
{
    Logger::prio = p;
}

/* Sets the file name used when FILE_OUTPUT logging is selected. */
void Logger::setFileName(const std::string &name)
{
    Logger::file_name = name;
}

/* Backward-compatible wrapper for the old misspelled setter name. */
void Logger::setFilenName(const std::string &name)
{
    setFileName(name);
}

/* Enables or disables logger output globally. */
void Logger::setState(L_State s)
{
    Logger::state = s;
}
