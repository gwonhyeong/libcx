/**
 * @brief 
 * 
 * @file basic_socket.hpp
 * @author ghtak
 * @date 2018-05-22
 */

#ifndef __cx_io_ip_basic_socket_h__
#define __cx_io_ip_basic_socket_h__

#include <cx/io/ip/detail/socket_layer.hpp>

namespace cx::io::ip{

    template < int Type , int Proto , typename SocketLayer >
    class basic_socket {
    public:
        basic_socket( void ) : _fd(invalid_descriptor){}
        basic_socket( descriptor_type fd ) : _fd(fd){}

        bool open( int family = AF_INET ) {
            _fd = SocketLayer::open( family , Type , Proto );
            return _fd != invalid_descriptor;
        }

        void close( void ) {
            if ( _fd != invalid_descriptor ) {
                SocketLayer::close(_fd);
            }
            _fd = invalid_descriptor;
        }

        bool bind( const ip::address& addr ) {
            return ::bind( _fd , addr.sockaddr() , addr.length()) != -1;
        }

        bool listen( void ) {
            return ::listen( _fd , SOMAXCONN ) != -1;
        }

        bool connect( const cx::io::ip::address& address ) {
            if (::connect( _fd , address.sockaddr() , address.length()) == 0 )
                return true;
#if CX_PLATFORM == CX_P_WINDOWS
            if ( WSAGetLastError() == WSAEWOULDBLOCK ) 
                return true;
#else
            if ( errno == EINPROGRESS )
                return true;
#endif
            return false;
        }

        int write( const cx::io::buffer& buf ) {
             return send( _fd , static_cast<const char*>(buf.ptr()) , buf.len() , 0 );
        }

        int read( const cx::io::buffer& buf ) {
            return recv( _fd , static_cast<char*>(buf.ptr()) , buf.len() , 0 );
        }

        int write( const std::string_view& msg ) {
            return write( cx::io::buffer(msg));
        }

        descriptor_type accept( cx::io::ip::address& addr ) {
            return ::accept( _fd , addr.sockaddr() , addr.length_ptr());
        }

        int shutdown( int how ) {
            return ::shutdown( _fd , how );
        }

        cx::io::ip::address local_address( void ) const {
            cx::io::ip::address addr;
            ::getsockname( _fd , addr.sockaddr() , addr.length_ptr() );
            return addr;
        }

        cx::io::ip::address remote_address( void ) const {
            cx::io::ip::address addr;
            ::getpeername( _fd , addr.sockaddr() , addr.length_ptr() );
            return addr;
        }

        descriptor_type descriptor( void ) {  
            return _fd;
        }

        descriptor_type descriptor( descriptor_type fd ) {
            std::swap( _fd , fd );
            return fd;
        }
        template < typename T >
        bool set_option( T opt ) {
            return opt.set( _fd );
        }

        template < typename T >
        bool get_option( T& opt ) {
            return opt.get( _fd );
        }
    private:
        descriptor_type _fd;
    };

    namespace tcp {
        using socket = basic_socket<  SOCK_STREAM , IPPROTO_TCP , tcp::socket_layer_impl >;
    }

    namespace udp {
        //using socket = basic_socket<  SOCK_DGRAM , IPPROTO_UDP >;
    }
} // cx::io::ip

#endif
