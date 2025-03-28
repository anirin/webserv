/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/17 17:52:45 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/29 16:39:14 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpResponse.hpp"

std::map<int, std::string> HttpResponse::status_codes_;

/* Make map data of status codes and messages */
void HttpResponse::initializeStatusCodes() {
	status_codes_[200] = "OK";
	status_codes_[400] = "Bad Request";
	status_codes_[404] = "Not Found";
	status_codes_[405] = "Method Not Allowed";
	status_codes_[414] = "URI Too Long";
	status_codes_[413] = "Content Too Long";
	status_codes_[501] = "Not Implemented";
	status_codes_[505] = "HTTP Version Not Supported";
}

// ==================================== constructor and destructor ====================================

HttpResponse::HttpResponse() {
	start_line_.resize(3);
	initializeStatusCodes();
}

HttpResponse::~HttpResponse() {}

// ==================================== setter ====================================

/* Set the start line of the response */
void HttpResponse::setStartLine(int status_code) {
	std::ostringstream oss;
	oss << status_code;
	start_line_[0] = "HTTP/1.1";
	start_line_[1] = oss.str();
	start_line_[2] = status_codes_[status_code];
}

/* Set the header of the response */
void HttpResponse::setHeader(std::map<std::string, std::string> requestHeader, std::string path,
							 std::string server_name) {
	headers_["Date"] = getDate();
	headers_["Server"] = server_name;
	if(path == "")
		headers_["Content-Type"] = "text/html";
	else
		headers_["Content-Type"] = getContentType(path);
	headers_["Content-Language"] = requestHeader["Accept-Language"];
	if(requestHeader["Keep-Alive"] != "none")
		headers_["Keep-Alive"] = "timeout=5, max=100";
	if(requestHeader["Connection"] != "")
		headers_["Connection"] = requestHeader["Connection"];
	headers_["Connection"] = requestHeader["Connection"];
	std::ostringstream ss;
	ss << body_.size();
	headers_["Content-Length"] = ss.str();
	if(requestHeader["Location"] != "")
		headers_["Location"] = requestHeader["Location"];
}

void HttpResponse::setBadRequestHeader() {
	headers_["Date"] = getDate();
	headers_["Content-Type"] = "text/html";
	headers_["Content-Length"] = "0";
	std::ostringstream ss;
	ss << body_.size();
	headers_["Content-Length"] = ss.str();
}

void HttpResponse::setBody(std::vector<char> buff) {
	body_ = buff;
}

std::vector<char> HttpResponse::buildResponse() {
	std::string header = start_line_[0] + " " + start_line_[1] + " " + start_line_[2] + "\r\n";
	for(std::map<std::string, std::string>::iterator it = headers_.begin(); it != headers_.end(); ++it) {
		header += it->first + ": " + it->second + "\r\n";
	}
	header += "\r\n";

	std::vector<char> response;

	response = stringToVector(header);

	for(size_t i = 0; i < body_.size(); ++i) {
		response.push_back(body_[i]);
	}

	return response;
}

void HttpResponse::setStatusCode(int status_code) {
	status_code_ = status_code;
}

// ==================================== getter ====================================

std::vector<std::string> HttpResponse::getStartLine() const {
	return start_line_;
}

std::map<std::string, std::string> HttpResponse::getHeader() const {
	return headers_;
}

std::vector<char> HttpResponse::getBody() const {
	return body_;
}

// ==================================== utils ====================================

/* make date data for response header */
std::string HttpResponse::getDate() {
	std::time_t now = std::time(NULL);
	std::string date = std::ctime(&now);
	std::vector<std::string> date_vec;
	std::stringstream ss(date);
	char space = ' ';
	while(getline(ss, date, space)) {
		if(!date.empty() && date != "\n")
			date_vec.push_back(date);
	}
	date_vec[4] = date_vec[4].substr(0, date_vec[4].size() - 1);
	std::string date_str =
		date_vec[0] + ", " + date_vec[2] + " " + date_vec[1] + " " + date_vec[4] + " " + date_vec[3] + " GMT";
	return date_str;
}

std::string HttpResponse::getContentType(std::string path) {
	// text : html, php
	// application : json
	// image : jpg, jpeg, png
	// video : mov, mp4
	// audio : mp3
	std::string extension = path.substr(path.find_last_of(".") + 1);
	if(extension == "html")
		return "text/html";
	else if(extension == "php")
		return "text/html";
	else if(extension == "json")
		return "application/json";
	else if(extension == "jpg")
		return "image/jpeg";
	else if(extension == "jpeg")
		return "image/jpeg";
	else if(extension == "png")
		return "image/png";
	else if(extension == "mov")
		return "video/quicktime";
	else if(extension == "mp4")
		return "video/mp4";
	else if(extension == "mp3")
		return "audio/mpeg";
	else
		return "text/html";
}
