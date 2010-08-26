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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <boost/version.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "util.hpp"

using std::string;

namespace util
{

boost::scoped_ptr<Environ> Environ::environ_;

Environ::Environ()
{
	for(int i = 0; environ[i] != NULL; i++) {
		string var(environ[i]);
		size_t pos = var.find('=');
		if(pos == string::npos)
			vars_.insert(std::make_pair(var, string()));
		else
			vars_.insert(std::make_pair(var.substr(0, pos), var.substr(pos+1, string::npos)));
	}
}

boost::filesystem::path where_is(const std::string& name)
{
	if(boost::filesystem::path(name).is_complete())
		return name;
	std::vector<string> env_path;
	string path_str = Environ::instance().get("PATH");
	boost::algorithm::split(env_path, path_str, boost::is_any_of(":"));
	foreach(const string& path_item, env_path) {
		boost::filesystem::path path = boost::filesystem::path(path_item) / name;
		if(boost::filesystem::exists(path))
			return path;
	}
	return string();
}

boost::filesystem::path readlink(const boost::filesystem::path& path)
{
	char buf[5000];
	size_t n = ::readlink(path.string().c_str(), buf, 5000);
	if(n == static_cast<size_t>(-1))
		throw boost::system::error_code(errno, boost::system::get_system_category());
	std::string name(buf, n);
	return name;
}

boost::filesystem::path to_relative(const boost::filesystem::path& p, const boost::filesystem::path& base)
{
	assert(p.is_complete());
	assert(base.is_complete());
	boost::filesystem::path::iterator path_iter = p.begin(), base_iter = base.begin();
	for(;; ++path_iter, ++base_iter) {
		if(path_iter == p.end()) {
			return p;
		}
		if(base_iter == base.end()) {
			boost::filesystem::path result;
			for(;path_iter != p.end(); ++path_iter) {
				result /= *path_iter;
			}
			return result;
		}
		if(*path_iter != *base_iter) {
			return p;
		}
	}
}

boost::filesystem::path canonicalize(const boost::filesystem::path& p)
{
	std::vector<boost::filesystem::path::iterator> canonicalized_path;
	for(boost::filesystem::path::iterator i = p.begin(); i != p.end(); ++i) {
		if(*i == ".")
			continue;
		if(*i == "..") {
			canonicalized_path.pop_back();
			continue;
		}
		canonicalized_path.push_back(i);
	}
	boost::filesystem::path result;
	foreach(const boost::filesystem::path::iterator& elem, canonicalized_path)
		result /= *elem;
	return result;
}

inline int throw_if_error(int return_value)
{
	if(return_value == -1)
#if BOOST_VERSION < 104400
		throw boost::system::system_error(errno, boost::system::system_category);
#else
		throw boost::system::system_error(errno, boost::system::system_category());
#endif
	return return_value;
}

scoped_chdir::scoped_chdir(const boost::filesystem::path& dir)
{
	old_current_dir = boost::filesystem::current_path();
	boost::filesystem::current_path(dir);
}
scoped_chdir::~scoped_chdir()
{
	boost::filesystem::current_path(old_current_dir);
}

void exec(const std::vector<string>& args)
{
	std::vector<char*> argv;
	foreach(const string& arg, args)
		argv.push_back(const_cast<char*>(arg.c_str()));
	argv.push_back(NULL);

	boost::filesystem::path binary = where_is(args[0]);
	if(binary.empty())
		throw std::runtime_error("util::exec : Failed to find executable: " + args[0]);
	throw_if_error(access(binary.file_string().c_str(), X_OK));

	pid_t child = throw_if_error(fork());
	if(child == 0)
		throw_if_error(execvp(binary.file_string().c_str(), &argv[0]));
	else {
		int status;
		throw_if_error(waitpid(child, &status, 0));
		if(WIFEXITED(status)) {
			if(WEXITSTATUS(status) != 0) {
				throw std::runtime_error(
					"util::exec : " + args[0] + " exited with status " + boost::lexical_cast<std::string>(WEXITSTATUS(status))
				);
			} else {
				return;
			}
		}
		if(WIFSIGNALED(status))
			throw std::runtime_error(
				"util::exec : " + args[0] + " terminated by signal: " + strsignal(WTERMSIG(status))
			);
		throw std::runtime_error("util::exec : " + args[0] + " exited abnormally");
	}
}

boost::array<unsigned char, 16> util::MD5::hash_file(const std::string& filename)
{
	MD5 md5;
	FILE* file = fopen(filename.c_str(), "r");
	if(file == NULL) throw boost::system::system_error(errno, boost::system::get_system_category(), "util::MD5::hash_file: Failed to open " + filename);
	while(!feof(file)) {
		const int blocksize = 4096;
		unsigned char buffer[blocksize];

		size_t count = fread(buffer, 1, blocksize, file);
		if(ferror(file)) throw boost::system::system_error(errno, boost::system::get_system_category(), "util::MD5::hash_file: Failed to read " + filename);

		md5.append(buffer, count);
	}
	return md5.finish();
}

}
