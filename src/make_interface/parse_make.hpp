#pragma once

namespace sconspp
{

namespace make_interface
{
	std::string make_subst(const Environment&, const std::string& input, bool);
	void run_makefile(const std::string&, int argc, char** argv);

}
}
