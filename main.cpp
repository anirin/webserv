/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 13:49:54 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/02/24 11:44:23by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include "ConnectionWrapper.hpp"
#include "EpollWrapper.hpp"
#include "Listener.hpp"

std::string getConfContent() {
	std::string confPath = "src/config/sample/test.conf";
	std::ifstream ifs(confPath.c_str());
	if(!ifs) {
		throw std::runtime_error("[main.cpp] Failed to open configuration file");
	}
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	std::string content = buffer.str();
	return content;
}

int main() {
	/* Load configuration */
	std::string content = getConfContent();
	MainConf mainConf(content);

	/* Make Listener, EpollWrapper, ConnectionWrapper */
	Listener listener(8080);
	EpollWrapper epollWrapper(100);
	ConnectionWrapper connections;
	epollWrapper.addEvent(listener.getFd());

	/* Main loop */
	while(true) {
		FileStatus file_status;
		// 空いているfdを全てちょっとづつ読み込む この機構が不要な場合
		// fileを開いた瞬間に読み込めばいい
		std::vector<Connection *> conns = connections.getConnections();
		for(std::vector<Connection *>::iterator it = conns.begin(); it != conns.end(); ++it) {
			if((*it)->getStaticFd() == -1)
				continue;
			file_status = (*it)->readStaticFile();
			if(file_status == ERROR) {
				epollWrapper.deleteEvent((*it)->getFd());
				close((*it)->getFd());
				std::cout << "[main.cpp] Error: connection closed" << std::endl;
			}
			if(file_status == SUCCESS) {
				epollWrapper.setEvent((*it)->getFd(), EPOLLOUT);
				std::cout << "[main.cpp] connection status cleared and closed" << std::endl;
			}
		}

		int nfds = epollWrapper.epwait();
		for(int i = 0; i < nfds; ++i) {
			std::cout << std::endl << "[main.cpp] epoll for-loop ===[" << i << "]===" << std::endl;
			struct epoll_event current_event = epollWrapper[i];
			int target_fd = current_event.data.fd;
			if(target_fd == listener.getFd()) {
				try {
					Connection *newConn = new Connection(listener.getFd());
					epollWrapper.addEvent(newConn->getFd());
					connections.addConnection(newConn);
				} catch(const std::exception &e) {
					std::cerr << "[main.cpp] Error: Accept failed: " << e.what() << std::endl;
				}
			} else {
				Connection *conn = connections.getConnection(target_fd);

				if(!conn) {
					std::cerr << "[main.cpp] Error: Connection not found" << std::endl;
					continue;
				}
				if(conn->isTimedOut(&mainConf)) {
					epollWrapper.deleteEvent(target_fd);
					connections.removeConnection(target_fd);
					continue;
				}

				FileTypes type = conn->getFdType(target_fd);
				switch(type) {
					case SOCKET:
						if(current_event.events & EPOLLIN) {
							file_status = conn->readSocket(&mainConf);
							if(file_status == ERROR) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] Error: connection closed" << std::endl;
							}
							if(file_status == SUCCESS_CGI) {
								epollWrapper.addEvent(conn->getCGI()->getFd());
								std::cout << "[main.cpp] CGI event add to epoll" << std::endl;
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set "
											 "to EPOLLOUT"
										  << std::endl;
							}
							if(file_status == CLOSED) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] connection closed" << std::endl;
							}
							// not completed
						} else if(current_event.events & EPOLLOUT) {
							file_status = conn->writeSocket();
							if(file_status == ERROR) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] Error: connection closed" << std::endl;
							}
							if(file_status == SUCCESS) {
								epollWrapper.setEvent(target_fd, EPOLLIN);
								std::cout << "[main.cpp] connection event set "
											 "to EPOLLIN"
										  << std::endl;
							}
							// not completed
						}
						break;
					case PIPE:
						conn->readCGI();
						if(file_status == ERROR) {
							epollWrapper.deleteEvent(conn->getFd());
							connections.removeConnection(target_fd);
							close(target_fd);
							std::cout << "[main.cpp] connection closed" << std::endl;
						} else if(file_status == SUCCESS) {
							epollWrapper.deleteEvent(target_fd);
							epollWrapper.setEvent(conn->getFd(),
												  EPOLLOUT); //  本来はSUCCESS後ではなく、cgi
															 //  開始した後に書き込むべき time out
															 //  が発生する可能性があるので悩ましい
							close(target_fd);
							std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
						}
						// not completed
						break;
					default:
						break;
				}
			}
		}
	}
}
