/***************************************************************************
 *   Copyright (C) 2009 by Sergey Popov                                    *
 *   loonycyborg@gmail.com                                                 *
 *                                                                         *
 *  This file is part of SCons++.                                          *
 *                                                                         *
 *  SCons++ is free software; you can redistribute it and/or modify        *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  SCons++ is distributed in the hope that it will be useful,             *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#ifndef PYTHON_INTERFACE_DIRECTIVES_HPP
#define PYTHON_INTERFACE_DIRECTIVES_HPP

#include <boost/python.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/type_traits.hpp>
#include <boost/function_types/result_type.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_type.hpp>
#include <boost/preprocessor/repetition/enum_shifted.hpp>
#include <boost/mpl/set_c.hpp>
#include "environment.hpp"
#include "python_interface/subst.hpp"

using namespace boost::python;

namespace sconspp
{
namespace python_interface
{
	object WhereIs(const std::string& name);

	object subst_directive_args(const char*);

	void AddPreAction(object target, object action);
	void AddPostAction(object target, object action);
	void Depends(object target, object dependency);
	object AlwaysBuild(tuple args, dict keywords);
	object FindFile(const std::string& name, object dir_objs);

	template<typename T>
	inline T subst_arg(const Environment&, const T& val) { return val; }
	template<>
	inline object subst_arg(const Environment& env, const object& obj)
	{
		return subst(env, obj);
	}
	template<>
	inline std::string subst_arg(const Environment& env, const std::string& str)
	{
		return expand_python(env, subst(env, str));
	}

	#define ARG_DECL(z, n, data) typename BOOST_PP_CAT(boost::function_traits<F>::arg, BOOST_PP_CAT(n, _type)) BOOST_PP_CAT(arg, n)
	#define ARG_SUBST(z, n, data) boost::mpl::has_key<NoSubstArgs, boost::mpl::integral_c<int, n> >::type::value ? BOOST_PP_CAT(arg, n) : subst_arg(env, BOOST_PP_CAT(arg, n))

	#define DEFINE_SUBST_WAPPER(z, n, data) \
		template <typename F, F* f, typename NoSubstArgs> \
		typename boost::function_traits<F>::result_type subst_wrap_directive( \
	        const Environment& env, \
			BOOST_PP_ENUM_SHIFTED(n, ARG_DECL, unused) \
			) \
		{ \
			return f(BOOST_PP_ENUM_SHIFTED(n, ARG_SUBST, unused)); \
		}
	BOOST_PP_REPEAT_FROM_TO(2, 5, DEFINE_SUBST_WAPPER, unused)

	template<typename F, F* f, typename NoSubstArgs, class Keywords>
	void def_directive(object env_object, const char* name, const Keywords& keywords)
	{
		def(name, f, keywords);
		scope s(env_object);
		typedef typename
			boost::function_types::function_type<
				typename boost::mpl::push_front<
			        typename boost::mpl::push_front<boost::function_types::parameter_types<F>, const Environment&>::type,
						typename boost::function_types::result_type<F>::type >::type
			>::type* wrapper_type;
		def(name, (wrapper_type)subst_wrap_directive<F, f, NoSubstArgs>, (arg("env"), keywords));
	}
	template<typename F, F* f, class Keywords>
	inline void def_directive(object env_object, const char* name, const Keywords& keywords)
	{
		def_directive<F, f, boost::mpl::set_c<int>, Keywords>(env_object, name, keywords);
	}
	template <object (*directive)(tuple, dict)>
	object subst_wrap_directive_raw(tuple args, dict keywords)
	{
		return directive(tuple(subst(extract<const Environment&>(args[0]), args.slice(1, _))), keywords);
	}
	template <object (*directive)(tuple, dict)>
	inline void def_directive_raw(object env_object, const char* name)
	{
		def(name, raw_function(directive));
		scope s(env_object);
		def(name, raw_function(subst_wrap_directive_raw<directive>));
	}
}
}

#endif
