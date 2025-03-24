#include "CGI.hpp"

CGI::CGI(const std::string &scriptPath) : _scriptPath(scriptPath), _fd(-1) {}

void CGI::createPipe(int pipefd[2]) {
    if (pipe(pipefd) == -1)
        throw std::runtime_error("pipe");
}

pid_t CGI::createChildProcess() {
    pid_t pid = fork();
    if (pid == -1)
        throw std::runtime_error("fork");
    return pid;
}

void CGI::executeScriptInChild(int pipefd[2]) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    char* args[] = {(char*)"/usr/bin/php", (char*)_scriptPath.c_str(), NULL};
    execve(args[0], args, NULL);

    throw std::runtime_error("execve");
}

std::string CGI::readScriptOutput(int pipefd[2]) {
    close(pipefd[1]);

    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if(flags == -1) {
        throw std::runtime_error("fcntl failed");
    }
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    
    _fd = pipefd[0];

    char buffer[1024];
    std::string output;
    ssize_t bytesRead;

    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    return output;
}

std::string CGI::execute() {
    int pipefd[2];
    pid_t pid;
    std::string output;

    try {
        createPipe(pipefd);
        pid = CGI::createChildProcess();
        if (pid == 0) {
            CGI::executeScriptInChild(pipefd);
        } else {
            output = CGI::readScriptOutput(pipefd);
            waitpid(pid, NULL, 0);
        }
    } catch (const std::exception& e) {
        if (pid == 0)
            _exit(1); 
        throw;
    }

    return output;
}

int CGI::getFd() const {
    return _fd;
}

void CGI::killCGI() {
	kill(_pid, SIGKILL);
}

CGI::~CGI() {
    if (_fd != -1) {
        close(_fd);
    }
}