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

#include "fs_node.hpp"
#include "util.hpp"

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

namespace
{
	boost::filesystem::path fs_root;
}

namespace dependency_graph
{

FS fs;

void set_fs_root(const path& path)
{
	static bool path_is_set = false;
	if(!path_is_set) {
		fs_root = path;
		path_is_set = true;
	}
	else
		throw std::runtime_error("set_fs_root: path can be set only once");

	assert(fs_root.is_complete());
}

Node add_entry(const std::string& name, boost::logic::tribool is_file)
{
	path filename = util::canonicalize(system_complete(path(name)));
	if(!fs_root.empty())
		filename = util::to_relative(filename, fs_root);

	Node file;
	FS::iterator file_iter = fs.find(filename);
	if(file_iter == fs.end()) {
		file = add_vertex(graph);
		graph[file].reset(new FSEntry(filename.string(), is_file));
		fs[filename] = file;
	} else {
		file = file_iter->second;
	}
	return file;
}

FSEntry::FSEntry(path name, boost::logic::tribool is_file) : path_(name), is_file_(is_file)
{
	if(name.is_complete())
		abspath_ = path_;
	else
		abspath_ = fs_root / path_;
}

bool FSEntry::unchanged(const NodeList& targets) const
{
	std::time_t source_timestamp = timestamp();
	bool up_to_date = true;
	foreach(Node target, targets) {
		try {
			if(
				!properties<FSEntry>(target).exists() ||
				(source_timestamp > properties<FSEntry>(target).timestamp())
			  )
				up_to_date = false;
		} catch(const std::bad_cast&) {
			up_to_date = false;
		}
	}
	if(up_to_date)
		std::cout << name() << " is unchanged." << std::endl;
	return up_to_date;
}

}
