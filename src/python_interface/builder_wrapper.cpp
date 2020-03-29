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
#include "task.hpp"
#include "python_interface/environment_wrappers.hpp"
#include "python_interface/subst.hpp"

using std::string;
using boost::polymorphic_cast;

namespace sconspp
{
namespace python_interface
{

inline NodeList extract_file_nodes(const Environment& env, py::object obj)
{
	NodeList result;
	for(auto node : obj)
		if(py::isinstance<py::str>(node)) {
			result.push_back(add_entry(extract_string_subst(env, py::reinterpret_borrow<py::object>(node))));
		} else {
			result.push_back(extract_node(py::reinterpret_borrow<py::object>(node)));
		}
	return result;
}

class PythonScanner
{
	py::object target_scanner_, source_scanner_;

	void scan(const Environment& env, py::object& scanner, Node node, std::set<Node>& deps, py::object path) {
		if(!scanner.is_none()) {
			py::object result = scanner(NodeWrapper(node), env, path);
			for(auto node : result) {
				Node dep = extract_node(py::reinterpret_borrow<py::object>(node));
				if(!deps.count(dep)) {
					deps.insert(dep);
					scan(env, scanner, dep, deps, path);
				}
			}
		}
	}
	py::object get_path(const Environment& env, py::object& scanner, Node node, py::object cwd, Node target, Node source)
	{
		if(scanner.is_none())
			return py::tuple();
		py::object scan_path = scanner.attr("select")(NodeWrapper(node));
		if(scan_path.is_none())
			scan_path = scanner;
		return scan_path.attr("path")(env, cwd, NodeWrapper(target), NodeWrapper(source));
	}
	public:
	PythonScanner(py::object target_scanner, py::object source_scanner)
	: target_scanner_(target_scanner), source_scanner_(source_scanner)
	{
	}
	void operator()(const Environment& env, Node target, Node source)
	{
		py::gil_scoped_acquire lock {};
		std::set<Node> deps;
		scan(env, source_scanner_, source, deps, get_path(env, source_scanner_, source, py::none(), target, source));
		scan(env, target_scanner_, target, deps, get_path(env, target_scanner_, target, py::none(), target, source));
		for(Node node : deps)
			add_edge(target, node, graph);
	}
};

class PythonBuilder
{
	py::object actions_;

	py::object target_factory_;
	py::object source_factory_;

	py::object prefix_;
	py::object suffix_;
	bool ensure_suffix_;
	py::object src_suffix_;

	py::object emitter_;

	py::object src_builder_;
	
	bool single_source_;

	bool multi_;

	Task::Scanner scanner_;

	public:
	typedef std::shared_ptr<PythonBuilder> pointer;
	PythonBuilder(
			py::object actions,
			py::object target_factory, py::object source_factory, py::object prefix, py::object suffix, bool ensure_suffix, py::object src_suffix,
			py::object emitter, py::object src_builder,
			bool single_source,
			bool multi,
			py::object target_scanner, py::object source_scanner, py::object scanner, py::kwargs overrides) :
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
		scanner_(!scanner.is_none() ? scanner.cast<Task::Scanner>() : PythonScanner(target_scanner, source_scanner))
	{}

	NodeList operator()(
	    const Environment& env,
		py::object targets,
		py::object sources
		) const
	{
		NodeList target_nodes;
		NodeList source_nodes;

		make_node_visitor<&PythonBuilder::make_target_node> target_visitor(env, this, target_nodes);
		make_node_visitor<&PythonBuilder::make_source_node> source_visitor(env, this, source_nodes);

		for(const auto& target : extract_nodes(env, flatten(targets))) apply_visitor(target_visitor, target);
		for(const auto& source : extract_nodes(env, flatten(sources))) apply_visitor(source_visitor, source);

		if(single_source_ && (source_nodes.size() > 1)) {
			if(target_nodes.empty()) {
				NodeList result;
				for(auto source : source_nodes) {
					auto target = (*this)(env, py::cast(NodeList{}), py::cast(NodeList{ source }));
					std::copy(target.begin(), target.end(), back_inserter(result));
				}
				return result;
			} else {
				throw std::runtime_error("More than one source given to single-source builder.");
			}
		}

		ActionList actions;
		py::object actions_obj = py::list();

		if(target_nodes.empty() && !source_nodes.empty()) {
			std::string implicit_target = split_ext(properties<FSEntry>(source_nodes[0]).relpath());
			return (*this)(env, py::cast(std::vector<std::string>{ implicit_target }), py::cast(source_nodes));
		}

		py::object emitter = emitter_;
		if(!emitter_.is_none()) {
			if(py::isinstance<py::dict>(emitter_) && !source_nodes.empty()) {
				emitter = py::dict(emitter_)[properties<FSEntry>(source_nodes[0]).suffix().c_str()];
			}
			if(py::isinstance<py::str>(emitter)) {
				emitter = subst(env, emitter);
			}
		}
		if(PyCallable_Check(emitter.ptr())) {
			py::tuple emitter_result { emitter(target_nodes, source_nodes, env) };
			NodeList emitted_targets = extract_file_nodes(env, emitter_result[0]);
			NodeList emitted_sources = extract_file_nodes(env, emitter_result[1]);
			target_nodes.swap(emitted_targets);
			source_nodes.swap(emitted_sources);
		}

		if(multi_ && graph[target_nodes[0]]->task()) {
			graph[target_nodes[0]]->task()->add_sources(source_nodes);
		} else {

			if(py::isinstance<py::dict>(actions_)) {
				if(!source_nodes.empty()) {
					actions_obj = py::dict(actions_)[properties<FSEntry>(source_nodes[0]).suffix().c_str()];
				}
			} else {
				actions_obj = actions_;
			}
			actions = make_actions(actions_obj);

			Task::add_task(env, target_nodes, source_nodes, actions, scanner_);
		}

		return target_nodes;
	}

