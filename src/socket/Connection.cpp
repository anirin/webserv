/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/25 15:01:54 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 100000;
ssize_t buff_size = 1024; // todo 持たせ方の検討

// ==================================== constructor and destructor ====================================

Connection::Connection() : ASocket() {}

Connection::Connection(int listenerFd) : ASocket() { // throw
	/* make a new socket for the client */
	socklen_t len = sizeof(addr_);
	fd_ = accept(listenerFd, (struct sockaddr *)&addr_, &len);
	if(fd_ == -1)
		throw std::runtime_error("accept failed");
	if(fcntl(fd_, F_SETFL, O_NONBLOCK))
		throw std::runtime_error("Failed to set socket to non-blocking");
	request_ = NULL;
	response_ = NULL;
	lastActive_ = std::time(NULL);
	std::cout << "[connection] Accepted connection from sin_port = " << addr_.sin_port << std::endl;

	cgi_ = NULL;
}

Connection::Connection(const Connection &other) : ASocket(other) {
	if(this == &other)
		return;
	fd_ = other.fd_;
	cgi_ = other.cgi_;
	rbuff_ = other.rbuff_;
	wbuff_ = other.wbuff_;
	request_ = other.request_;
	response_ = other.response_;
	lastActive_ = other.lastActive_;
}

Connection::~Connection() {
	close(fd_);
}

// ==================================== getter ====================================

int Connection::getFd() const {
	return fd_;
}

CGI *Connection::getCGI() const {
	return cgi_;
}

FileTypes Connection::getFdType(int fd) const {
	if(cgi_ != NULL && fd == cgi_->getFd())
		return PIPE;
	else
		return SOCKET;
}

// ==================================== setter ====================================

