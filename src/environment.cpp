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

#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "environment.hpp"
#include "python_interface/python_interface.hpp"

using std::string;
using std::vector;

namespace environment
{

std::string Environment::subst(const std::string& str) const
{
	return python_interface::subst_to_string(*this, str);
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
