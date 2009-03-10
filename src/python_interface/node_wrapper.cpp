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

namespace python_interface
{

NodeList extract_file_nodes(const environment::Environment& env, object obj)
{
	NodeList result;
	foreach(object node, make_object_iterator_range(obj))
		if(is_string(node)) {
			result.push_back(dependency_graph::add_entry_indeterminate(extract_string_subst(env, node)));
		} else {
			result.push_back(extract_node(node));
		}
	return result;
}

builder::Builder::NodeStringList::value_type extract_node(const environment::Environment& env, object obj)
{
	if(is_string(obj))
		return extract_string_subst(env, obj);
	return extract_node(obj);
}

builder::Builder::NodeStringList extract_nodes(const environment::Environment& env, object obj)
{
	builder::Builder::NodeStringList result;
	foreach(object node, make_object_iterator_range(obj))
		result.push_back(extract_node(env, node));
	return result;
}

}
