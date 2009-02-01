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

#include <boost/algorithm/string/predicate.hpp>
#include <boost/cast.hpp>

#include "builder_wrapper.hpp"
#include "fs_node.hpp"
#include "python_interface/environment_wrappers.hpp"

using std::string;
using boost::polymorphic_cast;

namespace python_interface
{

class factory_wrapper
{
	object factory_;
	public:
	factory_wrapper(object factory) : factory_(factory) {}
};

class PythonBuilder : public builder::Builder
{
	class FactoryWrapper
	{
		object factory_;
		object prefix_;
		object suffix_;
		public:
		FactoryWrapper(object factory, object prefix, object suffix) : factory_(factory), prefix_(prefix), suffix_(suffix) {}
		dependency_graph::Node operator()(const environment::Environment& env, std::string name)
		{
			if(prefix_)
				name = env.subst(extract<std::string>(prefix_)()) + name;
			if(suffix_) {
				std::string suffix = extract<std::string>(suffix_);
				suffix = env.subst(suffix);
				if(!suffix.empty() && !boost::algorithm::starts_with(suffix, "."))
					suffix = "." + suffix;
				if(!boost::algorithm::ends_with(name, suffix))
					name += suffix;
			}

			if(factory_)
				return extract<python_interface::NodeWrapper>(factory_(name))().node;
			else
				return dependency_graph::add_entry_indeterminate(name);
		}

		object suffix() const { return suffix_; }
		object prefix() const { return prefix_; }
	};

	object actions_;

	FactoryWrapper target_factory_;
	FactoryWrapper source_factory_;

	object emitter_;

	public:
	PythonBuilder(object actions, object target_factory, object source_factory, object prefix, object suffix, object src_suffix, object emitter) :
		actions_(actions),
		target_factory_(target_factory, prefix, suffix),
		source_factory_(source_factory, object(), src_suffix),
		emitter_(emitter)
	{}

	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const dependency_graph::NodeList& targets,
		const dependency_graph::NodeList& sources
		) const
	{
		std::deque<action::Action::pointer> actions;
		object actions_obj = list();

		dependency_graph::NodeList emitted_targets;
		dependency_graph::NodeList emitted_sources;
		object emitter = emitter_;
		if(emitter_) {
			if(is_dict(emitter_)) {
				emitter = dict(emitter_)[get_properties<dependency_graph::FSEntry>(sources[0])->suffix()];
			}
			if(is_string(emitter)) {
				string var_ref = extract<string>(emitter);
				string var = environment::parse_variable_ref(var_ref);
				if(var.empty()) {
					PyErr_SetString(PyExc_ValueError, string(
						"Failed to expand emitter passed as string '" + var_ref + "' to a python callable.").c_str());
					throw_error_already_set();
				}
				try {
					emitter = polymorphic_cast<const PythonVariable*>(env[var].get())->get();
				} catch(const std::bad_cast&) {}
			}
		}
		if(is_callable(emitter)) {
			tuple emitter_result = tuple(emitter(object(targets), object(sources), object(env)));
			extract_nodes(emitter_result[0], back_inserter(emitted_targets));
			extract_nodes(emitter_result[1], back_inserter(emitted_sources));
		} else {
			emitted_targets = targets;
			emitted_sources = sources;
		}

		if(is_dict(actions_)) {
			if(!sources.empty()) {
				actions_obj = dict(actions_)[get_properties<dependency_graph::FSEntry>(sources[0])->suffix()];
			}
		} else {
			actions_obj = actions_;
		}
		foreach(const object& action, make_object_iterator_range(make_actions(actions_obj)))
			actions.push_back(extract<action::Action::pointer>(action));

		create_task(env, emitted_targets, emitted_sources, actions);
		return emitted_targets;
	}

	NodeFactory target_factory() const { return target_factory_; }
	NodeFactory source_factory() const { return source_factory_; }

	friend object add_action(builder::Builder* builder, object, object);
	friend object add_emitter(builder::Builder* builder, object, object);

	object suffix() const { return target_factory_.suffix(); }
	object prefix() const { return target_factory_.prefix(); }
	object src_suffix() const { return source_factory_.suffix(); }

};

object make_builder(const tuple&, const dict& kw)
{
	return object(builder::Builder::pointer(new
				PythonBuilder(
					kw.get("action"),
					kw.get("target_factory"),
					kw.get("source_factory"),
					kw.get("prefix"),
					kw.get("suffix"),
					kw.get("src_suffix"),
					kw.get("emitter")
	)));
}

object add_action(builder::Builder* builder, object suffix, object action)
{
	boost::polymorphic_cast<PythonBuilder*>(builder)->actions_[suffix] = action;
	return object();
}

object add_emitter(builder::Builder* builder, object suffix, object emitter)
{
	boost::polymorphic_cast<PythonBuilder*>(builder)->emitter_[suffix] = emitter;
	return object();
}

object get_builder_suffix(builder::Builder* builder)
{
	return boost::polymorphic_cast<PythonBuilder*>(builder)->suffix();
}

object get_builder_prefix(builder::Builder* builder)
{
	return boost::polymorphic_cast<PythonBuilder*>(builder)->prefix();
}

object get_builder_src_suffix(builder::Builder* builder)
{
	return boost::polymorphic_cast<PythonBuilder*>(builder)->src_suffix();
}

}
