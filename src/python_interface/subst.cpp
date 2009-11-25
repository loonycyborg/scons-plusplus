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

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/algorithm/string/join.hpp>

#include "environment.hpp"
#include "subst.hpp"
#include "python_interface.hpp"
#include "python_interface_internal.hpp"
#include "environment_wrappers.hpp"

namespace
{

using boost::spirit::raw;
#if BOOST_VERSION < 104100
using boost::spirit::char_;
#else
using boost::spirit::qi::char_;
#endif
using boost::spirit::qi::parse;
using boost::spirit::qi::rule;
using boost::spirit::qi::grammar;
using boost::spirit::ascii::alnum;
using boost::spirit::ascii::alpha;
#if BOOST_VERSION < 104100
using boost::spirit::arg_names::_val;
namespace args = boost::spirit::arg_names;
#else
using boost::spirit::_val;
namespace args = boost::spirit;
#endif
using boost::phoenix::function;
using boost::phoenix::ref;

template <typename Iterator>
struct variable_ref : grammar<Iterator, std::string()>
{
	variable_ref() : variable_ref::base_type(ref)
	{
		variable_name %= raw[((alpha | '_') >> *(alnum | '_'))];
		ref %= '$' >> (variable_name | ('{' >> variable_name >> '}'));
	}

	rule<Iterator, std::string()> variable_name, ref;
};

template <typename Iterator>
struct python_code : grammar<Iterator, std::string()>
{
	python_code() : python_code::base_type(placeholder)
	{
		placeholder %= "${" >> code >> '}';
		code %= raw[(*(char_ - '}'))];
	}

	rule<Iterator, std::string()> code, placeholder;
};

struct expand_impl
{
	template<typename Arg1, typename Arg2>
	struct result
	{
		typedef object type;
	};

	object operator()(const environment::Environment& env, const std::string& variable) const
	{
		return env.count(variable) ? python_interface::subst(env, python_interface::variable_to_python(env[variable])) : object();
	}
};

function<expand_impl> lazy_expand;

struct eval_python_impl
{
	template<typename Arg1, typename Arg2>
	struct result
	{
		typedef object type;
	};

	object operator()(const environment::Environment& env, const std::string& python) const
	{
		return python_interface::subst(env, eval(str(python), object(env), object(env)));
	}
};

function<eval_python_impl> lazy_eval_python;

struct combine_subst_impl
{
	template<typename Arg1, typename Arg2, typename Arg3>
	struct result
	{
		typedef void type;
	};

	void operator()(const environment::Environment& env, object& val, object new_val) const
	{
		if(!val) {
			val = new_val;
			return;
		}
		if(!python_interface::is_string(val))
			val = str(python_interface::subst_to_string(env, python_interface::expand_python(env, val)));
		if(!python_interface::is_string(new_val))
			new_val = str(python_interface::subst_to_string(env, python_interface::expand_python(env, new_val)));
		val += new_val;
	}
	void operator()(const environment::Environment& env, object& val, const char new_val) const
	{
		(*this)(env, val, str(new_val));
	}
};

function<combine_subst_impl> combine_subst;

}

namespace python_interface
{

object subst(const environment::Environment& env, const std::string& input)
{
	object result;

	rule<std::string::const_iterator, object()> interpolator;
	variable_ref<std::string::const_iterator> vref;
	python_code<std::string::const_iterator> python;
	interpolator = *(
		vref[combine_subst(boost::phoenix::ref(env), _val, lazy_expand(boost::phoenix::ref(env), args::_1))] |
		python[combine_subst(boost::phoenix::ref(env), _val, lazy_eval_python(boost::phoenix::ref(env), args::_1))] |
		char_[combine_subst(boost::phoenix::ref(env), _val, args::_1)]
		);
	std::string::const_iterator iter = input.begin();
	parse(iter, input.end(), interpolator, result);

	return result;
}

object subst(const environment::Environment& env, object obj)
{
	if(is_list(obj) || is_tuple(obj)) {
		list result;
		foreach(object item, make_object_iterator_range(obj)) {
			result.append(subst(env, item));
		}
		return result;
	}

	if(is_string(obj))
		return subst(env, extract<std::string>(obj));
	return obj;
}

std::string expand_python(const environment::Environment& env, object obj)
{
	if(is_none(obj))
		return std::string();
	if(is_callable(obj))
		return extract<string>(str(obj(get_item_from_env(env, "SOURCES"), get_item_from_env(env, "TARGETS"), env, false)));
	try {
		std::vector<std::string> words;
		foreach(object item, make_object_iterator_range(obj))
			words.push_back(expand_python(env, item));
		return boost::algorithm::join(words, std::string(" "));
	} catch(const error_already_set&) {
		PyErr_Clear();
	}
	return extract<std::string>(str(obj));
}

std::string subst_to_string(const environment::Environment& env, const std::string& input)
{
	return expand_python(env, subst(env, input));
}

}
