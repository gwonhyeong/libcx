/**
 */
#ifndef __cx_io_detail_completion_port_socket_h__
#define __cx_io_detail_completion_port_socket_h__

#include <cx/cxdefine.hpp>
#include <cx/io/ip/basic_address.hpp>
#include <cx/io/basic_buffer.hpp>
#include <cx/io/detail/completion_port.hpp>
#include <cx/io/detail/basic_connect_op.hpp>
#include <cx/io/detail/basic_read_op.hpp>
#include <cx/io/detail/basic_write_op.hpp>

#if CX_PLATFORM == CX_P_WINDOWS

namespace cx::io::ip::detail {

	template < int Type, int Proto >
	class basic_completion_port_socket_service {
	protected:
		~basic_completion_port_socket_service(void) {}
	public:
		using implementation_type = cx::io::detail::completion_port;
		using handle_type = cx::io::detail::completion_port::handle_type;
		using address_type = cx::io::ip::basic_address< struct sockaddr_storage, Type, Proto >;
		using operation_type = typename implementation_type::operation_type;

		bool open(handle_type handle, const address_type& address) {
			close(handle);
			handle->fd.s = ::WSASocketW(address.family()
				, address.type()
				, address.proto()
				, nullptr
				, 0
				, WSA_FLAG_OVERLAPPED);
			if (handle->fd.s != INVALID_SOCKET)
				return true;
			return false;
		}

		void close(handle_type handle) {
			::closesocket(handle->fd.s);
			handle->fd.s = INVALID_SOCKET;
		}

		bool connect(handle_type handle, const address_type& address) {
			if (::connect(handle->fd.s, address.sockaddr(), address.length()) == 0)
				return true;
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return true;
			//if ( errno == EINPROGRESS )
			//    return true;
			return false;
		}

		bool bind(handle_type handle, const address_type& address) {
			return ::bind(handle->fd.s, address.sockaddr(), address.length()) != SOCKET_ERROR;
		}

		int poll(handle_type handle
			, int ops
			, const std::chrono::milliseconds& ms)
		{
			WSAPOLLFD pollfd = { 0 };
			pollfd.fd = handle->fd.s;
			if (ops & cx::io::ops::read) {
				pollfd.events = POLLRDNORM;
			}
			if (ops & cx::io::ops::write) {
				pollfd.events |= POLLWRNORM;
			}
			if (SOCKET_ERROR == WSAPoll(&pollfd, 1, static_cast<int>(ms.count()))) {
				std::error_code ec(WSAGetLastError(), cx::windows_category());
				std::string msg = ec.message();
				return -1;
			}
			ops = 0;
			if (pollfd.revents & POLLRDNORM) {
				ops = cx::io::ops::read;
			}
			if (pollfd.revents & POLLWRNORM) {
				ops |= cx::io::ops::write;
			}
			return ops;
		}

		address_type local_address(handle_type handle) const {
			address_type addr;
			::getsockname(handle->fd.s, addr.sockaddr(), addr.length_ptr());
			return addr;
		}

		address_type remote_address(handle_type handle) const {
			address_type addr;
			::getpeername(handle->fd.s, addr.sockaddr(), addr.length_ptr());
			return addr;
		}

		template < typename T >
		bool set_option(handle_type handle, T&& opt) {
			return opt.set(handle->fd.s);
		}

		template < typename T >
		bool get_option(handle_type handle, T&& opt) {
			return opt.get(handle->fd.s);
		}

		bool good(handle_type handle) {
			if (handle) {
				return handle->fd.s != INVALID_SOCKET;
			}
			return false;
		}
	private:
	};

	template < int Type, int Proto >
	class completion_port_socket_service;

	template <> class completion_port_socket_service< SOCK_STREAM, IPPROTO_TCP >
		: public basic_completion_port_socket_service < SOCK_STREAM, IPPROTO_TCP > {
	public:
		using this_type = completion_port_socket_service< SOCK_STREAM, IPPROTO_TCP >;
		using buffer_type = cx::io::buffer;
		using basic_completion_port_socket_service < SOCK_STREAM, IPPROTO_TCP >::connect;
		template < typename HandlerType >
		using connect_op = basic_connect_op< this_type, HandlerType>;
		template < typename HandlerType >
		using read_op = basic_read_op< this_type, HandlerType>;
		template < typename HandlerType >
		using write_op = basic_write_op< this_type, HandlerType>;

		completion_port_socket_service(implementation_type& impl)
			: _implementation(impl)
		{}
		implementation_type& implementation(void) {
			return _implementation;
		}

		int write(handle_type handle, const buffer_type& buf) {
			return send(handle->fd.s, static_cast<const char*>(buf.base()), buf.length(), 0);
		}

		int read(handle_type handle, buffer_type& buf) {
			return recv(handle->fd.s, static_cast<char*>(buf.base()), buf.length(), 0);
		}

		bool listen(handle_type handle, int backlog) {
			return ::listen(handle->fd.s, backlog) != SOCKET_ERROR;
		}

		int shutdown(handle_type handle, int how) {
			return ::shutdown(handle->fd.s, how);
		}

		handle_type accept(handle_type handle, address_type& addr) {
			handle_type accepted_handle = _implementation.make_shared_handle();
			accepted_handle->fd.s = ::accept(handle->fd.s, addr.sockaddr(), addr.length_ptr());
			return accepted_handle;
		}

		template < typename HandlerType >
		void async_connect(handle_type handle, const address_type& addr, HandlerType&& handler) {
			connect_op<HandlerType>* op =
				new connect_op<HandlerType>(addr, std::forward<HandlerType>(handler));

			if (INVALID_SOCKET != handle->fd.s) {
				//set_option(handle, cx::io::ip::option::non_blocking());
				LPFN_CONNECTEX _connect_ex = nullptr;
				DWORD bytes_returned = 0;
				GUID guid = WSAID_CONNECTEX;
				if (SOCKET_ERROR != WSAIoctl(handle->fd.s
					, SIO_GET_EXTENSION_FUNCTION_POINTER
					, &guid, sizeof(guid)
					, &_connect_ex, sizeof(_connect_ex)
					, &bytes_returned, nullptr, nullptr)) {
					address_type bindaddr = address_type::any(0, addr.family());
					if (bind(handle, bindaddr)) {
						if (_implementation.bind(handle, 0)) {
							op->reset();
							bytes_returned = 0;
							if (_connect_ex(handle->fd.s
								, op->address().sockaddr()
								, op->address().length()
								, nullptr, 0
								, &bytes_returned, op->overlapped()) == TRUE)
								return;
							if (WSAGetLastError() == WSA_IO_PENDING)
								return;
						}
					}
				}
			}
			op->error(std::error_code(WSAGetLastError(), cx::windows_category()));
			_implementation.post(op);
		}

		template < typename HandlerType >
		void async_write(handle_type handle, const buffer_type& buf, HandlerType&& handler) {
			write_op<HandlerType>* op =
				new write_op<HandlerType>(buf, std::forward<HandlerType>(handler));
			op->reset();
			DWORD flag = 0;
			DWORD bytes_transferred = 0;
			if (WSASend(handle->fd.s
				, &(op->buffer())
				, 1
				, &bytes_transferred
				, flag
				, op->overlapped()
				, nullptr) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING) {
					op->error(std::error_code(WSAGetLastError(), cx::windows_category()));
					_implementation.post(op);
				}
			}
		}

