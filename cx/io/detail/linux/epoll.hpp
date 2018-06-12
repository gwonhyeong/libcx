/**
*/
#ifndef __cx_io_detail_epoll_h__
#define __cx_io_detail_epoll_h__

 /*
 */
#include <cx/cxdefine.hpp>
#include <cx/slist.hpp>
#include <cx/container_of.hpp>
#include <cx/error.hpp>

#include <mutex>
#include <set>
#include <cx/io/detail/reactor/reactor.hpp>

#if CX_PLATFORM == CX_P_LINUX

namespace cx::io {

	class epoll {
    public:
        using handle = reactor_base::handle;
        using handle_type = reactor_base::handle_type;
        using operation_type = reactor_base::operation_type;

		epoll(void)
			: _handle(epoll_create(256))
            , _eventfd(eventfd(0,0))
		{
			_active_links.store(0);
            epoll_event evt;
            evt.events = EPOLLIN;
            evt.data.ptr = nullptr;
            epoll_ctl( _handle , EPOLL_CTL_ADD, _eventfd , &evt );
		}

        ~epoll(void) {
            close(_handle);
            _handle = -1;
        }

        bool bind(const handle_type& ptr, const int ops) {
            if (!ptr)
                return false;
            epoll_event evt;
            evt.events = ops;
            evt.data.ptr = ptr.get();
            if ( epoll_ctl( _handle , EPOLL_CTL_MOD , ptr->fd , &evt ) == 0) {
                return true;
            }
            if ( errno == ENOENT ){
                if ( epoll_ctl( _handle , EPOLL_CTL_ADD , ptr->fd , &evt ) == 0 ) {
                    _active_handles.insert(ptr);
                    return true;
                }
            }
            return false;
        }

        void unbind(const handle_type& ptr) {
            epoll_event evt;
            evt.events = 0;
            evt.data.ptr = ptr.get();
            epoll_ctl(_handle,EPOLL_CTL_DEL , ptr->fd , &evt );
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _active_handles.erase(ptr);
        }

        void add_active_links(void) {
            _active_links += 1;
        }

        void release_active_links(void) {
            _active_links -= 1;
        }

        void run(void) {
            while (_active_links.load() != 0 || _active_handles.size() != 0) {
                run(std::chrono::milliseconds(0xffffffff));
            }
        }

        int run(const std::chrono::milliseconds& ms) {
            struct epoll_event events[256];
            int nbfd = epoll_wait( _handle 
                    , events
                    , 256
                    , ms.count());
            if ( nbfd <= 0 ) {
                return 0;
            }
            int proc = 0;
            for ( int i = 0; i < nbfd ; ++i ) {
                epoll::handle* raw_handle = static_cast<epoll::handle*>(events[i].data.ptr );
                if ( raw_handle ) {
                    proc += raw_handle->handle_events( events[i].events );
                } else {
                    uint64_t v;
                    read( _eventfd , &v , sizeof(v));
                    cx::slist< operation_type > ops;
                    do {
                        std::lock_guard<std::recursive_mutex> lock(_mutex);
                        _ops.swap(ops);
                    } while(0);
                    while (operation_type* op = ops.head()) {
                        ops.remove_head();
                        proc += (*op)();
                    }
                }
            }
            return proc;
        }

        void post(operation_type* op) {
            do {
                std::lock_guard<std::recursive_mutex> lock(_mutex);
                _ops.add_tail(op);
            } while (0);
            uint64_t v = 0;
            write(_eventfd,&v,sizeof(v));
        }

        template < typename HandlerT >
            void post_handler(HandlerT&& handler) {
                class handler_op : public operation_type {
                    public:
                        handler_op(HandlerT&& handler)
                            : _handler(std::forward<HandlerT>(handler))
                        {}
                        virtual ~handler_op(void) override {
                        }
                        virtual int operator()(void) override {
                            _handler();
                            delete this;
                            return 1;
                        }
                    private:
                        HandlerT _handler;
                };
                post(new handler_op(std::forward<HandlerT>(handler)));
            }
    private:
        int _handle;
        int _eventfd;
        std::recursive_mutex _mutex;
        cx::slist<operation_type> _ops;
        std::set<handle_type> _active_handles;
        std::atomic<int> _active_links;
    };

}

#endif

#endif

