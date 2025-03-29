/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   AHttp.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 18:29:07 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/29 12:04:15 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef AHTTP_HPP
#define AHTTP_HPP

#include <map>
#include <string>
#include <vector>

class AHttp
{
protected:
	std::vector<std::string> start_line_;
	std::map<std::string, std::string> headers_;
	std::vector<char> body_;

public:
	AHttp();
	virtual ~AHttp() = 0;
	virtual std::vector<std::string> getStartLine() const = 0;
	virtual std::map<std::string, std::string> getHeader() const = 0;
	virtual std::vector<char> getBody() const = 0;
};

#endif
