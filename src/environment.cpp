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

#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_push_back_actor.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "environment.hpp"
#include "python_interface/python_interface.hpp"

using std::string;
using std::vector;

namespace
{

class substitute_variable
{
	string& result_;
	const environment::Environment& env_;
	public:
	substitute_variable(const environment::Environment& env, string& str) : result_(str), env_(env) {}

	void operator()(const char *begin, const char *end) const
	{
		result_ += env_.subst(python_interface::expand_variable(string(begin, end), env_));
	}
};

class substitute_python
{
	string& result_;
	const environment::Environment& env_;
	public:
	substitute_python(const environment::Environment& env, string& str) : result_(str), env_(env) {}

	void operator()(const char *begin, const char *end) const
	{
		result_ += python_interface::eval_string(string(begin, end), env_);
	}
};

using namespace BOOST_SPIRIT_CLASSIC_NS;
rule<> alpha_or_underscore = alpha_p | '_';
rule<> alnum_or_underscore = alnum_p | '_';
rule<> variable_name = alpha_or_underscore >> *alnum_or_underscore;

}

namespace environment
{

std::string parse_variable_ref(const std::string& str)
{
	string result;

	rule<> variable = ch_p('$') >> variable_name[assign_a(result)];
	rule<> variable_with_braces = ch_p('$') >> '{' >> variable_name[assign_a(result)] >> '}';
	rule<> variable_ref = variable | variable_with_braces;

	parse(str.c_str(), variable_ref);
	return result;
}

std::string Environment::subst(const std::string& str) const
{
	string result;

	rule<> variable = ch_p('$') >> variable_name[substitute_variable(*this, result)];
	rule<> python_code = ch_p('$') >> '{' >> (*~ch_p("}"))[substitute_python(*this, result)] >> '}';
	rule<> r = *(variable | python_code | anychar_p[push_back_a(result)]);

	parse(str.c_str(), r);
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
