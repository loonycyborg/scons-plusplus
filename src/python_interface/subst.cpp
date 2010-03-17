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
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/variant/apply_visitor.hpp>

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

boost::variant<std::string, object> expand_variable(const environment::Environment& env, const boost::iterator_range<std::string::const_iterator>& str)
{
	using namespace python_interface;
	std::string name(str.begin(), str.end());
	environment::Variable::const_pointer var = env[name];
	if(!var)
		return "";
	try {
		const PythonVariable* variable = boost::polymorphic_cast<const PythonVariable*>(var.get());
		object obj = variable->get();
		if(is_callable(obj))
			obj = obj(get_item_from_env(env, "SOURCES"), get_item_from_env(env, "TARGETS"), env, false);
		return python_interface::subst(env, obj);
	} catch(const std::bad_cast&) {
	}
	return python_interface::subst(env, python_interface::variable_to_python(var));
}

boost::variant<std::string, object> eval_python(const environment::Environment& env, const std::string& code)
{
	return python_interface::subst(env, python_interface::eval(str(code), object(env), object(env)));
}

template <typename Iterator>
struct interpolator : grammar<Iterator, std::vector<boost::variant<std::string, object> >()>
{
	interpolator(const environment::Environment& env) : interpolator::base_type(input)
	{
		input %= *(variable_ref | python | text);

		raw_text %= raw[+(char_ - '$')];
		text = raw_text[_val = args::_1];

		variable_ref = ('$' >> (variable_name | ('{' >> variable_name >> '}')))
			[_val = boost::phoenix::bind(expand_variable, boost::phoenix::ref(env), args::_1)];
		variable_name %= raw[((alpha | '_') >> *(alnum | '_'))];

		python_code %= raw[*(char_ - '}')];
		python = ("${" >> python_code >> '}')
			[_val = boost::phoenix::bind(eval_python, boost::phoenix::ref(env), args::_1)];
	}

	rule<Iterator, std::vector<boost::variant<std::string, object> >()> input;
	rule<Iterator, boost::variant<std::string, object>()> variable_ref, python, text;
	rule<Iterator, std::string()> raw_text, variable_name, python_code;
};

class to_object : public boost::static_visitor<object>
{
	public:
	object operator()(object& obj) const
	{
		return obj;
	}

	object operator()(const std::string& str) const
	{
		return boost::python::str(str);
	}
};

class to_string : public boost::static_visitor<std::string>
{
	const environment::Environment& env;
	public:
	to_string(const environment::Environment& env) : env(env)
	{
	}
	std::string operator()(object& obj) const
	{
		return python_interface::expand_python(env, obj);
	}

	std::string operator()(const std::string& str) const
	{
		return str;
	}
};


}

namespace python_interface
{

object subst(const environment::Environment& env, const std::string& input)
{
	object result;
	std::vector<boost::variant<std::string, object> > parse_result;
	std::string::const_iterator iter(input.begin());

	parse(iter, input.end(), interpolator<std::string::const_iterator>(env), parse_result);

	if(parse_result.empty())
		return str();
	if(parse_result.size() == 1)
		return boost::apply_visitor(::to_object(), parse_result[0]);

	std::vector<std::string> strings;
	::to_string visitor(env);
	std::transform(parse_result.begin(), parse_result.end(), std::back_inserter(strings), boost::apply_visitor(visitor));
	return object(boost::algorithm::join(strings, ""));
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
