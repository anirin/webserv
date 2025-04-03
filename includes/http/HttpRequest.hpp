/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 18:19:21 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/03 20:08:56 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <stdexcept>

#include "AHttp.hpp"
#include "MainConf.hpp"

// enum
enum Method
{
	GET,
	POST,
	DELETE,
	UNKNOWN
};

class HttpRequest : public AHttp
{
private:
	std::string server_name_;
	std::string port_;
	std::string request_path_;
	std::string location_path_;
	conf_value_t conf_value_;
	std::map<std::string, std::string> query_params_;


public:
	HttpRequest();

	// constructor
	HttpRequest(std::vector<char> request, MainConf *mainConf);
	~HttpRequest();

	// setter
	void setStatusCode();
	void setBody(std::vector<char> body);

	// parse
	std::vector<std::string> parseRequestStartLine(std::vector<char> request);
	std::map<std::string, std::string> parseRequestHeader(std::vector<char> request);
	std::vector<char> parseRequestBody(std::vector<char> request, std::map<std::string, std::string> headers);

	// getter
	std::vector<std::string> getStartLine() const;
	std::map<std::string, std::string> getHeader() const;
	std::vector<char> getBody() const;
	std::string getQueryString() const;

	std::string getLocationPath() const;
	Method getMethod() const;
	std::string getServerName() const;
	std::string getPort() const;
	std::string getRequestPath() const;
	int getStatusCode();

	// checker
	bool isValidHttpVersion();
	bool isValidHttpMethod();
	bool isValidPath();

	// utils
	std::string getLocationPath(std::string request_path, conf_value_t conf_value);
};

#endif
