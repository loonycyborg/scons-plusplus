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
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "environment.hpp"
#include "python_interface/python_interface.hpp"

using std::string;
using std::vector;

namespace
{

using namespace boost::spirit;
using namespace boost::spirit::qi;
using namespace boost::spirit::ascii;
using namespace boost::spirit::arg_names;
using namespace boost::phoenix;

template <typename Iterator>
struct variable_ref : grammar<Iterator, std::string()>
{
	variable_ref() : variable_ref::base_type(ref)
	{
		variable_name %= raw[((alpha || '_') >> *(alnum || '_'))];
		ref = '$' >> (variable_name[_val = _1] || ('{' >> variable_name[_val = _1] >> '}'));
	}

	rule<Iterator, std::string()> variable_name, ref;
};

template <typename Iterator>
struct python_code : grammar<Iterator, std::string()>
{
	python_code() : python_code::base_type(placeholder)
	{
		placeholder = "${" >> code[_val = _1] >> '}';
		code %= raw[(*(char_ - '}'))];
	}

	rule<Iterator, std::string()> code, placeholder;
};

struct expand_impl
{
	template<typename Arg1, typename Arg2>
	struct result
	{
		typedef std::string type;
	};

	std::string operator()(const environment::Environment& env, const std::string& variable) const
	{
		return env.subst(python_interface::expand_variable(variable, env));
	}
};

function<expand_impl> lazy_expand;

struct eval_python_impl
{
	template<typename Arg1, typename Arg2>
	struct result
	{
		typedef std::string type;
	};

	std::string operator()(const environment::Environment& env, const std::string& python) const
	{
		return env.subst(python_interface::eval_string(python, env));
	}
};

function<eval_python_impl> lazy_eval_python;

}

namespace environment
{

std::string parse_variable_ref(const std::string& str)
{
	string result;
	std::string::const_iterator iter = str.begin();
	variable_ref<std::string::const_iterator> vref;

	parse(iter, str.end(), vref, result);
	return result;
}

std::string Environment::subst(const std::string& str) const
{
	string result;

	rule<std::string::const_iterator, string()> interpolator;
	variable_ref<std::string::const_iterator> vref;
	python_code<std::string::const_iterator> python;
	interpolator = *(
		vref[_val += lazy_expand(ref(*this), _1)] ||
		python[_val += lazy_eval_python(ref(*this), _1)] ||
		char_[_val += _1]
		);
	std::string::const_iterator iter = str.begin();
	parse(iter, str.end(), interpolator, result);

	return result;
}

Environment::pointer Environment::clone() const
{
	pointer new_env(new Environment);
	foreach(const Variables::value_type& val, variables_)
		(*new_env)[val.first] = val.second->clone();
	return new_env;
}

string CompositeVariable::to_string() const
{
	vector<string> strings;
	foreach(const Variable::pointer& variable, variables_)
		strings.push_back(variable->to_string());
	return boost::join(strings, " ");
}

CompositeVariable::pointer CompositeVariable::clone() const
{
	boost::shared_ptr<CompositeVariable> result(new CompositeVariable);
	foreach(const Variable::pointer& variable, variables_)
		result->push_back(variable->clone());
	return result;
}

}
