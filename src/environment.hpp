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

#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <string>
#include <map>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "dependency_graph.hpp"

namespace environment
{

class Variable
{
	public:
	typedef boost::shared_ptr<Variable> pointer;
	typedef boost::shared_ptr<const Variable> const_pointer;

	virtual ~Variable() {}

	virtual std::string to_string() const = 0;

	virtual pointer clone() const = 0;
};

class Environment : public boost::enable_shared_from_this<Environment>
{
	typedef std::map<std::string, Variable::pointer> Variables;
	Variables variables_;

	Environment() {}

	public:
	typedef boost::shared_ptr<Environment> pointer;
	typedef boost::shared_ptr<const Environment> const_pointer;

	std::string subst(const std::string&) const;
	Variable::const_pointer operator[](const std::string str) const
	{
		Variables::const_iterator iter = variables_.find(str);
		if(iter != variables_.end())
			return iter->second;
		else
			return Variable::pointer();
	}
	Variable::pointer& operator[](const std::string str) { return variables_[str]; }

	static pointer create() { return pointer(new Environment()); }
	pointer override() const { return pointer(new Environment(*this)); }
	pointer clone() const;

	Variables::size_type count(const std::string& key) const { return variables_.count(key); }

	typedef Variables::const_iterator const_iterator;
	typedef Variables::iterator iterator;
	typedef Variables::value_type value_type;
	const_iterator begin() const { return variables_.begin(); }
	iterator begin() { return variables_.begin(); }
	const_iterator end() const { return variables_.end(); }
	iterator end() { return variables_.end(); }
};

std::string parse_variable_ref(const std::string& str);

template<typename T> class SimpleVariable : public Variable
{
	T value_;

	public:
	SimpleVariable(const T& value) : value_(value) {}
	std::string to_string() const { return boost::lexical_cast<std::string>(value_); }
	pointer clone() const { return pointer(new SimpleVariable<T>(value_)); }
	const T& get() const { return value_; }
};

template<typename T> inline Variable::pointer make_variable(const T& value)
{
	return Variable::pointer(new SimpleVariable<T>(value));
}

class CompositeVariable : public Variable
{
	std::vector<Variable::pointer> variables_;

	public:
	CompositeVariable() {}
	template<typename Iterator> CompositeVariable(Iterator start, Iterator end)
	{
		std::transform(start, end, back_inserter(variables_), make_variable<typename Iterator::value_type>);
	}
	std::string to_string() const;
	pointer clone() const;

	void push_back(const Variable::pointer& var)
	{
		variables_.push_back(var);
	}

	typedef std::vector<Variable::pointer>::iterator iterator;
	typedef std::vector<Variable::pointer>::const_iterator const_iterator;
	iterator begin() { return variables_.begin(); }
	iterator end() { return variables_.end(); }
	const_iterator begin() const { return variables_.begin(); }
	const_iterator end() const { return variables_.end(); }
	size_t size() const { return variables_.size(); }
};
template<typename Iterator> inline Variable::pointer make_variable(Iterator start, Iterator end)
{
	return Variable::pointer(new CompositeVariable(start, end));
}

}

#endif
