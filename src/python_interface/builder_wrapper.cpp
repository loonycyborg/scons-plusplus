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
#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/get.hpp>

#include "builder_wrapper.hpp"
#include "fs_node.hpp"
#include "python_interface/environment_wrappers.hpp"
#include "python_interface/subst.hpp"

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
	if(is_none(source)) {
		source = target;
		target = object();
	}
	return call_builder(builder, env, target, source);
}

inline NodeList extract_file_nodes(const environment::Environment& env, object obj)
{
	NodeList result;
	foreach(object node, make_object_iterator_range(obj))
		if(is_string(node)) {
			result.push_back(dependency_graph::add_entry_indeterminate(extract_string_subst(env, node)));
		} else {
			result.push_back(extract_node(node));
		}
	return result;
}

class PythonScanner
{
	object target_scanner_, source_scanner_;

	void scan(const environment::Environment& env, object& scanner, dependency_graph::Node node, std::set<dependency_graph::Node>& deps) {
		if(scanner) {
			object result = scanner(NodeWrapper(node), env);
			foreach(object node, make_object_iterator_range(result)) {
				dependency_graph::Node dep = extract_node(node);
				if(!deps.count(dep)) {
					deps.insert(dep);
					scan(env, scanner, dep, deps);
				}
			}
		}
	}
	public:
	PythonScanner(object target_scanner, object source_scanner)
	: target_scanner_(target_scanner), source_scanner_(source_scanner)
	{
	}
	void operator()(const environment::Environment& env, dependency_graph::Node target, dependency_graph::Node source)
	{
		std::set<dependency_graph::Node> deps;
		try {
			scan(env, source_scanner_, source, deps);
			scan(env, target_scanner_, target, deps);
			foreach(dependency_graph::Node node, deps)
				add_edge(target, node, dependency_graph::graph);
		} catch(const error_already_set&) {
			throw_python_exc("Exception while running a python scanner: ");
		}
	}
};

class PythonBuilder : public builder::Builder
{
	object actions_;

	object target_factory_;
	object source_factory_;

	object prefix_;
	object suffix_;
	bool ensure_suffix_;
	object src_suffix_;

	object emitter_;

	object src_builder_;
	
	bool single_source_;

	bool multi_;

	taskmaster::Task::Scanner scanner_;

	public:
	PythonBuilder(
			object actions,
			object target_factory, object source_factory, object prefix, object suffix, bool ensure_suffix, object src_suffix,
			object emitter, object src_builder,
			bool single_source,
			bool multi,
			object target_scanner, object source_scanner) :
		actions_(actions),
		target_factory_(target_factory),
		source_factory_(source_factory),
		prefix_(prefix),
		suffix_(suffix),
		ensure_suffix_(ensure_suffix),
		src_suffix_(src_suffix),
		emitter_(emitter),
		src_builder_(src_builder),
		single_source_(single_source),
		multi_(multi),
		scanner_(PythonScanner(target_scanner, source_scanner))
	{}

	dependency_graph::NodeList operator()(
		const environment::Environment& env,
		const NodeStringList& targets,
		const NodeStringList& sources
		) const
	{
		if(single_source_ && (sources.size() > 1)) {
			if(targets.empty()) {
				NodeList result;
				foreach(const NodeStringList::value_type& source, sources) {
					NodeStringList single_source;
					NodeList target;
					single_source.push_back(source);
					target = (*this)(env, targets, single_source);
					std::copy(target.begin(), target.end(), back_inserter(result));
				}
				return result;
			} else {
				throw std::runtime_error("More than one source given to single-source builder.");
			}
		}

		action::ActionList actions;
		object actions_obj = list();

		dependency_graph::NodeList target_nodes;
		dependency_graph::NodeList source_nodes;

		if(targets.empty() && !sources.empty()) {
			const std::string* name = boost::get<const std::string>(&sources[0]);
			if(name) {
				NodeStringList implicit_target;
				implicit_target.push_back(split_ext(*name));
				return (*this)(env, implicit_target, sources);
			}
			const dependency_graph::Node* node = boost::get<const dependency_graph::Node>(&sources[0]);
			if(node) {
				NodeStringList implicit_target;
				try {
					std::string name = properties<dependency_graph::FSEntry>(*node).relpath();
					implicit_target.push_back(split_ext(name));
					return (*this)(env, implicit_target, sources);
				} catch(const std::bad_cast&) {}
			}
		}

		make_node_visitor<&PythonBuilder::make_target_node> target_visitor(env, this, target_nodes);
		make_node_visitor<&PythonBuilder::make_source_node> source_visitor(env, this, source_nodes);

		for_each(targets.begin(), targets.end(), apply_visitor(target_visitor));
		for_each(sources.begin(), sources.end(), apply_visitor(source_visitor));

		object emitter = emitter_;
		if(emitter_) {
			if(is_dict(emitter_)) {
				emitter = dict(emitter_)[properties<dependency_graph::FSEntry>(source_nodes[0]).suffix()];
			}
			if(is_string(emitter)) {
				emitter = subst(env, emitter);
			}
		}
		if(is_callable(emitter)) {
			tuple emitter_result = tuple(emitter(object(target_nodes), object(source_nodes), object(env)));
			NodeList emitted_targets = extract_file_nodes(env, emitter_result[0]);
			NodeList emitted_sources = extract_file_nodes(env, emitter_result[1]);
			target_nodes.swap(emitted_targets);
			source_nodes.swap(emitted_sources);
		}

		if(multi_ && graph[target_nodes[0]]->task()) {
			graph[target_nodes[0]]->task()->add_sources(source_nodes);
		} else {

			if(is_dict(actions_)) {
				if(!sources.empty()) {
					actions_obj = dict(actions_)[properties<dependency_graph::FSEntry>(source_nodes[0]).suffix()];
				}
			} else {
				actions_obj = actions_;
			}
			actions = make_actions(actions_obj);

			create_task(env, target_nodes, source_nodes, actions, scanner_);
		}

		return target_nodes;
	}

