#include "test_common.hpp"

#include "python_interface/node_wrapper.hpp"

#include <boost/test/output_test_stream.hpp>
using boost::test_tools::output_test_stream;

BOOST_FIXTURE_TEST_SUITE(Builders, sconspp_fixture)
BOOST_AUTO_TEST_CASE(builders)
{
	SCONSPP_EXECFILE("src/python_interface/test/test_builders.SConstruct");
	Node n = python_interface::extract_node(ns["baz"][0]);
	output_test_stream output("src/python_interface/test/test_builders.reference.dot", true);
	write_build_graph(output, n);
	BOOST_CHECK(output.match_pattern());
}
BOOST_AUTO_TEST_SUITE_END()
