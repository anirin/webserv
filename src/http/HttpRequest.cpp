/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 18:19:08 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/29 11:11:43y atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

// ==================================== constructor and destructor ====================================

HttpRequest::HttpRequest() {}

HttpRequest::~HttpRequest() {}

// todo error handling が欲しい もしもし失敗した場合は400 bad request を返す
HttpRequest::HttpRequest(std::vector<char> request, MainConf *mainConf) {
	start_line_.resize(3);
	try {
		start_line_ = parseRequestStartLine(request);
		headers_ = parseRequestHeader(request);
		body_ = parseRequestBody(request, headers_);
	} catch(const std::exception &e) { throw std::runtime_error(e.what()); }

	std::string server_and_port = headers_["Host"];
	int pos = server_and_port.find(":");

	server_name_ = server_and_port.substr(0, pos);
	port_ = server_and_port.substr(pos + 1);
	// todo クエリパラメーターの処理を追加
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

std::vector<char> HttpRequest::getBody() const {
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

void HttpRequest::setBody(std::vector<char> buff) {
	body_ = buff;
}

std::vector<std::string> HttpRequest::parseRequestStartLine(std::vector<char> request) {
	std::string first_line;

	// 最初の行を抽出
	for(size_t i = 0; i < request.size(); ++i) {
		if(request[i] == '\r' && i + 1 < request.size() && request[i + 1] == '\n') {
			first_line = std::string(request.begin(), request.begin() + i);
			break;
		}
	}

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
std::map<std::string, std::string> HttpRequest::parseRequestHeader(std::vector<char> request) { // throw
	std::map<std::string, std::string> header;

	// ヘッダーの開始と終了位置を検索
	size_t start = 0;
	size_t end = 0;

	// 最初の\r\nを検索（リクエストラインの終わり）
	for(size_t i = 0; i < request.size() - 1; ++i) {
		if(request[i] == '\r' && request[i + 1] == '\n') {
			start = i + 2;
			break;
		}
	}

	// \r\n\r\nを検索（ヘッダーの終わり）
	for(size_t i = start; i < request.size() - 3; ++i) {
		if(request[i] == '\r' && request[i + 1] == '\n' && request[i + 2] == '\r' && request[i + 3] == '\n') {
			end = i;
			break;
		}
	}

	if(start >= end || start == 0)
		throw std::runtime_error("Header not found");

	std::string header_str(request.begin() + start, request.begin() + end);
	std::istringstream ss(header_str);

	std::string line;
	while(std::getline(ss, line)) {
		// 末尾の\r\nを削除
		while(!line.empty() && (line[line.length() - 1] == '\r' || line[line.length() - 1] == '\n')) {
			line = line.substr(0, line.length() - 1);
		}

		if(line.empty())
			break;

		size_t colon_pos = line.find(':');
		if(colon_pos != std::string::npos) {
			std::string key = line.substr(0, colon_pos);
			std::string value = line.substr(colon_pos + 2);
			header[key] = value;
		}
	}
	return header;
}

/* Parse Body of the request */
std::vector<char> HttpRequest::parseRequestBody(std::vector<char> request, std::map<std::string, std::string> headers) {
	// ボディの開始位置を検索
	size_t body_start = 0;
	// \r\n\r\nを検索 （ボディの開始位置）
	for(size_t i = 0; i < request.size() - 3; ++i) {
		if(request[i] == '\r' && request[i + 1] == '\n' && request[i + 2] == '\r' && request[i + 3] == '\n') {
			body_start = i + 4;
			break;
		}
	}

	if(body_start == 0)
		return std::vector<char>();

	// Transfer-Encoding: chunked の処理
	std::map<std::string, std::string>::iterator te_it = headers.find("Transfer-Encoding");
	if(te_it != headers.end() && te_it->second == "chunked") {
		std::vector<char> body;
		size_t pos = body_start;
		bool final_chunk_received = false;

		while(pos < request.size()) {
			// チャンクサイズを読み取り
			std::string size_str;
			while(pos < request.size() && request[pos] != '\r') {
				size_str += request[pos];
				++pos;
			}

			// チャンクヘッダの終わりを確認(\r\nとなっているか)
			if(pos + 1 >= request.size() || request[pos + 1] != '\n') {
				throw std::runtime_error("Invalid chunk header format");
			}

			// \r\nをスキップ
			pos += 2;

			// 16進数のチャンクサイズを変換
			std::istringstream iss(size_str);
			size_t chunk_size;

			// 16進数の解析に失敗した場合のチェック
			if(!(iss >> std::hex >> chunk_size)) {
				throw std::runtime_error("Invalid chunk size format");
			}

			if(chunk_size == 0) {
				final_chunk_received = true;
				break;
			}

			// チャンクデータが実際のリクエストサイズを超えていないか確認
			if(pos + chunk_size > request.size()) {
				throw std::runtime_error("Incomplete chunk data");
			}

			// チャンクデータを追加
			for(size_t i = 0; i < chunk_size; ++i) {
				body.push_back(request[pos++]);
			}

			// チャンク末尾の\r\nを確認してスキップ
			if(pos + 1 >= request.size() || request[pos] != '\r' || request[pos + 1] != '\n') {
				throw std::runtime_error("Missing chunk delimiter");
			}
			pos += 2;
		}

		// 最終チャンク（サイズ0）が受信されたことを確認
		if(!final_chunk_received) {
			throw std::runtime_error("Chunked transfer missing final chunk");
		}

		return body;
	}

	// Content-Length の処理
	std::map<std::string, std::string>::iterator cl_it = headers.find("Content-Length");
	if(cl_it != headers.end()) {
		std::istringstream iss(cl_it->second);
		size_t content_length;
		iss >> content_length;

		if(body_start + content_length <= request.size()) {
			return std::vector<char>(request.begin() + body_start, request.begin() + body_start + content_length);
		} else {
			throw std::runtime_error("Content-Length does not match actual request size");
		}
	}

	return std::vector<char>();
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
	// configに対応している場合はlocaiton pathには値が入る。そうではない場合は空文字列が入る
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
		if(conf_value._index.size() == 0) {
			location_path = "." + conf_value._root + request_path;
			if (stat(location_path.c_str(), &st) == 0)
				return location_path;
			else {
				std::cout << "[http request] dir not found" << std::endl;
				return "";
			}
		}
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
