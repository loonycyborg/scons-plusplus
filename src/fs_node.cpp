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
#include "db.hpp"

#include <fnmatch.h>
#include <numeric>
#include <boost/optional.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/unordered_map.hpp>

namespace
{

using boost::optional;
using std::map;
using std::string;
using boost::filesystem::path;
using sconspp::Node;
using sconspp::NodeList;
using sconspp::graph;

struct fs_trie_node
{
	fs_trie_node() : children{{}} { }

	optional<Node> node;
	optional<fs_trie_node&> parent;
	std::unordered_map<string, fs_trie_node*> children;
};

struct fs_trie
{
	typedef std::unordered_map<string, fs_trie_node> Trie;
	Trie trie;

	fs_trie() : trie{{}} {}

	Node add_entry(const path& p, boost::logic::tribool is_file)
	{
		path::const_iterator iter = p.begin();
		return add_entry(iter, p, is_file,  trie["."]);
	}
	Node add_entry(path::const_iterator& iter, const path& entry_path, boost::logic::tribool is_file, fs_trie_node& parent)
	{
		if(iter == entry_path.end() || *iter == ".") {
			if(!parent.node) {
				parent.node = add_vertex(graph);
				graph[parent.node.get()].reset(new sconspp::FSEntry(entry_path.string(), is_file));
			}
			return parent.node.get();
		} else {
			string elem { iter->string() };
			++iter;
			if(parent.children.count(elem) == 0) {
				path new_path = std::accumulate(entry_path.begin(), iter, path{}, [](path x, path y) -> path { return x/y; });
				parent.children[elem] = &trie[new_path.string()];
				parent.children[elem]->parent = parent;
			}
			return add_entry(iter, entry_path, is_file, *parent.children[elem]);
		}
	}

	optional<Node> get(const path& p) const {
		return trie.count(p.string()) ? trie.find(p.string())->second.node : optional<Node>{};
	}

	NodeList glob(const path& pattern) const
	{
		path::const_iterator iter = pattern.begin();
		NodeList result;
		glob(iter, pattern.end(), result, trie.find(".")->second);
		return result;
	}
	void glob(path::const_iterator& iter, const path::const_iterator& iter_end, NodeList& result, const fs_trie_node& parent) const
	{
		if(iter == iter_end) {
			if(parent.node) {
				result.push_back(parent.node.get());
			}
			return;
		}

		std::string pattern = iter->string();
		path::iterator next_pattern = ++iter;
		for(auto elem : parent.children) {
			if(fnmatch(pattern.c_str(), elem.first.c_str(), FNM_NOESCAPE) == 0) {
				glob(next_pattern, iter_end, result, *elem.second);
			}
		}
	}

	NodeList glob_on_disk(const path& pattern, const path& directory)
	{
		path::const_iterator iter = pattern.begin();
		NodeList result;
		if(pattern.is_complete())
			glob_on_disk(++iter, pattern.end(), *(directory.begin()));
		else
			glob_on_disk(iter, pattern.end(), directory);
		iter = pattern.begin();
		glob(iter, pattern.end(), result, trie["."]);
		return result;
	}
	void glob_on_disk(path::const_iterator& iter, const path::const_iterator& iter_end, const path& directory)
	{
		if(iter == iter_end) {
			sconspp::add_entry(directory.string(),  boost::logic::indeterminate);
			return;
		}

		std::string pattern = iter->string();
		path::iterator next_pattern = ++iter;
		using boost::filesystem::directory_iterator;
		for(directory_iterator i(directory); i != directory_iterator(); ++i) {
			if(fnmatch(pattern.c_str(), i->path().filename().c_str(), FNM_NOESCAPE) == 0) {
				glob_on_disk(next_pattern, iter_end, *i);
			}
		}
	}
};

boost::filesystem::path fs_root;
fs_trie fs;

}