void Connection::setHttpRequest(MainConf *mainConf) { // throw
	try {
		request_ = new HttpRequest(rbuff_, mainConf);
	} catch(const std::exception &e) {
		std::cerr << "[connection] Failed to parse request: " << e.what() << std::endl;
		throw std::runtime_error("Failed to parse request");
	}
	conf_value_ = mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	std::cout << "[connection] request is set" << std::endl;
	std::cout << rbuff_ << std::endl;

	mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

void Connection::setCGI() { // throw
    std::string location_path = request_->getLocationPath();
    int path_size = location_path.size();

    if(path_size > 4 && location_path.substr(path_size - 4, 4) == ".php") {
        // Convert to absolute path
        char abs_path[PATH_MAX];
        if (realpath(location_path.c_str(), abs_path) == NULL) {
            std::cerr << "[connection] Failed to resolve path: " << location_path << std::endl;
            throw std::runtime_error("Failed to resolve CGI script path");
        }
        
        std::string absolute_path(abs_path);
        std::cout << "[connection] Resolved CGI path: " << absolute_path << std::endl;
        
        CGI *cgi = NULL;
        Method method = request_->getMethod();
        if (method == POST) {
            // Use absolute path here
            cgi = new CGI(absolute_path, request_->getBody(), request_->getHeader());
        } else {
            cgi = new CGI(absolute_path);
        }
        
        if(cgi == NULL) {
            std::cerr << "[connection] Failed to create CGI object" << std::endl;
            throw std::runtime_error("Failed to create CGI object");
        }
        
        // Initialize the pipe and start the CGI process
        try {
            cgi->init();
        } catch (const std::exception& e) {
            delete cgi;
            throw std::runtime_error(std::string("Failed to initialize CGI: ") + e.what());
        }
        
        cgi_ = cgi;
        std::cout << "[connection] cgi is set" << std::endl;
        return;
    }
}

void Connection::clearValue() {
	response_ = NULL;
}

// ==================================== check ==============================================

bool Connection::isTimedOut(MainConf *mainConf) {
	std::time_t now = std::time(NULL);
	if(now - lastActive_ <= timeout_) {
		lastActive_ = now;
		return false;
	}
	std::cout << "[connection] time out" << std::endl;

	setHttpRequest(mainConf);
	setHttpResponse();
	response_->setStatusCode(504);

	setErrorFd(504);

	return true;
}

// ==================================== read and write ====================================

FileStatus Connection::readSocket(MainConf *mainConf) {
	char buff[buff_size];
	ssize_t rlen = recv(fd_, buff, buff_size, 0);

	if(rlen < 0) {
		return ERROR;
	} else if(rlen == 0) {
		std::cout << "[connection] read socket closed by client" << std::endl;
		return CLOSED;
	} else if(rlen == buff_size) {
		rbuff_ += buff;
		return NOT_COMPLETED;
	}

	buff[rlen] = '\0';
	rbuff_ += buff;

	// std::cout << "rbuff: " << rbuff_ << std::endl;

	return processAfterReadCompleted(mainConf);
}

FileStatus Connection::readStaticFile(std::string file_path) {
	std::ifstream ifs(file_path.c_str(), std::ios::binary); // バイナリモード推奨
	if(!ifs) {
		std::cerr << "[connection] open file failed" << std::endl;
		return ERROR;
	}

	// ファイル全体を一度に読み込み
	wbuff_.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	if(ifs.bad()) {
		std::cerr << "[connection] read error" << std::endl;
		return ERROR;
	}

	ifs.close();
	return SUCCESS;
}

FileStatus Connection::readCGI() {
    char buff[buff_size];
    ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

    if(rlen < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NOT_COMPLETED;
        }
        std::cerr << "[connection] read pipe failed: " << strerror(errno) << std::endl;
        cgi_->killCGI();
        return ERROR;
    } 
    else if(rlen == 0) {
        int status;
        waitpid(cgi_->getPid(), &status, WNOHANG);
        
        if (WIFEXITED(status)) {
            std::cout << "[connection] CGI process exited with status: " << WEXITSTATUS(status) << std::endl;
        }
        
        setHttpResponse();
        
        if (wbuff_.empty()) {
            wbuff_ = "CGI script produced no output";
            std::cout << "[connection] Warning: Empty CGI output" << std::endl;
        }
        
        std::cout << "[connection] CGI output:\n" << wbuff_ << std::endl;
        
        if (!wbuff_.empty()) {
            if (wbuff_.find("HTTP/") != 0) {
                response_->setStartLine(200);
                response_->setBody(wbuff_);
            } else {
            }
        } else {
            wbuff_ = "CGI script produced no output";
            response_->setStartLine(200);
            response_->setBody(wbuff_);
        }
        
        if (request_ != NULL) {
            std::map<std::string, std::string> headers = request_->getHeader();
            std::string path = request_->getLocationPath();
            std::string server_name = request_->getServerName();
            response_->setHeader(headers, path, server_name);
        } else {
            std::map<std::string, std::string> empty_headers;
            response_->setHeader(empty_headers, "", "localhost");
            std::cout << "[connection] Warning: Request object was NULL when building CGI response" << std::endl;
        }
        
        wbuff_ = response_->buildResponse();
        
        close(cgi_->getFd());
        delete cgi_;
        cgi_ = NULL;
        return SUCCESS;
    }

    buff[rlen] = '\0';
    wbuff_ += buff;
    return NOT_COMPLETED;
}

FileStatus Connection::writeSocket() {
    char buff[buff_size];

    if(!request_ && wbuff_.empty()) {
        std::cerr << "[connection] No request and no buffered data" << std::endl;
        return ERROR;
    }

    if(wbuff_.empty()) {
        return NOT_COMPLETED;
    }

    ssize_t copy_len = std::min(wbuff_.size(), static_cast<std::size_t>(buff_size));
    std::memcpy(buff, wbuff_.data(), copy_len);
    if(copy_len != buff_size)
        buff[copy_len] = '\0';
    wbuff_.erase(0, copy_len);
    ssize_t wlen = send(fd_, buff, copy_len, 0);
    if(wlen == -1)
        return ERROR;
    if(wlen == buff_size)
        return NOT_COMPLETED;

    if(wbuff_.empty()) {
        if(response_) {
            delete response_;
            response_ = NULL;
        }
        if(request_) {
            delete request_;
            request_ = NULL;
        }
        std::cout << "[connection] write socket completed" << std::endl;
        return SUCCESS;
    }
    
    return NOT_COMPLETED;
}

void Connection::cleanUp() {
	if(cgi_ != NULL && cgi_->getFd() != -1) {
		cgi_->killCGI();
		delete cgi_;
		cgi_ = NULL;
	}
}