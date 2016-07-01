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
#include "node_wrapper.hpp"
#include "fs_node.hpp"

#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>

namespace sconspp
{
namespace python_interface
{

NodeList extract_file_nodes(const Environment& env, object obj)
{
	NodeList result;
	for(object node : make_object_iterator_range(obj))
		if(is_string(node)) {
			result.push_back(add_entry_indeterminate(extract_string_subst(env, node)));
		} else {
			result.push_back(extract_node(node));
		}
	return result;
}

Builder::NodeStringList extract_nodes(object obj)
{
	Builder::NodeStringList result;
	for(object node : make_object_iterator_range(obj)) {
		if(is_string(node))
			result.push_back(extract<std::string>(node)());
		else
			result.push_back(extract_node(node));
	}
	return result;
}

Builder::NodeStringList extract_nodes(const Environment& env, object obj)
{
	Builder::NodeStringList result;
	for(object node : make_object_iterator_range(obj)) {
		if(is_string(node))
			result.push_back(extract_string_subst(env, node));
		else
			result.push_back(extract_node(node));
	}
	return result;
}

struct ExtractFileVisitor : public boost::static_visitor<Node>
{
	Node operator()(const std::string& name) {
		return add_entry_indeterminate(name);
	}
	Node operator()(Node node) {
		return node;
	}
};

NodeList extract_file_nodes(object obj)
{
	NodeList result;
	Builder::NodeStringList nodes = extract_nodes(obj);
	ExtractFileVisitor visitor;
	std::transform(nodes.begin(), nodes.end(), std::back_inserter(result), boost::apply_visitor(visitor));
	return result;
}

}
}
