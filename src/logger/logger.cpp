#include "logger.hpp"


std::string Logger::file_name = "logfile.txt";
LogPrio Logger::prio = ERROR;
L_State Logger::state = ON;

std::map<LogPrio, std::string> Logger::prio_str = initMap();

/* Erstellt die Textpräfixe für spätere prioritätsbasierte Logausgaben. */
std::map<LogPrio, std::string> Logger::initMap()
{
    std::map<LogPrio, std::string> p_map;

    p_map[DEBUG] = "[DEBUG]   ";
    p_map[INFO] = "[INFO]    ";
    p_map[ERROR] = "[ERROR]   ";
    return p_map;
}

/* Formatiert eine Logzeile und schreibt sie entweder in die Konsole oder nach logs/<file_name>. */
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

/* Erzeugt den Zeitstempel, der jeder Logzeile vorangestellt wird. */
std::string Logger::getCurrTime()
{
    char date[1000];
    time_t now = time(0);
    struct tm tm = *localtime(&now);
    strftime(date, sizeof(date), "[%Y-%m-%d  %H:%M:%S]   ", &tm);
    return (std::string(date));
}

/* Speichert die aktuelle Log-Priorität für zukünftige prioritätsbasierte Ausgaben. */
void Logger::setPrio(LogPrio p)
{
    Logger::prio = p;
}

/* Setzt den Dateinamen, der bei FILE_OUTPUT verwendet wird. */
void Logger::setFileName(const std::string &name)
{
    Logger::file_name = name;
}

/* Kompatibler Wrapper für den alten, falsch geschriebenen Setter-Namen. */
void Logger::setFilenName(const std::string &name)
{
    setFileName(name);
}

/* Schaltet alle Logger-Ausgaben global ein oder aus. */
void Logger::setState(L_State s)
{
    Logger::state = s;
}
