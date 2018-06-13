
#include "gtest/gtest.h"
#include "tests/gprintf.hpp"
#include <cx/io/basic_engine.hpp>

static const std::string get("GET / HTTP/1.1\r\n\r\n");

TEST( cx_io_ip_sockets , t0 ) {
    cx::io::engine<
		cx::io::ip::tcp::service 
	> engine;
    cx::io::ip::tcp::socket fd(engine);
    cx::io::ip::tcp::address addr( "127.0.0.1" , 7543 , AF_INET );
    gprintf( "%s" , addr.to_string().c_str());

    auto addresses = cx::io::ip::tcp::address::resolve( "naver.com" , 80 , AF_INET );
    for ( auto a : addresses ) {
        gprintf( a.to_string().c_str() );
    }
    ASSERT_FALSE( addresses.empty());
    ASSERT_TRUE( fd.open(addresses[0]) );
	ASSERT_TRUE(fd);
    ASSERT_TRUE(fd.connect( addresses[0]));

	gprintf("Remote Address : %s", fd.remote_address().to_string().c_str());
	gprintf("Local  Address : %s", fd.local_address().to_string().c_str());
    cx::io::ip::tcp::buffer buf(get);

    ASSERT_EQ( static_cast<std::size_t>(fd.write( buf )) , buf.length() );
    char read_buf[1024] = {0 , };
    cx::io::ip::tcp::buffer rdbuf(read_buf , 1024);
    ASSERT_TRUE( fd.read( rdbuf ) > 0 );
    gprintf( "%s" , rdbuf.base());
    fd.close();
	ASSERT_TRUE(!fd);
    ASSERT_TRUE( fd.handle().get() != nullptr );
}

TEST( cx_io_ip_sockets , timeout ) {
	cx::io::engine<
		cx::io::ip::tcp::service
	> engine;
    cx::io::ip::tcp::socket fd(engine);
    cx::io::ip::tcp::address addr( "127.0.0.1" , 7543 , AF_INET );
    gprintf( "%s" , addr.to_string().c_str());

    auto addresses = cx::io::ip::tcp::address::resolve( "naver.com" , 80 , AF_INET );
    for ( auto a : addresses ) {
        gprintf( a.to_string().c_str() );
    }
    ASSERT_FALSE( addresses.empty());
    ASSERT_TRUE( fd.open(addresses[0]) );
	ASSERT_TRUE(fd);
    ASSERT_TRUE( fd.set_option( cx::io::ip::option::non_blocking()));
    ASSERT_TRUE( fd.connect( addresses[0] , std::chrono::milliseconds(1000)));

    cx::io::ip::tcp::buffer buf(get);

	ASSERT_EQ( static_cast<std::size_t>(fd.write( buf, std::chrono::milliseconds(1000))) , buf.length() );
    char read_buf[1024] = {0 , };
    cx::io::ip::tcp::buffer rdbuf(read_buf , 1024);
    ASSERT_TRUE( fd.read( rdbuf, std::chrono::milliseconds(1000)) > 0 );
    gprintf( "%s" , rdbuf.base());
    fd.close();
	ASSERT_TRUE(!fd);
    ASSERT_TRUE( fd.handle().get() != nullptr );
}

TEST(cx_io_ip_sockets, async_connect ) {
	cx::io::engine<
		cx::io::ip::tcp::service
	> engine;
	cx::io::ip::tcp::socket fd(engine);
	auto addresses = cx::io::ip::tcp::address::resolve("naver.com", 80, AF_INET);
	ASSERT_FALSE(addresses.empty());
	ASSERT_TRUE(fd.open(addresses[0]));
	ASSERT_TRUE(fd);
	ASSERT_TRUE(fd.set_option(cx::io::ip::option::non_blocking()));

	bool in_loop = engine.implementation().in_loop();
	ASSERT_FALSE(in_loop);

	bool connected = false;
	fd.async_connect(addresses[0], [&](const std::error_code& ec) {
		in_loop = engine.implementation().in_loop();
		if (!ec) connected = true;
	});
	
	engine.implementation().run(std::chrono::milliseconds(1000));

	ASSERT_TRUE(connected);

	ASSERT_TRUE(in_loop);
	in_loop = engine.implementation().in_loop();
	ASSERT_FALSE(in_loop);

	int wrsize = 0;
	cx::io::ip::tcp::buffer buf(get);
	fd.async_write(buf, [&](const std::error_code& ec, const int size) {
		in_loop = engine.implementation().in_loop();
		if (!ec) {
			wrsize = size;
		}
	});

	engine.implementation().run(std::chrono::milliseconds(1000));

	ASSERT_TRUE(wrsize==static_cast<int>(buf.length()));

	ASSERT_TRUE(in_loop);
	in_loop = engine.implementation().in_loop();
	ASSERT_FALSE(in_loop);

	int rdsize = 0;
	char read_buf[1024] = { 0 , };
	cx::io::ip::tcp::buffer rdbuf(read_buf, 1024);

	fd.async_read(rdbuf, [&](const std::error_code& ec, const int size) {
		in_loop = engine.implementation().in_loop();
		if (!ec) {
			rdsize = size;
		}
	});
	engine.implementation().run(std::chrono::milliseconds(1000));
	ASSERT_TRUE(rdsize > 0);
	gprintf("%s", rdbuf.base());
	fd.close();
	ASSERT_TRUE(!fd);
	ASSERT_TRUE(fd.handle().get() != nullptr);

	ASSERT_TRUE(in_loop);
	in_loop = engine.implementation().in_loop();
	ASSERT_FALSE(in_loop);
}

TEST(cx_io_ip_sockets, async_connect0_timer ) {
	cx::io::engine<
		cx::io::ip::tcp::service ,
		cx::time::timer_service
	> engine;
	cx::io::ip::tcp::socket fd(engine);
	auto addresses = cx::io::ip::tcp::address::resolve("naver.com", 80, AF_INET);
	ASSERT_FALSE(addresses.empty());
	ASSERT_TRUE(fd.open(addresses[0]));
	ASSERT_TRUE(fd);
	ASSERT_TRUE(fd.set_option(cx::io::ip::option::non_blocking()));

	char read_buf[1024] = { 0 , };
	cx::io::ip::tcp::buffer rdbuf(read_buf, 1024);

	bool step[3] = { false,false,false };
	fd.async_connect(addresses[0], [&](const std::error_code& ec) {
		if (ec)
			fd.close();
		else {
			step[0] = true;
			cx::io::ip::tcp::buffer buf(get);
			fd.async_write(buf, [&](const std::error_code& ec, const int ) {
				if (ec)
					fd.close();
				else {
					step[1] = true;
					fd.async_read(rdbuf, [&](const std::error_code& ec, const int ) {
						if (!ec) {
							step[2] = true;
						}
						fd.close();
					});
				}
			});
		}
	});
	bool calltimer = false;
	cx::time::timer timer(engine);
	timer.expired_at(std::chrono::system_clock::now() + std::chrono::seconds(2));
	timer.handler([&calltimer](const std::error_code&) {
		calltimer = true;
	});
	timer.fire();
	engine.run();
	
	ASSERT_TRUE(step[0] && step[1] && step[2]);
	ASSERT_TRUE(calltimer);
	gprintf("%s", rdbuf.base());
	fd.close();
	ASSERT_TRUE(!fd);
	ASSERT_TRUE(fd.handle().get() != nullptr);
}
