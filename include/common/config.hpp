#pragma once

#include <iostream>
#include <fstream>
#include <string>

// #define ENABLE_LOGS

struct Logger
{
    std::ofstream logFile;

    Logger()
    {
#ifdef ENABLE_LOGS
        logFile.open("emulator.log", std::ios::out);
#endif
    }

    ~Logger()
    {
#ifdef ENABLE_LOGS
        if (logFile.is_open())
        {
            logFile.close();
        }
#endif
    }

    void write([[maybe_unused]] const std::string &mensagem)
    {
#ifdef ENABLE_LOGS
        if (logFile.is_open())
        {
            logFile << "[LOG]: " << mensagem << std::endl;
        }
#endif
    }
};

inline Logger meuLogger;

#ifdef ENABLE_LOGS
#define LOG_MSG(mensagem) meuLogger.write(mensagem)
#else
#define LOG_MSG(mensagem) \
    do                    \
    {                     \
    } while (0)
#endif