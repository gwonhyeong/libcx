/**
 */
#ifndef __cx_io_detail_reactor_socket_h__
#define __cx_io_detail_reactor_socket_h__

#include <cx/cxdefine.hpp>
#include <cx/io/ip/basic_address.hpp>
#include <cx/io/basic_buffer.hpp>
#include <cx/io/detail/reactor/reactor.hpp>
#include <cx/io/detail/reactor/reactor_connect_op.hpp>
#include <cx/io/detail/reactor/reactor_accept_op.hpp>
#include <cx/io/detail/reactor/reactor_read_op.hpp>
#include <cx/io/detail/reactor/reactor_write_op.hpp>

namespace cx::io::ip {

	template < typename ImplementationType, typename ServiceType >
	class basic_reactor_socket_service;

	template < typename ImplementationType
		, template <typename, int, int > class ServiceType
		, int Type, int Proto >
		class basic_reactor_socket_service
		<
		ImplementationType
		, ServiceType<ImplementationType, Type, Proto>
		> {
		public:
			using implementation_type = ImplementationType;
			using service_type = ServiceType<ImplementationType, Type, Proto>;
			struct _handle : public implementation_type::handle {
				_handle(service_type& svc)
					: service(svc)
				{
				}
				service_type& service;
			};
			using handle_type = std::shared_ptr<_handle>;
			using address_type = cx::io::ip::basic_address< struct sockaddr_storage, Type, Proto >;

			using native_handle_type = int;
			static const native_handle_type invalid_native_handle = -1;

			handle_type make_shared_handle(service_type& svc) {
				return std::make_shared<_handle>(svc);
			}

			basic_reactor_socket_service(implementation_type& impl)
				: _implementation(impl)
			{}

			bool open(handle_type handle, const address_type& address) {
				close(handle);
				handle->fd = ::socket(address.family()
					, address.type()
					, address.proto());
				if (handle->fd != -1)
					return true;
				return false;
			}

			void close(handle_type handle) {
				if (handle && handle->fd != -1) {
					implementation().unbind(handle);
					::close(handle->fd);
					handle->fd = -1;
				}
			}

			bool connect(handle_type handle, const address_type& address) {
				if (::connect(handle->fd, address.sockaddr(), address.length()) == 0)
					return true;
				if (errno == EINPROGRESS)
					return true;
				return false;
			}

			bool bind(handle_type handle, const address_type& address) {
				return ::bind(handle->fd, address.sockaddr(), address.length()) != -1;
			}

			int poll(handle_type handle
				, int ops
				, const std::chrono::milliseconds& ms)
			{
				struct pollfd pfd = { 0 };
				pfd.fd = handle->fd;
				pfd.events = ops;
				if (::poll(&pfd, 1, ms.count()) == -1) {
					return -1;
				}
				return pfd.revents;
			}

			address_type local_address(handle_type handle) const {
				address_type addr;
				::getsockname(handle->fd, addr.sockaddr(), addr.length_ptr());
				return addr;
			}

			address_type remote_address(handle_type handle) const {
				address_type addr;
				::getpeername(handle->fd, addr.sockaddr(), addr.length_ptr());
				return addr;
			}

			template < typename T >
			bool set_option(handle_type handle, T&& opt) {
				return opt.set(handle->fd);
			}

			template < typename T >
			bool get_option(handle_type handle, T&& opt) {
				return opt.get(handle->fd);
			}

			bool good(handle_type handle) {
				if (handle) {
					return handle->fd != -1;
				}
				return false;
			}

			implementation_type& implementation(void) {
				return _implementation;
			}
		private:
			implementation_type& _implementation;
	};

	template < typename ImplementationType, int Type, int Proto >
	class reactor_socket_service;

