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

#ifndef FS_NODE_HPP
#define FS_NODE_HPP

#include <boost/logic/tribool.hpp>
#include <boost/filesystem.hpp>

#include "dependency_graph.hpp"

namespace dependency_graph
{

using boost::filesystem::path;

typedef std::map<path, Node> FS;
extern FS fs;

class FSEntry : public node_properties
{
	path path_;
	path abspath_;
	boost::logic::tribool is_file_;
	public:
	FSEntry(const std::string name, boost::logic::tribool is_file = boost::logic::indeterminate);
	std::string name() const { return is_file_ ? path_.file_string() : path_.directory_string(); }
	bool up_to_date(const NodeList&) const;

	boost::logic::tribool is_file() const { return is_file_; }
	void make_file() { is_file_ = true; }
	void make_directory() { is_file_ = false; }

	std::string dir() const { return path_.parent_path().directory_string(); }
	std::string file() const { return path_.filename(); }
	std::string suffix() const { return path_.extension(); }
	std::string base() const { return name().substr(0, name().length() - suffix().length()); }
	std::string filebase() const { return file().substr(0, file().length() - suffix().length()); }

	bool exists() const { return boost::filesystem::exists(abspath_); }
	std::time_t timestamp() const { return boost::filesystem::last_write_time(abspath_); }
};

void set_fs_root(const path& path);

Node add_entry(const std::string& name, boost::logic::tribool is_file);

inline Node add_entry_indeterminate(const std::string& name)
{
	return add_entry(name, boost::logic::indeterminate);
}

inline Node add_file(const std::string& name)
{
	return add_entry(name, true);
}

inline Node add_directory(const std::string& name)
{
	return add_entry(name, false);
}

}

#endif
