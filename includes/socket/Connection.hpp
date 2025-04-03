/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:18:35 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/04 02:23:35 by atsu             ###   ########.fr       */
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
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <set>

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

enum FileStatus
{
	SUCCESS,
	SUCCESS_CGI,
	SUCCESS_STATIC,
	ERROR,
	NOT_COMPLETED,
	CLOSED,
	BADREQUEST,
};

class Connection : public ASocket
{
private:
	Connection();

	// file
	CGI *cgi_; // dynamic file

	// buffer
	std::vector<char> rbuff_;
	std::vector<char> wbuff_;

	// request and response
	HttpRequest *request_;
	HttpResponse *response_;
	
	// conf
	conf_value_t conf_value_;

	// timeout
	std::time_t lastActive_;
	static std::time_t timeout_;

	bool is_timeout_;

public:
	Connection(int clinentFd);
	Connection(const Connection &other);
	~Connection();

	// check timeout
	int isTimedOut();

	// getter
	int getFd() const;
	CGI *getCGI() const;
	FileTypes getFdType(int fd) const;
	bool getIsTimeout() const;

	// setter
	void setErrorFd(int status_code);
	void setHttpRequest(MainConf *mainConf);
	void setHttpResponse();
	void clearValue();

	FileStatus processAfterReadCompleted(MainConf *mainConf);

	// method
		//cgi
	void initCGI();
	bool isCGI();
	void executeCGI();
		// file upload
	bool isFileUpload();
	FileStatus fileUpload(std::string upload_dir);
		// autoindex
	std::string getAutoIndexPath();
	void buildAutoIndexContent(std::string path);
		// file delete
	void deleteFile();
		// clean up
	void cleanUp();
		// response 構築
	void buildStaticFileResponse(int status_code);
	void buildBadRequestResponse();
	FileStatus buildRedirectResponse(int status_code, std::string redirect_path);
		// read write
	FileStatus readStaticFile(std::string file_path);
	FileStatus readSocket(MainConf *mainConf);
	FileStatus readCGI();
	FileStatus writeSocket();
		// chunked
	size_t getBodyLength(const std::vector<char>& buffer);
	bool hasFinalChunk(const std::vector<char>& buffer);
	bool isChunked();
	void setChunkedBody();

};

// ==================================== utils ====================================

std::vector<char> stringToVector(std::string str);

std::string vectorToString(std::vector<char> vec);

#endif
