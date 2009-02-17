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

#ifndef PYTHON_INTERFACE_INTERNAL_HPP
#define PYTHON_INTERFACE_INTERNAL_HPP

#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/foreach.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/filesystem/path.hpp>
#define foreach BOOST_FOREACH

using namespace boost::python;

extern dict main_namespace;

namespace python_interface
{

inline bool is_instance(object obj, object type)
{
	int result = PyObject_IsInstance(obj.ptr(), type.ptr());
	if(result == -1)
		throw_error_already_set();
	return result;
}

inline bool is_string(object obj)
{
	return is_instance(obj, make_tuple(handle<>(borrowed(&PyString_Type)), handle<>(borrowed(&PyUnicode_Type))));
}

inline bool is_list(object obj)
{
	return is_instance(obj, object(handle<>(borrowed(&PyList_Type))));
}

inline bool is_dict(object obj)
{
	return is_instance(obj, object(handle<>(borrowed(&PyDict_Type))));
}

inline bool is_callable(object obj)
{
	static object callable = eval("callable", main_namespace, main_namespace);
	return extract<bool>(callable(obj));
}

inline std::string type(object obj)
{
	static object type_func = eval("type", main_namespace, main_namespace);
	return extract<std::string>(str(type_func(obj)));
}

inline object identity_function()
{
	static object function = eval("lambda x : x");
	return function;
}

object flatten(object obj);
object dictify(object obj);

template<class Object>
inline boost::iterator_range<stl_input_iterator<object> > make_object_iterator_range(Object obj)
{
	return boost::make_iterator_range(stl_input_iterator<object>(obj), stl_input_iterator<object>());
}

inline object partial(object func, object arg)
{
	static object partial_func = import("functools").attr("partial");
	return partial_func(func, arg);
}

inline object copy(object obj)
{
	static object copy_func = import("copy").attr("copy");
	return copy_func(obj);
}

inline object deepcopy(object obj)
{
	static object deepcopy_func = import("SCons").attr("Util").attr("semi_deepcopy");
	return deepcopy_func(obj);
}

inline object call_extended(object obj, tuple args, dict kw)
{
	PyObject* result = PyObject_Call(obj.ptr(), args.ptr(), kw.ptr());
	if(result == NULL)
		throw_error_already_set();
	return object(handle<PyObject>(borrowed(result)));
}

}

#endif
