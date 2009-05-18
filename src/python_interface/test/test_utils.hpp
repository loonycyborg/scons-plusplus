#define SCONSPP_EXEC(str) exec(str, main_namespace, main_namespace)
#define SCONSPP_CHECK(cond) BOOST_CHECK(extract<bool>(eval(cond, main_namespace, main_namespace)))
