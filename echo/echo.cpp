#include <cx/io/basic_engine.hpp>

#include <iostream>

using engine_type = cx::io::engine<
	cx::io::ip::tcp::service
	, cx::time::timer_service >;

class session : public std::enable_shared_from_this<session> {
public:
	session(engine_type& e, cx::io::ip::tcp::accept_context& ac)
		: _fd(e.template service< cx::io::ip::tcp::service>()
			, ac.make_shared_handle())
	{	
	}

	void do_read(void) {
		if (!_read_op) {
			auto pthis = shared_from_this();
			_read_op = _fd.make_read_op([&, pthis](const std::error_code& ec, int size) {
				if (ec || size <= 0) {
					pthis->_fd.close();
					std::cout << "Close" << std::endl;
					return;
				}

				char* wr_buf = new char[size];
				memcpy(wr_buf, _read_buffer, size);

				_fd.async_write(cx::io::buffer(wr_buf, size),
					[wr_buf](const std::error_code& ec, int) {
					delete[] wr_buf;
				});
				do_read();
			});
		}
		_read_op->buffer().reset(_read_buffer,1024);
		_fd.async_read(_read_op );
	}
private:
	cx::io::ip::tcp::socket _fd;
	cx::io::ip::tcp::socket::read_op_type _read_op;
	char _read_buffer[1024];
};

void async_accept(engine_type& engine, cx::io::ip::tcp::acceptor& acceptor) {
	acceptor.async_accept([&](const std::error_code& e
		, cx::io::ip::tcp::accept_context& ac
		, const cx::io::ip::tcp::address& /*addr*/)
	{
		if (!e) {
			auto p = std::make_shared<session>(engine, ac);
			p->do_read();
		}
		async_accept(engine, acceptor);
	});
}

int main(int argc, char* argv[]) {
	engine_type engine;

	cx::io::ip::tcp::acceptor acceptor(engine);
	if (acceptor.open(cx::io::ip::tcp::address::any(7543, AF_INET))) {
		std::cout << "Open Success" << std::endl;
		async_accept(engine, acceptor);
		engine.run();
	}
	acceptor.close();
	return 0;
}