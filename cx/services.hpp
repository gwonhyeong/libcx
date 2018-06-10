/**
 */
#ifndef __cx_io_services_h__
#define __cx_io_services_h__

#include <cx/io/io.hpp>
#include <cx/io/basic_object.hpp>
#include <cx/io/basic_buffer.hpp>

#include <cx/io/ip/basic_address.hpp>
#include <cx/io/ip/option.hpp>
#include <cx/io/ip/basic_socket.hpp>
#include <cx/io/ip/basic_acceptor.hpp>

#include <cx/io/detail/completion_port.hpp>
#include <cx/io/detail/completion_port_socket_service.hpp>

namespace cx::io {

#if CX_PLATFORM == CX_P_WINDOWS
	using basic_implementation = cx::io::detail::completion_port;

	namespace ip::tcp {
		using service = cx::io::ip::detail::completion_port_socket_service<SOCK_STREAM, IPPROTO_TCP>;
		using address = typename service::address_type;
		using socket = cx::io::ip::basic_socket<service>;
		using buffer = typename service::buffer_type;
		using acceptor = cx::io::ip::basic_acceptor<service>;
	}
	namespace ip::udp {
		using service = cx::io::ip::detail::completion_port_socket_service<SOCK_DGRAM, IPPROTO_UDP>;
		using address = typename service::address_type;
		using socket = cx::io::ip::basic_socket<service>;
		using buffer = typename service::buffer_type;
	}
#elif CX_PLATFORM == CX_P_LINUX
	using basic_implementation = cx::io::detail::epoll;
#elif CX_PLATFORM == CX_P_MACOSX
	using basic_implementation = cx::io::detail::kqueue;
#else
	using basic_implementation = cx::io::detail::poll;
#endif

}

#include <cx/time/basic_timer.hpp>
#include <cx/time/detail/win32_timer_queue_service.hpp>

namespace cx::time {

#if CX_PLATFORM == CX_P_WINDOWS
	using timer_service = cx::time::detail::win32_timer_queue_service< cx::io::basic_implementation, 100 >;
#elif CX_PLATFORM == CX_P_LINUX

#elif CX_PLATFORM == CX_P_MACOSX 

#else 

#endif
	using timer = cx::time::basic_timer< timer_service >;

}

#endif