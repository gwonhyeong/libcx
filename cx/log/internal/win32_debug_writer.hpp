/**
*/
#ifndef __cx_log_win32_debug_writer_h__
#define __cx_log_win32_debug_writer_h__

#include <cx/log/writer.hpp>
#if defined(CX_PLATFORM_WIN32)
namespace cx::log {

	class win32_debug_writer : public cx::log::writer {
	public:
		virtual void operator()(const cx::log::record& r, const cx::basic_buffer<char>& formatted) {
			CX_UNUSED(r);
            OutputDebugStringA(formatted.rdptr());
		}
	public:
		static std::shared_ptr<writer> instance(void) {
			static std::shared_ptr<writer> writer_ptr = std::make_shared<win32_debug_writer>();
			return writer_ptr;
		}
	};
}
#endif
#endif