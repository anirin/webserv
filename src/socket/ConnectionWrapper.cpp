/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConnectionWrapper.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/26 22:49:41 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/02 15:38:15 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConnectionWrapper.hpp"

ConnectionWrapper::ConnectionWrapper() {
	connections_.clear();
	connections_ = std::vector<Connection *>();
}

ConnectionWrapper::~ConnectionWrapper() {
	int i = 0;
	while(connections_[i]) {
		delete connections_[i];
		i++;
	}
}

std::vector<Connection *> ConnectionWrapper::getConnections() const {
	return connections_;
}

/* Get the connection with the given file descriptor */
Connection *ConnectionWrapper::getConnection(int fd) const {
	for(unsigned int i = 0; i < connections_.size(); i++) {
		if(connections_[i]->getFd() == fd ||
		   (connections_[i]->getCGI() != NULL && connections_[i]->getCGI()->getFd() == fd))
			return connections_[i];
	}
	return NULL;
}

/* Remove the connection with the given file descriptor */
void ConnectionWrapper::removeConnection(int fd) {
	for(unsigned int i = 0; i < connections_.size(); i++) {
		if(connections_[i]->getFd() == fd) {
			delete connections_[i];
			connections_.erase(connections_.begin() + i);
			return;
		}
	}
}

/* Add a connection to the connection list */
void ConnectionWrapper::addConnection(Connection *conn) {
	connections_.push_back(conn);
}
