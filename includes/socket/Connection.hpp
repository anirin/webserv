/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:18:35 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/02/24 12:05:50 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ACCELPTOR_HPP
#define ACCELPTOR_HPP

#include <ctime>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>


#include "ASocket.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "MainConf.hpp"
#include "MainConf.hpp"
#include "CGI.hpp"

enum FileTypes
{
	STATIC,
	PIPE,
	SOCKET,
};

class Connection : public ASocket
{
private:
	Connection();

	// file
	CGI cgi_; // dynamic file
	int static_fd_; // static file

	// buffer
	std::string rbuff_;
	std::string wbuff_;
	std::string static_file_buff_;

	// request and response
	HttpRequest *request_;
	HttpResponse *response_;
	
	// conf
	conf_value_t conf_value_;

	// timeout
	std::time_t lastActive_;
	static std::time_t timeout_;

public:
	Connection(int clinentFd);
	Connection(const Connection &other);
	~Connection();

	// check timeout
	bool isTimedOut();
	bool findError(MainConf &mainConf);

	// getter
	int getFd() const;
	int getStaticFd() const;
	CGI getCGI() const;
	std::string getRbuff() const;
	std::string getWbuff() const;
	HttpRequest *getRequest() const;
	HttpResponse *getResponse() const;
	FileTypes getFdType(int fd) const;

	// setter
	void buildResponseString();
	void setWbuff(std::string wbuff);
	void setReadFd();
	void setErrorFd();
	void setStaticBuff(std::string static_buff);
	void setHttpRequest(MainConf *mainConf);
	void setHttpResponse();
	void setHttpResponseHeader();
	void setHttpResponseBody();
	void setStaticFd(int fd);
	void clearValue();

	// read and write
	bool readSocket();
	bool writeSocket();
};

/* Connection *getConnection(std::vector<Connection *> &connections, int fd); */
std::string vecToString(std::vector<std::string> vec);
std::string mapToString(std::map<std::string, std::string>);


#endif
