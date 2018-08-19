/**
 * @brief
 *
 * @file type_list.hpp
 * @author ghtak
 * @date 2018-08-19
 */

#ifndef __cx_base_type_list_h__
#define __cx_base_type_list_h__

#include <cx/base/mp.hpp>

namespace cx::base {

    /**
     * @brief 
     * 
     * @tparam Ts 
     */
	template < typename ... Ts >
	struct type_list {

		using this_type = type_list< Ts ... >;

		template < std::size_t I >
		using at = cx::base::mp::at<I, this_type>;

		using front = cx::base::mp::front<this_type>;

		using back = cx::base::mp::back<this_type>;

		template < typename ... Us >
		using push_back = cx::base::mp::push_back<this_type, Us ... >;

		template < typename ... Us >
		using push_front = cx::base::mp::push_front<this_type, Us ... >;

		using pop_front = cx::base::mp::pop_front<this_type>;

		using pop_back = cx::base::mp::pop_back< cx::base::mp::make_sequence<sizeof...(Ts)-1>, this_type>;

		using size = cx::base::mp::size< Ts ... >;

		template < std::size_t I >
		using at_t = typename cx::base::mp::at< I, type_list< Ts ... >>::type;

		template < template < typename ... > class U >
		struct rebind {
			using other = U< Ts ... >;
		};
	};
}
#endif