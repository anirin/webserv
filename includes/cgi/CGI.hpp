#ifndef CGI_HPP
#define CGI_HPP

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

class CGI
{
private:
    std::string _scriptPath;
    int _fd;
    pid_t _pid;
    std::string _body;
    std::map<std::string, std::string> _headers;

    void createPipe(int pipefd[2]);
    pid_t createChildProcess();
    void executeScriptInChild(int pipefd[2]);
    std::string readScriptOutput(int pipefd[2]);

public:
    CGI(const std::string& scriptPath);
    CGI(const std::string& scriptPath, const std::string& body, const std::map<std::string, std::string>& headers);
    ~CGI();

    std::string execute();
    int getFd() const;
    void killCGI();
};

#endif