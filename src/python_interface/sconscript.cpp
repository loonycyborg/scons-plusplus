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
#include "sconscript.hpp"

#include <boost/filesystem/operations.hpp>

#include "fs_node.hpp"
#include "util.hpp"

using std::string;

namespace
{
	boost::filesystem::path sconstruct_file, sconstruct_dir;

	dict exports;

	class SConscriptFile
	{
		boost::filesystem::path path_, dir_;
		SConscriptFile* old_sconscript;

		object& ns_;

		static SConscriptFile* current_sconscript;

		public:
		SConscriptFile(const boost::filesystem::path& new_sconscript_file, object& ns) 
			: path_(new_sconscript_file), dir_(path_.parent_path()), ns_(ns)
		{
			old_sconscript = current_sconscript;
			current_sconscript = this;
		}
		~SConscriptFile()
		{
			current_sconscript = old_sconscript;
		}

		static SConscriptFile& current() { return *current_sconscript; }

		boost::filesystem::path path() const { return path_; }
		boost::filesystem::path dir() const { return dir_; }
		object& ns() { return ns_; }
	};

	SConscriptFile* SConscriptFile::current_sconscript;
}

namespace python_interface
{

void SConscript(const std::string& script)
{
	object ns = copy(main_namespace);

	if(sconstruct_file.empty()) {
		sconstruct_file = system_complete(boost::filesystem::path(script));
		sconstruct_dir = boost::filesystem::current_path();
		dependency_graph::set_fs_root(sconstruct_dir);
	}
	SConscriptFile sconscript_file(system_complete(boost::filesystem::path(script)), ns);

	util::scoped_chdir chdir(SConscriptFile::current().dir());

	exec_file(SConscriptFile::current().path().string().c_str(), ns, ns);
}

void SConscript(const environment::Environment&, const std::string& script)
{
	SConscript(script);
}

object Export(tuple args, dict kw)
{
	foreach(object var, make_object_iterator_range(flatten(args))) {
		string name = extract<string>(var);
		exports[name] = SConscriptFile::current().ns()[name];
	}
	return object();
}

object Import(tuple args, dict kw)
{
	foreach(object var, make_object_iterator_range(flatten(args))) {
		string name = extract<string>(var);
		SConscriptFile::current().ns()[name] = exports[name];
	}
	return object();
}

}
