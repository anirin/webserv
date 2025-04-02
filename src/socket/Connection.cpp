/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/02 18:44:22 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 5;
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

	is_timeout_ = false;
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
	// if (cgi_ != NULL) {
	// 	delete cgi_;
	// 	cgi_ = NULL;
	// }
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

bool Connection::getIsTimeout() const {
	return is_timeout_;
}

// ==================================== setter ====================================

void Connection::setHttpRequest(MainConf *mainConf) { // throw
	try {
		request_ = new HttpRequest(rbuff_, mainConf);
	} catch(const std::exception &e) {
		// ここでエラーが発生している, telnetを使っている時
		throw std::runtime_error(e.what());
	}

	try {
		conf_value_ =
			mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	} catch(const std::exception &e) { throw std::runtime_error(e.what()); }
	std::cout << "[connection] request is set" << std::endl;
	//  std::cout << rbuff_ << std::endl;

	// mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

bool Connection::isCGI() {
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();

	if(path_size <= 4 || location_path.substr(path_size - 4, 4) != ".php") {
		return false;
	}
	return true;
}

void Connection::executeCGI() { // throw
	CGI *cgi = NULL;

	std::string script_path = request_->getLocationPath();
	std::string query_string = request_->getQueryString();
	std::map<std::string, std::string> headers = request_->getHeader();
	std::vector<char> body = request_->getBody();

	Method method = request_->getMethod();
	if(method == POST) {
		cgi = new CGI(script_path, query_string, body, headers);
	} else {
		cgi = new CGI(script_path, query_string, headers);
	}

	if(cgi == NULL) {
		std::cerr << "[connection] Failed to create CGI object" << std::endl;
		throw std::runtime_error("Failed to create CGI object");
	}

	// CGIを実行し、パイプFDを設定
	try {
		cgi->execute();
	} catch(const std::exception &e) {
		delete cgi;
		throw std::runtime_error(std::string("[connection] CGI execution failed: ") + e.what());
	}

	cgi_ = cgi;
	std::cout << "[connection] CGI is set, fd=" << cgi_->getFd() << std::endl;
	return;
}

void Connection::clearValue() {
	delete response_;
	delete request_;
	response_ = NULL;
	request_ = NULL;

	rbuff_.clear();
	wbuff_.clear();

	lastActive_ = std::time(NULL);
}

// ==================================== check ==============================================

int Connection::isTimedOut() {
	if(is_timeout_) {
		std::cout << "[connection] already timed out" << std::endl;
		return 0;
	}

	std::time_t now = std::time(NULL);
	std::time_t diff = now - lastActive_;
	if(diff <= timeout_) {
		return 0;
	}

	std::cout << "[connection] time out" << std::endl;

	// ケース1: keep alive の待ち時間でtime outする場合
	// 単に切断する（status code は出さない）
	if(request_ == NULL && rbuff_.empty()) {
		std::cout << "[connection] keep-alive timeout - closing connection" << std::endl;
		is_timeout_ = true;
		return 1;
	}

	// ケース2: request が長時間かけて送られる場合 408 Request Timeout
	if(request_ == NULL && !rbuff_.empty()) {
		std::cout << "[connection] request timeout - sending 408" << std::endl;
		setHttpResponse();
		response_->setStatusCode(408);
		setErrorFd(408);
		buildStaticFileResponse(408);
		is_timeout_ = true;
		return 2;
	}

	// ケース3: EPOLLOUTの設定の場合（requestは正常に処理できたが対応できない場合）504
	if(cgi_ != NULL) {
		delete cgi_;
		cgi_ = NULL;
	}
	std::cout << "[connection] gateway timeout - sending 504" << std::endl;
	setHttpResponse();
	response_->setStatusCode(504);
	setErrorFd(504);
	buildStaticFileResponse(504);
	is_timeout_ = true;
	return 2;
}

// ==================================== read and write ====================================

FileStatus Connection::readSocket(MainConf *mainConf) {
	std::cout << "[connection] started to read socket" << std::endl;
	char buff[buff_size];
	ssize_t rlen = recv(fd_, buff, buff_size, 0);

	if(rlen < 0) {
		return ERROR;
	} else if(rlen == 0) {
		std::cout << "[connection] read socket closed by client" << std::endl;
		return CLOSED;
	} else if(rlen == buff_size) {
		rbuff_.insert(rbuff_.end(), buff, buff + rlen);
		/* for(ssize_t i = 0; i < buff_size; i++) { */
		/* 	rbuff_.push_back(buff[i]); */
		/* } */
		return NOT_COMPLETED;
	}

	/* for(ssize_t i = 0; i < rlen; i++) { */
	/* 	rbuff_.push_back(buff[i]); */
	/* } */
	// Getのチェック
	rbuff_.insert(rbuff_.end(), buff, buff + rlen);
	std::string request_str(rbuff_.begin(), rbuff_.end());
	size_t pos = request_str.find("\r\n\r\n");
	if(pos == std::string::npos) {
		return NOT_COMPLETED;
	}
	// POSTの場合はContent-Lengthを見て、bodyを読み込む
	if(request_str.find("Content-Length") != std::string::npos && request_str.find("POST") != std::string::npos) {
		std::istringstream iss(request_str);
		std::string line;
		size_t content_length;
		while(std::getline(iss, line)) {
			if(line.find("Content-Length") != std::string::npos) {
				std::string content_length_str = line.substr(strlen("Content-Length: "));
				std::stringstream ss(content_length_str);
				ss >> content_length;
				break;
			}
		}
		// トータルのサイズを計算して、まだ読み込むべきデータがあるか確認
		size_t total_length = pos + 4 + content_length;
		if(rbuff_.size() < total_length) {
			return NOT_COMPLETED;
		}
	}
	return processAfterReadCompleted(mainConf);
}

FileStatus Connection::readStaticFile(std::string file_path) {
	struct stat path_stat;
	if(stat(file_path.c_str(), &path_stat) == 0) {
		if(S_ISDIR(path_stat.st_mode)) {
			std::cerr << "[connection] path is a directory" << std::endl;
			setErrorFd(404);
			buildStaticFileResponse(404);
			return SUCCESS_STATIC;
		}
	}

	std::ifstream ifs(file_path.c_str(), std::ios::binary); // バイナリモード推奨
	if(!ifs) {
		std::cerr << "[connection] open file failed" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	// ファイル全体を一度に読み込み
	wbuff_.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	if(ifs.bad()) {
		std::cerr << "[connection] read error" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	ifs.close();
	buildStaticFileResponse(200);

	return SUCCESS_STATIC;
}

FileStatus Connection::readCGI() {
	char buff[buff_size];
	std::cout << "[read cgi] started to read CGI" << std::endl;
	ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

	if(rlen < 0) {
		std::cerr << "[read cgi] read pipe failed" << std::endl;
		delete cgi_;
		cgi_ = NULL;
		return ERROR;
	}

	// データを読み取れた場合はバッファに追加
	std::cout << "[read cgi] read CGI: " << rlen << std::endl;
	std::cout << "[read cgi] read CGI: " << rlen << std::endl;
	if(rlen > 0) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
	}

	// CGIプロセスの状態を確認
	int status;
	pid_t result = waitpid(cgi_->getPid(), &status, WNOHANG);

	if(result == 0) {
		// プロセスがまだ実行中
		std::cout << "[read cgi] CGI process still running" << std::endl;
		return NOT_COMPLETED;
	} else if(result < 0) {
		// エラー
		std::cerr << "[read cgi] waitpid error: " << strerror(errno) << std::endl;
		delete cgi_;
		cgi_ = NULL;
		return ERROR;
	}

	// プロセスが終了した場合
	std::cout << "[read cgi] CGI process completed" << std::endl;

	// 残りのデータがあれば読み取る
	while((rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1)) > 0) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
	}

	std::cout << "[read cgi] wbuff size: " << wbuff_.size() << std::endl;
	if(!wbuff_.empty()) {
		std::cout << "[read cgi] wbuff preview: " << std::string(wbuff_.begin(), wbuff_.begin() + std::min(wbuff_.size(), size_t(50))) << "..." << std::endl;
	}

		buildStaticFileResponse(200);
	std::cout << "[read cgi] read CGI completed" << std::endl;
	return SUCCESS;
}

