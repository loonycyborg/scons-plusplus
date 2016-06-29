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

#ifndef DEPENDENCY_GRAPH_HPP
#define DEPENDENCY_GRAPH_HPP

#include <boost/graph/adjacency_list.hpp>
#include <boost/shared_ptr.hpp>

namespace sconspp
{

using boost::adjacency_list;
using boost::vecS;
using boost::setS;
using boost::listS;
using boost::directedS;
using boost::graph_traits;
using boost::add_vertex;
using boost::add_edge;

class node_properties;
typedef boost::adjacency_list<setS, listS, directedS, boost::shared_ptr<node_properties> > Graph;
typedef graph_traits<Graph>::vertex_descriptor Node;
typedef graph_traits<Graph>::edge_descriptor Edge;
typedef std::vector<Node> NodeList;

extern Graph graph;
extern std::set<Node> default_targets;

}

#endif
