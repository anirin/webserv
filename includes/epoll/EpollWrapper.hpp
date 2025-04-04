/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EpollWrapper.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 14:08:59 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/04 13:21:21 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EPOLLWRAPPER_HPP
#define EPOLLWRAPPER_HPP

#include <sys/epoll.h>
#include <stdexcept>
#include <vector>
#include <unistd.h>
#include <cstring>

class EpollWrapper
{
private:
	int epfd_;
	size_t max_events_;
	std::vector<struct epoll_event> events_list_;
	EpollWrapper();

public:
	EpollWrapper(int max_events);
	~EpollWrapper();
	int getEpfd() const;
	std::vector<struct epoll_event> getEventsList() const;
	void addEvent(int fd);
	void deleteEvent(int fd);
	int epwait();
	struct epoll_event &operator[](size_t index);
	void setEvent(int fd, uint32_t events);
};

#endif
