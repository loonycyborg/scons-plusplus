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
#include <boost/type_traits.hpp>
#include <boost/function_types/result_type.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_type.hpp>
#include "environment.hpp"
#include "python_interface/subst.hpp"

using namespace boost::python;

namespace python_interface
{
	object WhereIs(const std::string& name);

	object subst_directive_args(const char*);

	void AddPreAction(object target, object action);
	void AddPostAction(object target, object action);

	template<typename T>
	inline T subst_arg(const environment::Environment&, const T&);
	template<>
	inline object subst_arg(const environment::Environment& env, const object& obj)
	{
		return subst(env, obj);
	}
	template<>
	inline std::string subst_arg(const environment::Environment& env, const std::string& str)
	{
		return expand_python(env, subst(env, str));
	}

	template <typename F, F* f>
	typename boost::function_traits<F>::result_type subst_wrap_directive(
			const environment::Environment& env,
			typename boost::function_traits<F>::arg1_type x,
			typename boost::function_traits<F>::arg2_type y
			)
	{
		return f(subst_arg(env, x), y);
	}
	template <typename F, F* f>
	typename boost::function_traits<F>::result_type subst_wrap_directive(
			const environment::Environment& env,
			typename boost::function_traits<F>::arg1_type x			
			)
	{
		return f(subst_arg(env, x));
	}

	template<typename F, F* f, class Keywords>
	void def_directive(object env_object, const char* name, const Keywords& keywords)
	{
		def(name, f, keywords);
		scope s(env_object);
		typedef typename
			boost::function_types::function_type<
				typename boost::mpl::push_front<
					typename boost::mpl::push_front<boost::function_types::parameter_types<F>, const environment::Environment&>::type,
						typename boost::function_types::result_type<F>::type >::type
			>::type* wrapper_type;
		def(name, (wrapper_type)subst_wrap_directive<F, f>, (arg("env"), keywords));
	}
}

#endif
