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

NodeList call_builder_interface(tuple args, dict kw)
{
	const builder::Builder& builder = extract<const builder::Builder&>(args[0]);
	const environment::Environment& env = extract<const environment::Environment&>(args[1]);
	object target, source;
	if(len(args) >= 3) {
		if(kw.has_key("target")) {
			PyErr_SetString(PyExc_ValueError, "target specified as both keyword and positional argument");
			throw_error_already_set();
		} else {
			target = args[2];
		}
	} else {
		target = kw.get("target");
	}
	if(len(args) >= 4) {
		if(kw.has_key("source")) {
			PyErr_SetString(PyExc_ValueError, "source specified as both keyword and positional argument");
			throw_error_already_set();
		} else {
			source = args[3];
		}
	} else {
		source = kw.get("source");
	}
	if(!source) {
		source = target;
	}
	return call_builder(builder, env, target, source);
}

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
		public:
		FactoryWrapper(object factory)
			: factory_(factory) {}
		dependency_graph::Node operator()(const environment::Environment& env, std::string name)
		{
			if(factory_)
				return extract<python_interface::NodeWrapper>(factory_(name))().node;
			else
				return dependency_graph::add_entry_indeterminate(name);
		}
	};

	object actions_;

	FactoryWrapper target_factory_;
	FactoryWrapper source_factory_;

	object prefix_;
	object suffix_;
	bool ensure_suffix_;
	object src_suffix_;

	object emitter_;

	object src_builder_;

	public:
	PythonBuilder(
			object actions,
			object target_factory, object source_factory, object prefix, object suffix, bool ensure_suffix, object src_suffix,
			object emitter, object src_builder) :
		actions_(actions),
		target_factory_(target_factory),
		source_factory_(source_factory),
		prefix_(prefix),
		suffix_(suffix),
		ensure_suffix_(ensure_suffix),
		src_suffix_(src_suffix),
		emitter_(emitter),
		src_builder_(src_builder)
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
	std::string adjust_target_name(const environment::Environment& env, const std::string& name) const
	{
		std::string result,
			prefix = prefix_ ? env.subst(extract<std::string>(prefix_)()) : std::string(),
			suffix = suffix_ ? env.subst(extract<std::string>(suffix_)()) : std::string();
		result = adjust_affixes(env.subst(name), prefix, suffix, ensure_suffix_);
		return result;
	}

	std::string adjust_source_name(const environment::Environment& env, const std::string& name) const
	{
		if(name.find('.') == std::string::npos) {
			std::string suffix = src_suffix_ ? env.subst(extract<std::string>(flatten(src_suffix_)[0])()) : std::string();
			return adjust_affixes(env.subst(name), std::string(), suffix);
		}
		return name;
	}
	std::string adjust_affixes(const std::string& name, const std::string& prefix, const std::string& suffix, bool ensure_suffix = false) const
	{
		std::string full_suffix, result = name;
		if(!suffix.empty() && !boost::algorithm::starts_with(suffix, "."))
			full_suffix = "." + suffix;
		else
			full_suffix = suffix;
		if(!full_suffix.empty() && !boost::algorithm::ends_with(name, full_suffix))
			if(ensure_suffix || !boost::algorithm::contains(name, string(".")))
				result += full_suffix;

		result = prefix + result;
		return result;
	}
	bool source_ext_match(const environment::Environment& env, const std::string& name) const
	{
		std::set<std::string> suffixes;
		foreach(object suffix, make_object_iterator_range(flatten(src_suffix_))) {
			suffixes.insert(env.subst(extract<std::string>(suffix)));
		}
		std::string suffix = name.substr(name.rfind('.'));
		return suffixes.find(suffix) != suffixes.end();
	}

	friend object add_action(builder::Builder* builder, object, object);
	friend object add_emitter(builder::Builder* builder, object, object);

	object suffix() const { return suffix_; }
	object prefix() const { return prefix_; }
	object src_suffix() const { return src_suffix_; }
	object src_builder() const { return src_builder_; }

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
					kw.get("ensure_suffix"),
					kw.get("src_suffix"),
					kw.get("emitter"),
					kw.get("src_builder")
	)));
}

template<class OutputIterator>
inline void target_string2node(const std::string& name, OutputIterator iter, const builder::Builder* builder, const environment::Environment& env)
{
	*iter++ = builder->target_factory()(env, transform_node_name(builder->adjust_target_name(env, name)));
}

template<class OutputIterator>
inline void source_string2node(const std::string& name, OutputIterator iter, const builder::Builder* builder, const environment::Environment& env)
{
	try {
		const PythonBuilder* python_builder = boost::polymorphic_cast<const PythonBuilder*>(builder);
		object src_builder = python_builder->src_builder();
		if(src_builder) {
			if(!python_builder->source_ext_match(env, name)) {
				NodeList nodes;
				if(is_string(src_builder)) {
					src_builder = object(env).attr(src_builder);
					extract_nodes(src_builder(str(name.substr(0, name.rfind('.'))), str(name)), back_inserter(nodes));
				} else {
					extract_nodes(src_builder(env, str(name.substr(0, name.rfind('.'))), str(name)), back_inserter(nodes));
				}
				std::copy(nodes.begin(), nodes.end(), iter);
				return;
			}
		}
	} catch(const std::bad_cast&) {
	}
	*iter++ = builder->source_factory()(env, transform_node_name(builder->adjust_source_name(env, name)));
}

template<class OutputIterator> inline void extract_target_nodes(object obj, OutputIterator iter, const builder::Builder& builder, const environment::Environment& env)
{
	extract_nodes(obj, iter, boost::bind(target_string2node<OutputIterator>, _1, iter, &builder, env));
}

template<class OutputIterator> inline void extract_source_nodes(object obj, OutputIterator iter, const builder::Builder& builder, const environment::Environment& env)
{
	extract_nodes(obj, iter, boost::bind(source_string2node<OutputIterator>, _1, iter, &builder, env));
}

NodeList call_builder(const builder::Builder& builder, const environment::Environment& env, object target, object source)
{
	dependency_graph::NodeList targets, sources;
	extract_target_nodes(
		flatten(target), back_inserter(targets), builder, env
		);
	extract_source_nodes(
		flatten(source), back_inserter(sources), builder, env
		);
	return builder(env, targets, sources);
}

object add_action(builder::Builder* builder, object suffix, object action)
{
	PythonBuilder* python_builder = boost::polymorphic_cast<PythonBuilder*>(builder);
	python_builder->actions_[suffix] = action;
	python_builder->src_suffix_ = flatten(python_builder->src_suffix_) + flatten(suffix);
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
