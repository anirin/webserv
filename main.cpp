/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 13:49:54 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/26 14:08:03 by rmatsuba         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include "ConnectionWrapper.hpp"
#include "EpollWrapper.hpp"
#include "Listener.hpp"

std::string getConfContent(char *confPath) {
	if(confPath) {
		std::ifstream ifs(confPath);
		if(!ifs) {
			throw std::runtime_error("[main.cpp] Failed to open configuration file");
		}
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		std::string content = buffer.str();
		return content;
	}

	// todo 削除
	std::string defaultPath = "src/config/sample/test.conf";
	std::ifstream ifs(defaultPath.c_str());
	if(!ifs) {
		throw std::runtime_error("[main.cpp] Failed to open configuration file");
	}
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	std::string content = buffer.str();
	return content;
}

int main(int argc, char **argv) {
	if(argc != 1 && argc != 2) {
		std::cerr << "./webserv {your conf path} or ./webserv" << std::endl;
		return 1;
	}

	char *confPath = NULL;
	if(argc == 2) {
		confPath = argv[1];
	}

	/* Load configuration */
	std::string content = getConfContent(confPath);
	MainConf mainConf(content);

	/* Make Listener, EpollWrapper, ConnectionWrapper */
	Listener listener(8080);
	EpollWrapper epollWrapper(100);
	ConnectionWrapper connections;
	epollWrapper.addEvent(listener.getFd());

	/* Main loop */
	while(true) {
		FileStatus file_status;

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
					close(target_fd);
					continue;
				}
				FileTypes type = conn->getFdType(target_fd);
				switch(type) {
					case SOCKET:
						if(current_event.events & EPOLLIN) {
							file_status = conn->readSocket(&mainConf);
							if(file_status == ERROR || file_status == CLOSED) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] Error: connection closed" << std::endl;
							}
							if(file_status == SUCCESS_STATIC) {
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
							}
							if(file_status == SUCCESS_CGI) {
								epollWrapper.addEvent(conn->getCGI()->getFd());
								std::cout << "[main.cpp] CGI event add to epoll" << std::endl;
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
							}
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
								std::cout << "[main.cpp] connection event set to EPOLLIN" << std::endl;
								// todo 初期化すべき部分は別にあるかも
								conn->clearValue();
							}
						}
						break;
					case PIPE:
						conn->readCGI();
						if(file_status == ERROR) {
							epollWrapper.deleteEvent(target_fd);
							close(target_fd);
							epollWrapper.deleteEvent(conn->getFd());
							connections.removeConnection(conn->getFd());
							close(conn->getFd());
							std::cout << "[main.cpp] connection closed" << std::endl;
						} else if(file_status == SUCCESS) {
							epollWrapper.deleteEvent(target_fd);
							close(target_fd);
							epollWrapper.setEvent(
								conn->getFd(),
								EPOLLOUT); // 本来はSUCCESS後ではなく、cgi 開始した後に書き込むべき time outも考慮
							std::cout << "[main.cpp] coection event set to EPOLLOUT" << std::endl;
						}
						break;
					default:
						break;
				}
			}
		}
	}
}
