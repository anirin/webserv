/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 18:19:08 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/27 20:29:06 by rmatsuba         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

// ==================================== constructor and destructor ====================================

HttpRequest::HttpRequest() {}

HttpRequest::~HttpRequest() {}

// todo error handling が欲しい もしもし失敗した場合は400 bad request を返す
HttpRequest::HttpRequest(std::string request, MainConf *mainConf) {
	start_line_.resize(3);
	start_line_ = parseRequestStartLine(request);
	headers_ = parseRequestHeader(request);
	body_ = parseRequestBody(request);

	std::string server_and_port = headers_["Host"];
	int pos = server_and_port.find(":");

	server_name_ = server_and_port.substr(0, pos);
	port_ = server_and_port.substr(pos + 1);
	request_path_ = start_line_[1];
	conf_value_ = mainConf->getConfValue(port_, server_name_, request_path_);
}

// ==================================== getter ====================================

std::vector<std::string> HttpRequest::getStartLine() const {
	return start_line_;
}

std::map<std::string, std::string> HttpRequest::getHeader() const {
	return headers_;
}

std::string HttpRequest::getBody() const {
	return body_;
}

std::string HttpRequest::getLocationPath() const {
	return location_path_;
}

Method HttpRequest::getMethod() const {
	std::string method = start_line_[0];
	if(method == "GET")
		return GET;
	else if(method == "POST")
		return POST;
	else if(method == "DELETE")
		return DELETE;
	return UNKNOWN;
}

std::string HttpRequest::getServerName() const {
	return server_name_;
}

std::string HttpRequest::getPort() const {
	return port_;
}

std::string HttpRequest::getRequestPath() const {
	return request_path_;
}

// ==================================== setter ====================================

void HttpRequest::setBody(std::string body) {
	body_ = body;
}

std::vector<std::string> HttpRequest::parseRequestStartLine(std::string request) {
	std::string first_line = request.substr(0, request.find("\r\n"));
	std::istringstream ss(first_line);
	std::vector<std::string> start_line;
	while(ss) {
		std::string token;
		std::getline(ss, token, ' ');
		if(!token.empty())
			start_line.push_back(token);
	}
	return start_line;
}

/* Parse Header of the request */
std::map<std::string, std::string> HttpRequest::parseRequestHeader(std::string request) {
	std::map<std::string, std::string> header;
	size_t start = request.find("\r\n") + 2;
	size_t end = request.find("\r\n\r\n");
	if(start > end)
		throw std::runtime_error("Header not found");
	std::string header_str = request.substr(start, end - start);
	std::istringstream ss(header_str);
	while(true) {
		std::string line;
		std::getline(ss, line);
		int count = 0;
		for(long unsigned int i = 0; i < line.size(); i++) {
			if(line[i] == '\r' || line[i] == '\n') {
				count++;
			}
		}
		line = line.substr(0, line.size() - count);
		std::string key = line.substr(0, line.find(":"));
		std::string value = line.substr(line.find(":") + 2);
		/* std::cout << "key: " << key << std::endl; */
		/* std::cout << "value: " << value << std::endl; */
		header[key] = value;
		if(ss.eof())
			break;
	}
	return header;
}

/* Parse Body of the request */
std::string HttpRequest::parseRequestBody(std::string request) {
	std::string body = request.substr(request.find("\r\n\r\n") + 4);
	return body;
}

// ==================================== checker ====================================

bool HttpRequest::isValidHttpVersion() {
	std::string version = start_line_[2];

	if(version != "HTTP/1.1") {
		std::cout << "[http request] invalid http version" << std::endl;
		return false;
	}
	return true;
}

/* this process needs to add the process of distinguishing http method */
bool HttpRequest::isValidHttpMethod() {
	std::string method = start_line_[0];
	std::vector<std::string> limit_except = conf_value_._limit_except;

	if(limit_except.size() == 0)
		return true;

	if(std::find(limit_except.begin(), limit_except.end(), method) == limit_except.end()) {
		std::cout << "[http request] invalid http method" << std::endl;
		return false;
	}
	return true;
}

bool HttpRequest::isValidPath() {
	/* in this process, check only the existence of the requested path */
	/* where error_page exist or not is not checked */
	/* make requested path that is based wevserv root */
	// todo location path のロジックは改良する必要あり autoindexの場合も考慮する必要あり
	// configに対応している場合はlocaiton pathには値が入る。そうではない場合は空文字列が入る
	// またautoindexがonの場合は先にautoindexの処理が入るので、そもそもgetLocationPathは呼ばれない
	location_path_ = getLocationPath(request_path_, conf_value_);
	if(location_path_ == "") {
		std::cout << "[http request] invalid path" << std::endl;
		return false;
	}
	return true;
}

int HttpRequest::getStatusCode() {
	if(!isValidHttpVersion())
		return 505;
	if(!isValidHttpMethod())
		return 405;
	if(!isValidPath())
		return 404;
	return 200;
}

// ==================================== utils ====================================

std::string HttpRequest::getLocationPath(std::string request_path, conf_value_t conf_value) {
	std::string location_path;
	struct stat st;
	/* if request_path is directory, check the existence of index file */
	std::cout << "[http request] request_path: " << request_path << std::endl;
	bool is_directory = false;
	if(request_path[request_path.size() - 1] == '/')
		is_directory = true;
	if(is_directory) {
		for(size_t i = 0; i < conf_value._index.size(); i++) {
			/* std::cout << "index: " << conf_value._index[i] << std::endl; */
			std::string index_path = conf_value._index[i];
			if(index_path[0] == '/')
				index_path = index_path.substr(1);
			location_path = "." + conf_value._root + request_path + index_path;
			std::cout << "[http request] location_path: " << location_path << std::endl;
			if(stat(location_path.c_str(), &st) == 0)
				return location_path;
		}
	} else {
		location_path = "." + conf_value._root + request_path;
		std::cout << "[http request] location_path: " << location_path << std::endl;
		if(stat(location_path.c_str(), &st) == 0)
			return location_path;
	}
	return ""; // todo throwの方がいいかも？
}
