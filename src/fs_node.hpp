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
#include "node_properties.hpp"

namespace sconspp
{

using boost::filesystem::path;

class FSEntry : public node_properties
{
	path path_;
	path abspath_;
	boost::logic::tribool is_file_;
	mutable boost::optional<bool> unchanged_;
	public:
	FSEntry(path name, boost::logic::tribool is_file = boost::logic::indeterminate);
	std::string name() const { return path_.string(); }
	std::string abspath() const { return abspath_.string(); }
	std::string relpath() const;
	const char* type() const { return "fs"; }

	bool unchanged(PersistentNodeData&) const;
	bool needs_rebuild() const { return always_build_ || !exists(); }

	boost::logic::tribool is_file() const { return is_file_; }
	void make_file() { is_file_ = true; }
	void make_directory() { is_file_ = false; }

	std::string dir() const { return path_.parent_path().string(); }
	std::string file() const { return path_.filename().string(); }
	std::string suffix() const { return path_.extension().string(); }
	std::string base() const { return name().substr(0, name().length() - suffix().length()); }
	std::string filebase() const { return file().substr(0, file().length() - suffix().length()); }

	bool exists() const { return boost::filesystem::exists(abspath_); }
	std::time_t timestamp() const { return boost::filesystem::last_write_time(abspath_); }

	std::string get_contents() const;

	private:
	enum class deletion_policy {
		precious,
		on_fail
	} deletion_policy_ = deletion_policy::on_fail;
	public:
	void precious() { deletion_policy_ = deletion_policy::precious; }

	enum class change_detection {
		timestamp_md5,
		timestamp_match
	} change_detection = change_detection::timestamp_md5;

	void was_rebuilt(int status)
	{
		unchanged_.reset();
		if(deletion_policy_ == deletion_policy::on_fail && status != 0)
			boost::filesystem::remove(abspath_);
	}
	void record_persistent_data(PersistentNodeData&);
};

void set_fs_root(const path& path);

Node add_entry(const std::string& name, boost::logic::tribool is_file);
boost::optional<Node> get_entry(const std::string& name);
boost::optional<Node> find_file(const std::string& name, const std::vector<std::string>& directories, bool cached = false);
NodeList glob(const std::string& pattern, bool on_disk = true);

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
