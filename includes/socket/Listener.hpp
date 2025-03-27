/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/12 22:44:59 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/27 12:04:35 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
#define LISTENER_HPP

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#include "ASocket.hpp"

class Listener : public ASocket
{
private:

public:
	Listener();
	Listener(int port);
	~Listener();
	int getFd() const;
};

#endif
