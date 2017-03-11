#include "test_common.hpp"

#include "python_interface/node_wrapper.hpp"

#include <boost/graph/graphviz.hpp>

#include <boost/test/output_test_stream.hpp>

namespace sconspp
{
namespace python_interface
{

using boost::test_tools::output_test_stream;

BOOST_FIXTURE_TEST_SUITE(Builders, sconspp_fixture)
BOOST_AUTO_TEST_CASE(builders)
{
	SCONSPP_EXECFILE("src/python_interface/test/test_builders.SConstruct");
	Node n = python_interface::extract_node(py::list(ns["baz"])[0]);
	//output_test_stream output("src/python_interface/test/test_builders.reference.dot", true);
	//write_build_graph(output, n);
	std::ifstream ifs { "src/python_interface/test/test_builders.reference.dot" };
	//BOOST_CHECK(output.match_pattern());
	BOOST_CHECK(check_isomorphism(ifs, n));
}
BOOST_AUTO_TEST_SUITE_END()

}
}
