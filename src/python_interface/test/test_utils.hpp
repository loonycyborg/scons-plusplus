#define SCONSPP_EXEC(str) exec(str, main_namespace, main_namespace)
#define SCONSPP_CHECK(cond) BOOST_CHECK_MESSAGE((extract<bool>(eval(cond, main_namespace, main_namespace))), cond)

struct python_exception_matches
{
	PyObject* type;
	python_exception_matches(PyObject* type) : type(type) {}
	bool operator()(const error_already_set&)
	{
		bool result = PyErr_ExceptionMatches(type);
		PyErr_Clear();
		return result;
	}
};
#define SCONSPP_CHECK_THROW(expr, exeption) BOOST_CHECK_EXCEPTION(eval(expr, main_namespace, main_namespace), error_already_set, python_exception_matches(exeption))
