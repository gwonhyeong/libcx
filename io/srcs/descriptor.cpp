/**
 * @brief 
 * 
 * @file descriptor.cpp
 * @author ghtak
 * @date 2018-08-26
 */
#include <cx/io/io.hpp>

#include <cx/io/descriptor.hpp>
#include <cx/io/operation.hpp>

#include <cx/io/engine.hpp>

namespace cx::io {

	descriptor::descriptor(void)
		: _fd(-1)
	{

	}

	void descriptor::put(cx::io::type type, cx::io::operation* op) {
		if(type == cx::io::pollin)
			_ops[0].add_tail(op);
		else if(type == cx::io::pollout)
			_ops[1].add_tail(op);
	}

	cx::io::operation* descriptor::get(cx::io::type type) {
		if (type == cx::io::pollin)
			return _ops[0].head();
		else if (type == cx::io::pollout)
			return _ops[1].head();
		return nullptr;
	}


	int descriptor::handle_event(cx::io::engine& e, int revt) {
		auto pthis = this->shared_from_this();
		int ops_filter[2] = { cx::io::pollin , cx::io::pollout };
		bool changed = false;
		int ev = 0;
		for (int i = 0; i < 2; ++i) {
			if (ops_filter[i] & revt) {
				operation* op = _ops[i].head();
				if (op && op->is_complete(pthis)) {
					_ops[i].remove_head();
					(*op)();
					if (_ops[i].empty())
						changed = true;
					else
						_ops[i].head()->request();
					ev |= ops_filter[i];
				}
			}
		}
		if (changed) {
			int ops = (_ops[0].empty() ? 0 : cx::io::pollin)
				| (_ops[1].empty() ? 0 : cx::io::pollout);
			std::error_code ec;
			if (e.implementation().bind(pthis, ops, ec) == false) {
				// drain_all_ops
				CX_UNUSED(ec);
			}
		}
		return ev;
	}

}