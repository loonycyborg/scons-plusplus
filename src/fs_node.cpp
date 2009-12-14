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

#include <fnmatch.h>
#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

namespace
{

using boost::optional;
using boost::filesystem::path;
using std::map;
using std::string;
using dependency_graph::Node;
using dependency_graph::NodeList;
using dependency_graph::graph;

struct fs_trie
{
	optional<Node> node;
	map<string, fs_trie> children;
	typedef map<string, fs_trie>::iterator child_iterator;
	typedef map<string, fs_trie>::const_iterator const_child_iterator;

	Node add_entry(const path& p, boost::logic::tribool is_file)
	{
		path::const_iterator iter = p.begin();
		return add_entry(iter, p, is_file);
	}
	Node add_entry(path::const_iterator& iter, const path& entry_path, boost::logic::tribool is_file)
	{
		if(iter == entry_path.end()) {
			if(!node) {
				node = add_vertex(graph);
				graph[node.get()].reset(new dependency_graph::FSEntry(entry_path.string(), is_file));
			}
			return node.get();
		} else {
			string elem = *iter;
			return children[elem].add_entry(++iter, entry_path, is_file);
		}
	}
	optional<Node> get(const path& p) const
	{
		path::const_iterator iter = p.begin();
		return get(iter, p.end());
	}
	optional<Node> get(path::const_iterator& iter, const path::const_iterator& iter_end) const
	{
		if(iter == iter_end)
			return node;

		const_child_iterator i = children.find(*iter);
		if(i == children.end())
			return optional<Node>();
		return i->second.get(++iter, iter_end);
	}
	NodeList glob(const path& pattern) const
	{
		path::const_iterator iter = pattern.begin();
		NodeList result;
		glob(iter, pattern.end(), result);
		return result;
	}
	void glob(path::const_iterator& iter, const path::const_iterator& iter_end, NodeList& result) const
	{
		if(iter == iter_end) {
			if(node) {
				result.push_back(node.get());
			}
			return;
		}

		const char* pattern = iter->c_str();
		path::iterator next_pattern = ++iter;
		for(const_child_iterator i = children.begin(); i != children.end(); ++i) {
			if(fnmatch(pattern, i->first.c_str(), FNM_NOESCAPE) == 0) {
				i->second.glob(next_pattern, iter_end, result);
			}
		}
	}
	void dump(int nesting_level = 0) const
	{
		if(node)
			std::cout << node.get();
		else
			std::cout << "N";
		std::cout << std::endl;
		nesting_level++;
		for(const_child_iterator i = children.begin(); i != children.end(); ++i) {
			for(int j = 0; j < nesting_level; j++)
				std::cout << '-';
			std::cout << i->first << ' ';
			i->second.dump(nesting_level);
		}
	}
};

boost::filesystem::path fs_root;
fs_trie fs;

}

namespace dependency_graph
{

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
	path filename;
	if(name[0] == '#')
		filename = fs_root / name.substr(1);
	else
		filename = system_complete(path(name));
	filename = util::canonicalize(filename);
	if(!fs_root.empty())
		filename = util::to_relative(filename, fs_root);

	return fs.add_entry(filename, is_file);
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