	template<void (PythonBuilder::*make_node)(const std::string&, const environment::Environment&, NodeList&) const>
	struct make_node_visitor : public boost::static_visitor<>
	{
		const environment::Environment& env_;
		const PythonBuilder* builder_;
		NodeList& result_;
		make_node_visitor(
			const environment::Environment& env, const PythonBuilder* builder, NodeList& result
			) : env_(env), builder_(builder), result_(result) {}
		void operator()(const dependency_graph::Node& node) const
		{
			result_.push_back(node);
		}
		void operator()(const std::string& name) const
		{
			(builder_->*make_node)(name, env_, result_);
		}
	};

	void make_target_node(const std::string& name, const environment::Environment& env, NodeList& result) const
	{
		std::string full_name = adjust_target_name(env, name);
		if(target_factory_)
			result.push_back(extract_node(target_factory_(full_name)));
		else
			result.push_back(dependency_graph::add_entry_indeterminate(full_name));
	}

	void make_source_node(const std::string& name, const environment::Environment& env, NodeList& result) const
	{
		if(src_builder_) {
			if(!source_ext_match(env, name)) {
				object sources;
				if(is_string(src_builder_)) {
					sources = object(env).attr(src_builder_)(str(name.substr(0, name.rfind('.'))), str(name));
				} else {
					sources = src_builder_(env, str(name.substr(0, name.rfind('.'))), str(name));
				}
				foreach(object node, make_object_iterator_range(sources))
					result.push_back(extract_node(node));
				return;
			}
		}
		std::string full_name = adjust_source_name(env, name);
		if(source_factory_)
			result.push_back(extract_node(source_factory_(full_name)));
		else
			result.push_back(dependency_graph::add_entry_indeterminate(full_name));
	}

	std::string adjust_target_name(const environment::Environment& env, const std::string& name) const
	{
		std::string result,
			prefix = prefix_ ? extract_string_subst(env, prefix_) : std::string(),
			suffix = suffix_ ? extract_string_subst(env, suffix_) : std::string();
		result = adjust_affixes(env.subst(name), prefix, suffix, ensure_suffix_);
		return result;
	}

	std::string adjust_source_name(const environment::Environment& env, const std::string& name) const
	{
		if(name.find('.') == std::string::npos) {
			std::string suffix = src_suffix_ ? extract_string_subst(env, flatten(src_suffix_)[0]) : std::string();
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
			suffixes.insert(extract_string_subst(env, suffix));
		}
		std::string suffix = boost::filesystem::extension(name);
		return suffixes.find(suffix) != suffixes.end();
	}
	std::string split_ext(const boost::filesystem::path& name) const
	{
		return (name.parent_path() / name.stem()).string();
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
					kw.get("src_builder"),
					kw.get("single_source"),
					kw.get("multi"),
					kw.get("target_scanner"),
					kw.get("source_scanner")
	)));
}

NodeList call_builder(const builder::Builder& builder, const environment::Environment& env, object target, object source)
{
	return builder(env, extract_nodes(env, flatten(target)), extract_nodes(env, flatten(source)));
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
