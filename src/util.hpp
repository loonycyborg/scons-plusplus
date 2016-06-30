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

#ifndef UTIL_HPP
#define UTIL_HPP

#include <map>
#include <string>
#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/array.hpp>
#include "md5.h"

namespace sconspp
{

class Environ
{
	static boost::scoped_ptr<Environ> environ_;
	Environ();

	std::map<std::string, std::string> vars_;

	public:
	static const Environ& instance()
	{
		if(!environ_)
			environ_.reset(new Environ);
		return *environ_;
	}

	typedef std::map<std::string, std::string>::const_iterator const_iterator;
	typedef std::map<std::string, std::string>::value_type value_type;

	const_iterator begin() const { return vars_.begin(); }
	const_iterator end() const { return vars_.end(); }
	const_iterator find(const std::string& key) const { return vars_.find(key); }
	std::string get(const std::string& key, const std::string& default_value = "") const
	{
		const_iterator loc = find(key);
		if(loc == end())
			return default_value;
		else
			return loc->second;
	}
};

boost::filesystem::path where_is(const std::string& name);

boost::filesystem::path readlink(const boost::filesystem::path& path);

boost::filesystem::path to_relative(const boost::filesystem::path& p, const boost::filesystem::path& base);
boost::filesystem::path canonicalize(const boost::filesystem::path& p);

class scoped_chdir
{
	boost::filesystem::path old_current_dir;
	public:
	scoped_chdir(const boost::filesystem::path&);
	~scoped_chdir();
};

int exec(const std::vector<std::string>&);

class MD5
{
	md5_state_t state;

	public:
	MD5()
	{
		md5_init(&state);
	}
	void append(const unsigned char* data, size_t length)
	{
		md5_append(&state, data, length);
	}
	void append(const std::string& data)
	{
		md5_append(&state, (const unsigned char*)data.data(), data.size());
	}
	boost::array<unsigned char, 16> finish()
	{
		boost::array<unsigned char, 16> result;
		md5_finish(&state, result.c_array());
		return result;
	}
	static boost::array<unsigned char, 16> hash(const std::string& str)
	{
		MD5 md5;
		md5.append((const unsigned char*)str.data(), str.length());
		return md5.finish();
	}
	static boost::array<unsigned char, 16> hash_file(const std::string& filename);
};

}

#endif
