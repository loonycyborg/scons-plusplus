#include <pybind11/pybind11.h>

#include "test_common.hpp"

#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/isomorphism.hpp>

#include "taskmaster.hpp"
#include "fs_node.hpp"

namespace sconspp
{
namespace python_interface
{

struct python_setup
{
	python_setup()
	{
		python_interface::init_python({}, 0, nullptr);
		set_fs_root(boost::filesystem::current_path());
	}
};


struct NodeInSet
{
	std::set<Node> nodes;
	NodeInSet() {}
	template<typename Iterator> NodeInSet(Iterator begin, Iterator end) 
	: nodes(begin, end)
	{
	}
	bool operator()(Node node) const
	{
		return nodes.count(node);
	}
};

bool all_edges(const boost::graph_traits<Graph>::edge_descriptor&) { return true; }

void write_build_graph(std::ostream& os, Node end_goal)
{
	std::vector<Node> nodes;
	build_order(end_goal, nodes);
	boost::write_graphviz(
		os,
		boost::make_filtered_graph(graph, all_edges, NodeInSet(nodes.begin(), nodes.end())),
		boost::default_writer(), boost::default_writer(), boost::default_writer(), IdMap(graph)
		);
}

bool check_isomorphism(std::istream& is, Node end_goal)
{
	Graph reference_graph;
	boost::dynamic_properties dp;
	std::map<Node, std::string> names;
	auto name_map { boost::make_assoc_property_map(names) } ;
	dp.property("node_id", name_map);
	read_graphviz(is, reference_graph, dp);
	std::map<Node, std::size_t> reference_indices;
	std::size_t i = 0;
	for(Node n : boost::make_iterator_range(vertices(reference_graph))) {
		reference_indices[n] = i;
		i++;
	}
	auto reference_map { boost::make_assoc_property_map(reference_indices) } ;

	std::vector<Node> nodes;
	build_order(end_goal, nodes);
	std::map<Node, std::size_t> goal_indices;
	i = 0;
	for(Node n : nodes) {
		goal_indices[n] = i;
		i++;
	}
	auto goal_map { boost::make_assoc_property_map(goal_indices) } ;

	return boost::graph::isomorphism(
		reference_graph,
		boost::make_filtered_graph(graph, all_edges, NodeInSet(nodes.begin(), nodes.end())),
		boost::graph::keywords::_vertex_index1_map = reference_map,
		boost::graph::keywords::_vertex_index2_map = goal_map);
}

BOOST_GLOBAL_FIXTURE(python_setup);

}
}
