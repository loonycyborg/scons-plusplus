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

#include "python_interface_internal.hpp"
#include <pybind11/eval.h>

#include <numeric>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_function.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "environment.hpp"
#include "subst.hpp"
#include "python_interface.hpp"
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
using boost::spirit::qi::on_error;
using boost::spirit::qi::fail;
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
using boost::phoenix::val;
using boost::phoenix::throw_;
using boost::phoenix::construct;

using sconspp::Environment;
using sconspp::Variable;

boost::variant<std::string, py::object> expand_variable(const Environment& env, const boost::iterator_range<std::string::const_iterator>& str, bool for_signature)
{
	using namespace sconspp::python_interface;
	std::string name(str.begin(), str.end());
	Variable::const_pointer var = env[name];
	if(!var)
		return "";
	try {
		const PythonVariable* variable = boost::polymorphic_cast<const PythonVariable*>(var.get());
		py::object obj = variable->get();
		if(PyCallable_Check(obj.ptr()))
			obj = obj(get_item_from_env(env, "SOURCES"), get_item_from_env(env, "TARGETS"), env, false);
		return sconspp::python_interface::subst(env, obj, for_signature);
	} catch(const std::bad_cast&) {
	}
	return sconspp::python_interface::subst(env, sconspp::python_interface::variable_to_python(var), for_signature);
}

boost::variant<std::string, py::object> eval_python(const Environment& env, const std::string& code, bool for_signature)
{
	return sconspp::python_interface::subst(env, py::eval(py::str(code), py::cast(env), py::cast(env)), for_signature);
}

class concat : public boost::static_visitor<boost::variant<std::string, py::object> >
{
	const Environment& env;

	public:
	concat(const Environment& env) : env(env) {}
	boost::variant<std::string, py::object> operator()(const std::string str1, const std::string str2)
	{
		return str1 + str2;
	}
	boost::variant<std::string, py::object> operator()(py::object obj, const std::string str)
	{
		return sconspp::python_interface::expand_python(env, obj) + str;
	}
	boost::variant<std::string, py::object> operator()(const std::string str, py::object obj)
	{
		return str + sconspp::python_interface::expand_python(env, obj);
	}
	boost::variant<std::string, py::object> operator()(py::object obj1, py::object obj2)
	{
		return sconspp::python_interface::expand_python(env, obj1) + sconspp::python_interface::expand_python(env, obj2);
	}
};

boost::variant<std::string, py::object> concat_subst(const Environment& env, const std::vector<boost::variant<std::string, py::object> >& objs)
{
	if(objs.empty())
		return "";
	::concat visitor(env);
	return std::accumulate(++objs.begin(), objs.end(), objs[0], boost::apply_visitor(visitor));
}

struct handle_parse_error_impl
{
	template <class Arg1>
	struct result
	{
		typedef std::string type;
	};
	template <class Arg1, class Arg2, class Arg3, class Arg4>
	std::string operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) const
	{
		std::ostringstream os;
		os << "Parse error during substitution: Expecting " << arg4;
		throw std::runtime_error(os.str());
	}

};

function<handle_parse_error_impl> handle_parse_error;

template <typename Iterator>
struct interpolator : grammar<Iterator, boost::variant<std::string, py::object>()>
{
	interpolator(const Environment& env, bool for_signature) : interpolator::base_type(input)
	{
		input = (*(variable_ref | python | skip | text))
			[_val = boost::phoenix::bind(concat_subst, boost::phoenix::ref(env), args::_1)];

		if(for_signature)
			skip = "$(" > (*(char_ - "$)"))[_val = ""] > "$)";
		else
			skip %= "$(" > input > "$)";

		raw_text %= raw[+(char_ - '$')];
		text = raw_text[_val = args::_1];

		variable_ref = ('$' >> (variable_name | ('{' >> variable_name >> '}')))
			[_val = boost::phoenix::bind(expand_variable, boost::phoenix::ref(env), args::_1, for_signature)];
		variable_name %= raw[((alpha | '_') >> *(alnum | '_'))];

		python_code %= raw[*(char_ - '}')];
		python = ("${" > python_code > '}')
			[_val = boost::phoenix::bind(eval_python, boost::phoenix::ref(env), args::_1, for_signature)];

		on_error<fail>(input, handle_parse_error(args::_1, args::_2, args::_3, args::_4));
	}

	rule<Iterator, boost::variant<std::string, py::object>()> variable_ref, python, text, skip, input;
	rule<Iterator, std::string()> raw_text, variable_name, python_code;
};

class to_object : public boost::static_visitor<py::object>
{
	public:
	py::object operator()(py::object& obj) const
	{
		return obj;
	}

	py::object operator()(const std::string& str) const
	{
		return py::str(str);
	}
};

class to_string : public boost::static_visitor<std::string>
{
	const Environment& env;
	public:
	to_string(const Environment& env) : env(env)
	{
	}
	std::string operator()(py::object& obj) const
	{
		return sconspp::python_interface::expand_python(env, obj);
	}

	std::string operator()(const std::string& str) const
	{
		return str;
	}
};


}

namespace sconspp
{
namespace python_interface
{

py::object subst(const Environment& env, const std::string& input, bool for_signature)
{
	boost::variant<std::string, py::object> parse_result;
	std::string::const_iterator iter(input.begin());

	interpolator<std::string::const_iterator> interp(env, for_signature);
	parse(iter, input.end(), interp, parse_result);

	::to_object visitor;
	return boost::apply_visitor(visitor, parse_result);
}

py::object subst(const Environment& env, py::object obj, bool for_signature)
{
	if(py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
		py::list result;
		for(auto item : obj) {
			result.append(subst(env, py::reinterpret_borrow<py::object>(item), for_signature));
		}
		return result;
	}

	if(py::isinstance<py::str>(obj))
		return subst(env, obj.cast<std::string>(), for_signature);
	return obj;
}

std::string expand_python(const Environment& env, py::object obj)
{
	if(obj.is_none())
		return std::string();

	if(py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
		std::vector<std::string> words;
		for(auto item : obj)
			words.push_back(expand_python(env, py::reinterpret_borrow<py::object>(item)));
		return boost::algorithm::join(words, std::string(" "));
	}
	return py::str(obj).cast<std::string>();
}

}
}