namespace sconspp
{

void set_fs_root(const path& path)
{
	static bool path_is_set = false;
	if(!path_is_set) {
		fs_root = path.lexically_normal();
		path_is_set = true;
	}
	else
		throw std::runtime_error("set_fs_root: path can be set only once");

	assert(fs_root.is_complete());
}

path canonical_path(const path& name)
{
	path result = name.lexically_normal();
	if(!fs_root.empty()) {
		auto relative = result.lexically_relative(fs_root);
		if(*relative.begin() != "..") // we want only paths below buildroot made relative
			result = relative;
	}
	return result;
}

path canonical_path(const std::string& name)
{
	path filename;
	if(name[0] == '#')
		filename = fs_root / name.substr(1);
	else
		filename = system_complete(path(name));
	return canonical_path(filename);
}

Node add_entry(const std::string& name, boost::logic::tribool is_file)
{
	return fs.add_entry(canonical_path(name), is_file);
}
boost::optional<Node> get_entry(const std::string& name)
{
	return fs.get(canonical_path(name));
}

inline boost::optional<Node> find_file_cached(const std::string& name, const std::vector<std::string>& directories)
{
	typedef std::pair<
		std::string, std::vector<std::string>
		> FindPath;
	typedef boost::unordered_map<
		FindPath,
		boost::optional<Node>
		> FindCache;
	static FindCache find_cache;
	FindPath find_path(name, directories);
	FindCache::iterator it = find_cache.find(find_path);
	if(it != find_cache.end())
		return it->second;
	else {
		boost::optional<Node> result = find_file(name, directories);
		find_cache[find_path] = result;
		return result;
	}
}

boost::optional<Node> find_file(const std::string& name, const std::vector<std::string>& directories, bool cached)
{
	if(cached)
		return find_file_cached(name, directories);
	for(const std::string& directory : directories) {
		path p = canonical_path(directory.empty() ? "." : directory) / name;
		boost::optional<Node> result = fs.get(p);
		if(result) return result;
		if(!p.is_complete())
			p = fs_root / p;
		if(exists(p))
			return fs.add_entry(canonical_path(p), boost::logic::indeterminate);
	}
	return boost::optional<Node>();
}

NodeList glob(const std::string& pattern, bool on_disk)
{
	if(on_disk)
		return fs.glob_on_disk(canonical_path(pattern), fs_root);
	else
		return fs.glob(canonical_path(pattern));
}

FSEntry::FSEntry(path name, boost::logic::tribool is_file) : path_(name), is_file_(is_file)
{
	if(name.is_absolute()) {
		abspath_ = path_;
		return;
	}
	if(name.filename_is_dot()) {
		abspath_ = fs_root;
		return;
	}
	abspath_ = fs_root / path_;
}

bool FSEntry::unchanged(PersistentNodeData& prev_data) const
{
	if(!unchanged_ || prev_data.is_archive()) {
		if(exists()) {
			bool timestamp_same = (timestamp() == prev_data.timestamp());
			if(!timestamp_same) prev_data.bump_generation();
			switch(change_detection) {
				case change_detection::timestamp_match:
					unchanged_ = (prev_data.existed() == boost::optional<bool>(true)) && timestamp_same;
				break;

				case change_detection::timestamp_md5:
					unchanged_ = (prev_data.existed() == boost::optional<bool>(true)) &&
						(timestamp_same ||
						MD5::hash_file(abspath_.string()) == prev_data.signature());
				break;
			}
		} else
			unchanged_ = (prev_data.existed() == boost::optional<bool>(false));
	}
	if(!unchanged_.get()) prev_data.bump_generation();
	return unchanged_.get();
}

std::string FSEntry::dir() const {
	path result { path_.parent_path() };
	if(!path_.is_absolute()) {
		if(path_.begin() == --path_.end()) {
			if(path_.filename_is_dot()) result = abspath_.parent_path();
			else result = ".";
		}
	}
	if(result.empty()) throw std::runtime_error("Attempt to get parent dir of a root path");
	return result.string();
}

std::string FSEntry::relpath() const
{
	path relpath = abspath_.lexically_relative(boost::filesystem::current_path());
	return relpath.string();
}

std::string FSEntry::get_contents() const
{
	boost::filesystem::ifstream ifs(abspath_);
	std::ostringstream os;
	os << ifs.rdbuf();
	return os.str();
}

void FSEntry::record_persistent_data(PersistentNodeData& data)
{
	bool entry_exists = exists();
	data.existed() = entry_exists;
	data.timestamp() = entry_exists ? timestamp() : boost::optional<time_t>();
	if(unchanged(data))
		return;
	data.signature() = entry_exists ? MD5::hash_file(abspath_.string()) : boost::optional<boost::array<unsigned char, 16> >();
}

}
