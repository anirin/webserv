#ifndef CGI_HPP
#define CGI_HPP

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class CGI {
private:
	std::string _script_path;
	std::string _query_string;
	int _fd;
	pid_t _pid;
	std::vector<char> _body;
	std::map<std::string, std::string> _headers;

	void createPipe(int pipefd[2]);
	pid_t createChildProcess();
	void executeScriptInChild(int pipefd[2]);
	void setupOutputFd(int pipefd[2]);

public:
	CGI(const std::string& script_path, const std::string& query_string, const std::vector<char>& body,
		const std::map<std::string, std::string> headers);
	CGI(const std::string& script_path, const std::string& query_string,
		const std::map<std::string, std::string> headers);
	~CGI();

	void execute();
	int getFd() const;
	int getPid() const;
};

#endif