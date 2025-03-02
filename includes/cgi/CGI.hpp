// テスト用のcgiクラス 米野さんのものを最終的に用いる
#pragma once

#include <string>
#include <unistd.h>
#include <signal.h> 
#include <fcntl.h>
#include <sys/wait.h>

#include <iostream>

class CGI
{
private:
	int _fd;
	std::string _path;
	int _pid;

public:
	CGI();
	CGI(std::string path); // テスト用 本来はさまざまな値が渡される
	CGI(const CGI &cgi);
	~CGI();

	// getter
	int getFd() const;

	CGI &operator=(const CGI &cgi); // なんでか使えない
	void killCGI();
};