	template <typename ImplementationType>
	class reactor_socket_service< ImplementationType, SOCK_STREAM, IPPROTO_TCP >
		: public basic_reactor_socket_service < ImplementationType,
		reactor_socket_service< ImplementationType, SOCK_STREAM, IPPROTO_TCP >> {
	public:
		using implementation_type = ImplementationType;
		using this_type = reactor_socket_service< ImplementationType, SOCK_STREAM, IPPROTO_TCP >;
		using buffer_type = cx::io::buffer;
		using base_type = basic_reactor_socket_service < ImplementationType, this_type >;
		using base_type::connect;
		using base_type::make_shared_handle;
		using address_type = typename base_type::address_type;
		using handle_type = typename base_type::handle_type;
		using native_handle_type = typename base_type::native_handle_type;
		using operation_type = typename reactor_base::operation_type;

		/*
		template < typename HandlerType > using connect_op
			= cx::io::ip::reactor_connect_op< this_type, HandlerType>;
		template < typename HandlerType > using accept_op
			= cx::io::ip::reactor_accept_op< this_type, HandlerType>;
		template < typename HandlerType > using read_op
			= cx::io::reactor_read_op< this_type, HandlerType>;
		template < typename HandlerType > using write_op
			= cx::io::reactor_write_op< this_type, HandlerType>;
			*/

		handle_type make_shared_handle(void) {
			return this->make_shared_handle(*this);
		}

		handle_type make_shared_handle(native_handle_type handle) {
			auto h = make_shared_handle();
			h->fd.s = handle;
			return h;
		}

		reactor_socket_service(implementation_type& impl)
			: base_type(impl)
		{}

		int write(handle_type handle, const buffer_type& buf) {
			return send(handle->fd, static_cast<const char*>(buf.base()), buf.length(), 0);
		}

		int read(handle_type handle, buffer_type& buf) {
			return recv(handle->fd, static_cast<char*>(buf.base()), buf.length(), 0);
		}

		bool listen(handle_type handle, int backlog) {
			return ::listen(handle->fd, backlog) != -1;
		}

		int shutdown(handle_type handle, int how) {
			return ::shutdown(handle->fd, how);
		}

		handle_type accept(handle_type handle, address_type& addr) {
			handle_type accepted_handle = make_shared_handle();
			accepted_handle->fd = ::accept(handle->fd, addr.sockaddr(), addr.length_ptr());
			return accepted_handle;
		}

		template < typename HandlerType >
		void async_connect(handle_type handle, const address_type& addr, HandlerType&& handler) {
		}

		template < typename HandlerType >
		void async_write(handle_type handle, const buffer_type& buf, HandlerType&& handler) {
		}


		template < typename HandlerType >
		void async_read(handle_type handle, const buffer_type& buf, HandlerType&& handler) {
		}

		template < typename HandlerType >
		void async_accept(handle_type handle, const address_type& addr, HandlerType&& handler) {
		}

		bool connect_complete(handle_type handle, cx::io::ip::basic_connect_op<this_type>* op) {
			return true;
		}

		bool accept_complete(handle_type handle, cx::io::ip::basic_accept_op<this_type>* op) {
			return true;
		}

		bool write_complete(handle_type handle, cx::io::basic_write_op<this_type>* op) {
			return true;
		}

		bool read_complete(handle_type handle, cx::io::basic_read_op<this_type>* op) {
			return true;
		}
	private:

	};

	template <typename ImplementationType>
	class reactor_socket_service< ImplementationType, SOCK_DGRAM, IPPROTO_UDP >
		: public basic_reactor_socket_service < ImplementationType,
		reactor_socket_service< ImplementationType, SOCK_DGRAM, IPPROTO_UDP >>{
	public:
		using implementation_type = ImplementationType;
		using this_type = reactor_socket_service< ImplementationType, SOCK_DGRAM, IPPROTO_UDP>;
		using base_type = basic_reactor_socket_service < ImplementationType, this_type >;
		using base_type::connect;
		using base_type::make_shared_handle;
		using address_type = typename base_type::address_type;
		using handle_type = typename base_type::handle_type;
		using native_handle_type = typename base_type::native_handle_type;
		using operation_type = typename reactor_base::operation_type;

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
		using buffer_type = _buffer;

		/*
		template < typename HandlerType > using read_op = cx::io::reactor_read_op< this_type, HandlerType>;
		template < typename HandlerType > using write_op = cx::io::reactor_write_op< this_type, HandlerType>;

		*/
		handle_type make_shared_handle(void) {
			return this->make_shared_handle(*this);
		}

		handle_type make_shared_handle(native_handle_type handle) {
			auto h = make_shared_handle();
			h->fd.s = handle;
			return h;
		}

		reactor_socket_service(implementation_type& impl)
			: base_type(impl)
		{}

		int write(handle_type handle, const buffer_type& buf) {
			return sendto(handle->fd
				, static_cast<const char*>(buf.buffer.base())
				, buf.buffer.length()
				, 0
				, buf.address.sockaddr()
				, buf.address.length());
		}

		int read(handle_type handle, buffer_type& buf) {
			return recvfrom(handle->fd
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
		}

		template < typename HandlerType >
		void async_read(handle_type handle, const buffer_type& buf, HandlerType&& handler) {
		}

		bool write_complete(handle_type handle, cx::io::basic_write_op<this_type>* op) {
			return true;
		}

		bool read_complete(handle_type handle, cx::io::basic_read_op<this_type>* op) {
			return true;
		}
	};
}

#endif
