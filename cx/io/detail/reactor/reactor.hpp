/**
*/
#ifndef __cx_io_detail_reactor_base_h__
#define __cx_io_detail_reactor_base_h__

#include <cx/cxdefine.hpp>
#include <cx/slist.hpp>

#include <cx/io/io.hpp>
#include <cx/io/ip/basic_address.hpp>

namespace cx::io {

	class reactor_base {
	public:
		struct handle;
		using handle_type = std::shared_ptr<handle>;

		class operation {
		public:
			operation(void)
				: _next(nullptr) {
			}

			virtual ~operation(void) = default;

			int io_size(void) { return _io_size; }
			int io_size(int sz) {
				std::swap(_io_size, sz);
				return sz;
			}

			std::error_code error(void) { return _ec; }
			std::error_code error(const std::error_code& ec) {
				std::error_code old(_ec);
				_ec = ec;
				return old;
			}
			
			operation* next(void) { return _next; }
			operation* next(operation* op) {
				std::swap(_next, op);
				return op;
			}

			virtual int operator()(void) = 0;

			virtual bool complete(const handle_type& /*handle*/){
                return true;
            };
		private:
			std::error_code _ec;
			int _io_size;
			operation* _next;
		};

		using operation_type = operation;
		
		struct handle : public std::enable_shared_from_this<handle> {
			handle( void ){
				fd = -1;
			}

			std::recursive_mutex _mutex;
			int fd;
			cx::slist<operation> ops[2];
			int handle_events(int revt) {
				int ops_filter[2] = { cx::io::pollin , cx::io::pollout };
				int proc = 0;
				for (int i = 0; i < 2; ++i) {
					if (ops_filter[i] & revt) {
						operation* op = ops[i].head();
						if (op && op->complete( shared_from_this())) {
							ops[i].remove_head();
							proc += (*op)();
						}
					}
				}
				return proc;
			}
		};
	};

}

#endif