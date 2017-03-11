#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>

#include <pybind11/eval.h>

#include "python_interface/python_interface.hpp"
#include "python_interface/python_interface_internal.hpp"

#define SCONSPP_EXEC(code) py::eval<py::eval_single_statement>(code, ns, ns)
#define SCONSPP_EXECFILE(name) py::eval_file(name, ns, ns)
#define SCONSPP_CHECK(cond) BOOST_CHECK_MESSAGE(py::eval(cond, ns, ns).cast<bool>(), cond)

namespace sconspp
{
namespace python_interface
{

struct python_exception_matches
{
	PyObject* type;
	python_exception_matches(PyObject* type) : type(type) {}
	bool operator()(py::error_already_set const& err)
	{
		bool result = PyErr_ExceptionMatches(type);
		return true;
	}
};
#define SCONSPP_CHECK_THROW(expr, exception) BOOST_CHECK_EXCEPTION(py::eval(expr, ns, ns), py::error_already_set, python_exception_matches(exception))

struct sconspp_fixture
{
	py::dict ns;
	sconspp_fixture()
	{
		ns = python_interface::copy(main_namespace);
	}
};

void write_build_graph(std::ostream& os, Node end_goal);
bool check_isomorphism(std::istream& is, Node end_goal);

}
}
