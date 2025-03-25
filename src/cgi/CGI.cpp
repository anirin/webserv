#include "CGI.hpp"

CGI::CGI(const std::string &scriptPath) : _scriptPath(scriptPath), _fd(-1) {}

CGI::CGI(const std::string& scriptPath, const std::string& body, const std::map<std::string, std::string>& headers) 
    : _scriptPath(scriptPath), _fd(-1), _pid(-1), _body(body), _headers(headers) {}

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
    
    if (!_body.empty()) {
        int stdin_pipe[2];
        if (pipe(stdin_pipe) == -1)
            throw std::runtime_error("stdin pipe failed");
            
        write(stdin_pipe[1], _body.c_str(), _body.length());
        close(stdin_pipe[1]);
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);
    }
    
    std::vector<std::string> env_strings;
    
    env_strings.push_back("SCRIPT_FILENAME=" + _scriptPath);
    
    if (!_body.empty()) {
        env_strings.push_back("REQUEST_METHOD=POST");
        
        std::stringstream ss;
        ss << _body.length();
        env_strings.push_back("CONTENT_LENGTH=" + ss.str());
        
        if (_headers.find("Content-Type") != _headers.end()) {
            env_strings.push_back("CONTENT_TYPE=" + _headers.at("Content-Type"));
        }
    } else {
        env_strings.push_back("REQUEST_METHOD=GET");
    }
    
    char** env = new char*[env_strings.size() + 1];
    for (size_t i = 0; i < env_strings.size(); i++) {
        env[i] = strdup(env_strings[i].c_str());
    }
    env[env_strings.size()] = NULL;
    
    char* args[] = {(char*)"/usr/bin/php", (char*)_scriptPath.c_str(), NULL};
    execve(args[0], args, env);
    
    for (size_t i = 0; i < env_strings.size(); i++) {
        free(env[i]);
    }
    delete[] env;
    
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