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

#include "dependency_graph.hpp"
#include "node_properties.hpp"
#include "python_interface/python_interface.hpp"
#include "write_dot.hpp"
#include "taskmaster.hpp"

#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

using dependency_graph::Graph;
using dependency_graph::Node;

int main(int argc, char** argv)
{
	python_interface::init_python();
	const char* default_script_names[] = { "SConstruct++", "SConstruct" };
	bool script_found = false;
	foreach(const char* name, default_script_names) {
		if(boost::filesystem::exists(name)) {
			script_found = true;
			python_interface::run_script(name, argc, argv);
			break;
		}
	}
	if(!script_found)
		std::cerr << "scons++: *** No SConstruct file found." << std::endl;
	
	{
	std::ofstream dot_file("graph.dot");
	visualization::write_dot(dot_file, dependency_graph::graph);
	}

	if(dependency_graph::default_targets.size())
	{
		Node default_target = dependency_graph::add_dummy_node("The end goal");
		foreach(Node node, dependency_graph::default_targets)
			add_edge(default_target, node, dependency_graph::graph);
		try {
			taskmaster::build(default_target);
		} catch(const std::exception& e) {
			std::cerr << "scons++: *** " << e.what() << std::endl;
			return 1;
		}
	}
}
