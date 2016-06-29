#include "test_common.hpp"

#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/graphviz.hpp>

#include "taskmaster.hpp"
#include "fs_node.hpp"

namespace sconspp
{
namespace python_interface
{

static void translate_error_already_set(const error_already_set&)
{
	python_interface::throw_python_exc("A python exception was thrown");
}

struct python_setup
{
	python_setup()
	{
		python_interface::init_python();
		boost::unit_test::unit_test_monitor.register_exception_translator<error_already_set>(&translate_error_already_set);
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

BOOST_GLOBAL_FIXTURE(python_setup);

}
}
