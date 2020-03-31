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

#include <pybind11/eval.h>

#include "node_wrapper.hpp"

#include "fs_node.hpp"
#include "util.hpp"

using std::string;

namespace
{
    using namespace sconspp::python_interface;
	boost::filesystem::path sconstruct_file, sconstruct_dir;

	class SConscriptFile
	{
		boost::filesystem::path path_, dir_;
		SConscriptFile* old_sconscript;

		py::object& ns_;
		py::list return_value_;

		static SConscriptFile* current_sconscript;

		py::object exports_;

		public:
		SConscriptFile(const boost::filesystem::path& new_sconscript_file, py::object& ns)
			: path_(new_sconscript_file), dir_(path_.parent_path()), ns_(ns)
		{
			if(current_sconscript == nullptr)
				exports_ = py::dict();
			else
				exports_ = current_sconscript->exports_;
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
		py::object& ns() { return ns_; }
		py::list& return_value() { return return_value_; }

		void export_var(const string& name) {
			exports_[name.c_str()] = ns_[name.c_str()];
		}

		void import_var(const string& name) {
			ns_[name.c_str()] = exports_[name.c_str()];
		}
	};

	SConscriptFile* SConscriptFile::current_sconscript;

	py::object SConscriptReturnException()
	{
		static py::object exception = py::reinterpret_steal<py::object>(PyErr_NewException((char*)"SConspp.SConscriptReturn", NULL, NULL));
		return exception;
	}
}

namespace sconspp
{
namespace python_interface
{

py::object SConscript(const std::string& script)
{
	py::object ns = copy(main_namespace());

	if(sconstruct_file.empty()) {
		sconstruct_file = system_complete(boost::filesystem::path(script));
		sconstruct_dir = boost::filesystem::current_path();
		set_fs_root(sconstruct_dir);
	}
	SConscriptFile sconscript_file(system_complete(boost::filesystem::path(script)), ns);

	scoped_chdir chdir(SConscriptFile::current().dir());

	try {
		py::eval_file(SConscriptFile::current().path().string().c_str(), ns, ns);
	} catch(py::error_already_set& err) {
		if(!err.matches(SConscriptReturnException())) {
			throw;
		}
	}
	return SConscriptFile::current().return_value();
}

py::object SConscript(const Environment&, const std::string& script)
{
	return SConscript(script);
}

void Export(py::args args)
{
	for(auto var : flatten(args)) {
		string name = var.cast<string>();
		SConscriptFile::current().export_var(name);
	}
}

void Import(py::args args)
{
	for(auto var : flatten(args)) {
		string name = var.cast<string>();
		SConscriptFile::current().import_var(name);
	}
}

void Return(py::args args, py::kwargs kw)
{
	py::list& return_value = SConscriptFile::current().return_value();
	for(auto var : flatten(args)) {
		string name = var.cast<string>();
		return_value.append(SConscriptFile::current().ns()[name.c_str()]);
	}
	if(!kw.contains("stop") || kw["stop"].cast<bool>() == true) {
		PyErr_SetObject(SConscriptReturnException().ptr(),  SConscriptReturnException()().ptr());
		throw py::error_already_set();
	}
}

void Default(py::object obj)
{
	if(obj.is_none()) {
		default_targets.clear();
		main_namespace()["DEFAULT_TARGETS"] = py::list();
	}

	obj = flatten(obj);
	NodeList nodes = extract_file_nodes(obj);
	for(Node node : nodes) {
		default_targets.insert(node);
		main_namespace()["DEFAULT_TARGETS"].cast<py::list>().append(NodeWrapper(node));
	}
}

}
}