FileStatus Connection::writeSocket() {
	char buff[buff_size];

	/* if(!request_) { */
	/* 	std::cerr << "[connection] No request found" << std::endl; */
	/* 	return ERROR; */
	/* } */

	if(wbuff_.empty()) {
		return NOT_COMPLETED;
	}

	// std::cout << " >>>>> " << wbuff_.data() << " <<<<< " << std::endl; // デバッグ用

	ssize_t copy_len = std::min(wbuff_.size(), static_cast<std::size_t>(buff_size));
	std::memcpy(buff, wbuff_.data(), copy_len);
	if(copy_len != buff_size)
		buff[copy_len] = '\0';
	wbuff_.erase(wbuff_.begin(), wbuff_.begin() + copy_len);
	ssize_t wlen = send(fd_, buff, copy_len, 0);
	if(wlen == -1)
		return ERROR;
	if(wlen == buff_size)
		return NOT_COMPLETED;
	delete response_;
	if(request_)
		delete request_;
	response_ = NULL;
	request_ = NULL;
	std::cout << "[connection] write socket completed" << std::endl;
	return SUCCESS;
}

void Connection::cleanUp() {
	if(cgi_ != NULL && cgi_->getFd() != -1) {
		delete cgi_;
		cgi_ = NULL;
	}
}

// ==================================== utils ====================================
std::vector<char> stringToVector(std::string str) {
	std::vector<char> vec(str.begin(), str.end());
	return vec;
}

std::string vectorToString(std::vector<char> vec) {
	std::string str(vec.begin(), vec.end());
	return str;
}