	template<void (PythonBuilder::*make_node)(const std::string&, const Environment&, NodeList&) const>
	struct make_node_visitor : public boost::static_visitor<>
	{
		const Environment& env_;
		const PythonBuilder* builder_;
		NodeList& result_;
		make_node_visitor(
		    const Environment& env, const PythonBuilder* builder, NodeList& result
			) : env_(env), builder_(builder), result_(result) {}
		void operator()(const Node& node) const
		{
			result_.push_back(node);
		}
		void operator()(const std::string& name) const
		{
			(builder_->*make_node)(name, env_, result_);
		}
	};

	void make_target_node(const std::string& name, const Environment& env, NodeList& result) const
	{
		std::string full_name = adjust_target_name(env, name);
		if(!target_factory_.is_none())
			result.push_back(extract_node(target_factory_(full_name)));
		else
			result.push_back(add_entry(full_name));
	}

	void make_source_node(const std::string& name, const Environment& env, NodeList& result) const
	{
		if(!src_builder_.is_none()) {
			if(!source_ext_match(env, name)) {
				py::object sources;
				if(py::isinstance<py::str>(src_builder_)) {
					sources = py::cast(env).attr(src_builder_)(py::str(name.substr(0, name.rfind('.'))), py::str(name));
				} else {
					sources = src_builder_(env, py::str(name.substr(0, name.rfind('.'))), py::str(name));
				}
				for(auto node : sources)
					result.push_back(extract_node(py::reinterpret_borrow<py::object>(node)));
				return;
			}
		}
		std::string full_name = adjust_source_name(env, name);
		if(!source_factory_.is_none())
			result.push_back(extract_node(source_factory_(full_name)));
		else
			result.push_back(add_entry(full_name));
	}

	std::string adjust_target_name(const Environment& env, const std::string& name) const
	{
		std::string result,
			prefix = prefix_ ? extract_string_subst(env, prefix_) : std::string(),
			suffix = suffix_ ? extract_string_subst(env, suffix_) : std::string();
		result = adjust_affixes(env.subst(name), prefix, suffix, ensure_suffix_);
		return result;
	}

	std::string adjust_source_name(const Environment& env, const std::string& name) const
	{
		if(name.find('.') == std::string::npos) {
			std::string suffix = !src_suffix_.is_none() ? extract_string_subst(env, flatten(src_suffix_)[0]) : std::string();
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
	bool source_ext_match(const Environment& env, const std::string& name) const
	{
		std::set<std::string> suffixes;
		for(auto suffix : flatten(src_suffix_)) {
			suffixes.insert(extract_string_subst(env, py::reinterpret_borrow<py::object>(suffix)));
		}
		std::string suffix = boost::filesystem::extension(name);
		return suffixes.find(suffix) != suffixes.end();
	}
	std::string split_ext(const boost::filesystem::path& name) const
	{
		return (name.parent_path() / name.stem()).string();
	}

	void add_action(py::object, py::object);
	void add_emitter(py::object, py::object);

	py::object suffix() const { return suffix_; }
	py::object prefix() const { return prefix_; }
	py::object src_suffix() const { return src_suffix_; }
	py::object src_builder() const { return src_builder_; }

};

NodeList call_builder_interface(const PythonBuilder& builder, const Environment& env, py::object target, py::object source, py::kwargs kw)
{
	if(source.is_none()) {
		source = target;
		target = py::none();
	}
	return builder(env, target, source);
}

void PythonBuilder::add_action(py::object suffix, py::object action)
{
	actions_[suffix] = action;
	src_suffix_ = py::reinterpret_steal<py::object>(PySequence_Concat(flatten(src_suffix_).ptr(), flatten(py::str(suffix)).ptr()));
}

void PythonBuilder::add_emitter(py::object suffix, py::object emitter)
{
	emitter_[suffix] = emitter;
}

using namespace pybind11::literals;

void def_builder(py::module& m_builder)
{
	py::class_<PythonBuilder, PythonBuilder::pointer>(m_builder, "Builder")
		.def(py::init<
			 py::object, py::object, py::object, py::object, py::object, int, py::object, py::object, py::object, int, int, py::object, py::object, py::object, py::kwargs>(),
			 "action"_a = py::none(), "target_factory"_a = py::none(), "source_factory"_a = py::none(),
			 "prefix"_a = ""_s, "suffix"_a = ""_s, "ensure_suffix"_a = false, "src_suffix"_a = ""_s,
			 "emitter"_a = py::none(), "src_builder"_a = py::none(), "single_source"_a = false, "multi"_a = false,
			 "target_scanner"_a = py::none(), "source_scanner"_a = py::none(), "scanner"_a = py::none())
		.def("__call__", &call_builder_interface, "env"_a, "target"_a, "source"_a = py::none())
		.def("add_action", &PythonBuilder::add_action, "suffix"_a, "action"_a)
		.def("add_emitter", &PythonBuilder::add_emitter)
		.def("get_suffix", &PythonBuilder::suffix)
		.def("get_prefix", &PythonBuilder::prefix)
		.def("get_src_suffix", &PythonBuilder::src_suffix)
		.def_property_readonly("suffix", &PythonBuilder::suffix)
		;
}

}
}