		template < typename HandlerType >
		void async_read(handle_type handle, buffer_type& buf, HandlerType&& handler) {
			read_op<HandlerType>* op =
				new read_op<HandlerType>(buf, std::forward<HandlerType>(handler));
			op->reset();
			DWORD flag = 0;
			DWORD bytes_transferred = 0;
			if (WSARecv(handle->fd.s
				, &(op->buffer())
				, 1
				, &bytes_transferred
				, &flag
				, op->overlapped()
				, nullptr) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING) {
					op->error(std::error_code(WSAGetLastError(), cx::windows_category()));
					_implementation.post(op);
				}
			}
		}
	private:
		implementation_type& _implementation;
	};

	template <> class completion_port_socket_service< SOCK_DGRAM, IPPROTO_UDP>
		: public basic_completion_port_socket_service < SOCK_DGRAM, IPPROTO_UDP > {
	public:
		struct _buffer {
			_buffer(void* ptr, std::size_t len)
				: buffer(ptr, len)
			{}
			_buffer(void)
				: buffer(nullptr, 0)
			{
			}
			address_type address;
			cx::io::buffer buffer;
		};

		using this_type = completion_port_socket_service< SOCK_DGRAM, IPPROTO_UDP>;
		using buffer_type = _buffer;
		using basic_completion_port_socket_service < SOCK_DGRAM, IPPROTO_UDP >::connect;
		template < typename HandlerType >
		using read_op = basic_read_op< this_type, HandlerType>;
		template < typename HandlerType >
		using write_op = basic_write_op< this_type, HandlerType>;

		completion_port_socket_service(implementation_type& impl)
			: _implementation(impl)
		{}
		implementation_type& implementation(void) {
			return _implementation;
		}

		int write(handle_type handle, const buffer_type& buf) {
			return sendto(handle->fd.s
				, static_cast<const char*>(buf.buffer.base())
				, buf.buffer.length()
				, 0
				, buf.address.sockaddr()
				, buf.address.length());
		}

		int read(handle_type handle, buffer_type& buf) {
			return recvfrom(handle->fd.s
				, static_cast<char*>(buf.buffer.base())
				, buf.buffer.length()
				, 0
				, buf.address.sockaddr()
				, buf.address.length_ptr());
		}

		int shutdown(handle_type, int) {
			return 0;
		}

		template < typename HandlerType >
		void async_write(handle_type handle, const buffer_type& buf, HandlerType&& handler) {
			write_op<HandlerType>* op =
				new write_op<HandlerType>(buf, std::forward<HandlerType>(handler));
			op->reset();
			DWORD flag = 0;
			DWORD bytes_transferred = 0;
			if (WSASendTo(handle->fd.s
				, &(op->buffer())
				, 1
				, &bytes_transferred
				, flag
				, op->buffer().address.sockaddr()
				, op->buffer().address.length()
				, op->overlapped()
				, nullptr) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING) {
					op->error(std::error_code(WSAGetLastError(), cx::windows_category()));
					_implementation.post(op);
				}
			}
		}

		template < typename HandlerType >
		void async_read(handle_type handle, buffer_type& buf, HandlerType&& handler) {
			read_op<HandlerType>* op =
				new read_op<HandlerType>(buf, std::forward<HandlerType>(handler));
			op->reset();
			DWORD flag = 0;
			DWORD bytes_transferred = 0;
			if (WSARecvFrom(handle->fd.s
				, &(op->buffer())
				, 1
				, &bytes_transferred
				, &flag
				, op->buffer().address.sockaddr()
				, op->buffer().address.length_ptr()
				, op->overlapped()
				, nullptr) == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING) {
					op->error(std::error_code(WSAGetLastError(), cx::windows_category()));
					_implementation.post(op);
				}
			}
		}
	private:
		implementation_type& _implementation;
	};

}

#endif // 

#endif