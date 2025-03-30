/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/17 16:40:12 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/29 16:40:17 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

#include "AHttp.hpp"
#include "HttpRequest.hpp"
#include "MainConf.hpp"
#include "CGI.hpp"

class HttpResponse : public AHttp
{
	private:
		int status_code_;
		static std::map<int, std::string> status_codes_; // ? Ahttp に移動させるべきかもしれない

	public:
		// constructor
		HttpResponse();
		~HttpResponse();

		// getter
		std::vector<std::string> getStartLine() const;
		std::map<std::string, std::string> getHeader() const;
		std::vector<char> getBody() const;

		// setter
		static void initializeStatusCodes();
		void setStartLine(int status_code);
		void setBody(std::vector<char> body);
		void setHeader(std::map<std::string, std::string> requestHeader, std::string path, std::string server_name);
		void setBadRequestHeader();
		void setStatusCode(int status_code);
		void addHeader(const std::string& key, const std::string& value);

		std::vector<char> buildResponse();

		// util
		std::string getContentType(std::string path);
		std::string getDate();
};

// ========================= util ====================================

std::vector<char> stringToVector(std::string str);
std::string vectorToString(std::vector<char> vec);

#endif
