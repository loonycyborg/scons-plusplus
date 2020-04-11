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

#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <boost/version.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>

#include "log.hpp"
#include "util.hpp"

using std::string;

namespace sconspp
{

[[ noreturn ]] void restart_exception::do_restart(char** argv) const
{
	execv(argv[0], argv);
	std::terminate();
}

boost::scoped_ptr<Environ> Environ::environ_;

Environ::Environ()
{
	for(int i = 0; environ[i] != nullptr; i++) {
		string var(environ[i]);
		size_t pos = var.find('=');
		if(pos == string::npos)
			vars_.insert(std::make_pair(var, string()));
		else
			vars_.insert(std::make_pair(var.substr(0, pos), var.substr(pos+1, string::npos)));
	}
}

boost::filesystem::path where_is(const std::string& name)
{
	if(boost::filesystem::path(name).is_complete())
		return name;
	std::vector<string> env_path;
	string path_str = Environ::instance().get("PATH");
	boost::algorithm::split(env_path, path_str, boost::is_any_of(":"));
	for(const string& path_item : env_path) {
		boost::filesystem::path path = boost::filesystem::path(path_item) / name;
		if(boost::filesystem::exists(path))
			return path;
	}
	return string();
}

boost::filesystem::path readlink(const boost::filesystem::path& path)
{
	char buf[5000];
	size_t n = ::readlink(path.string().c_str(), buf, 5000);
	if(n == static_cast<size_t>(-1))
		throw boost::system::error_code(errno, boost::system::system_category());
	std::string name(buf, n);
	return name;
}

inline int throw_if_error(int return_value)
{
	if(return_value == -1)
#if BOOST_VERSION < 104400
		throw boost::system::system_error(errno, boost::system::system_category);
#else
		throw boost::system::system_error(errno, boost::system::system_category());
#endif
	return return_value;
}

scoped_chdir::scoped_chdir(const boost::filesystem::path& dir)
{
	old_current_dir = boost::filesystem::current_path();
	boost::filesystem::current_path(dir);
}
scoped_chdir::~scoped_chdir()
{
	boost::filesystem::current_path(old_current_dir);
}

std::pair<int, std::vector<string>> exec(const std::vector<string>& args, bool capture_output)
{
	std::vector<char*> argv;
	for(const string& arg : args)
		argv.push_back(const_cast<char*>(arg.c_str()));
	argv.push_back(nullptr);

	boost::filesystem::path binary = where_is(args[0]);
	if(binary.empty())
		throw std::runtime_error("util::exec : Failed to find executable: " + args[0]);
	throw_if_error(access(binary.string().c_str(), X_OK));

	int stdout_fds[2] = { 0, 0 }, stderr_fds[2] = { 0, 0 };
	if(capture_output) {
		throw_if_error(pipe(stdout_fds)); throw_if_error(pipe(stderr_fds));
	}

	pid_t child = throw_if_error(fork());
	if(child == 0) {
		if(capture_output) {
			close(stdout_fds[0]); close(stderr_fds[0]);

			dup2(stdout_fds[1], 1);
			dup2(stderr_fds[1], 2);

			close(stdout_fds[1]); close(stderr_fds[1]);
		}

		execvp(binary.string().c_str(), &argv[0]);
		std::terminate();
	} else {
		int status;
		std::vector<string> output;
		if(capture_output) {
			close(stdout_fds[1]); close(stderr_fds[1]);
			output.resize(2);

			pollfd pollfds[2] { { stdout_fds[0], POLLIN, 0 }, { stderr_fds[0], POLLIN, 0 } };
			bool eof_reached[2] { false, false };
			while(!eof_reached[0] || !eof_reached[1]) {
				int num_fds = throw_if_error(poll(pollfds, 2, -1));
				assert(num_fds > 0);
				assert(num_fds <= 2);

				char buf[1024];
				for(int index : { 0, 1 }) {
					if((pollfds[index].revents & POLLIN) || (pollfds[index].revents & POLLHUP)) {
						int n_bytes = throw_if_error(read(pollfds[index].fd, buf, 1024));
						if(n_bytes == 0)
							eof_reached[index] = true;
						else
							output[index] += string(buf, n_bytes);
					}
				}
			}
		}

		throw_if_error(waitpid(child, &status, 0));

		if(WIFEXITED(status)) {
			if(WEXITSTATUS(status) != 0) {
				logging::error(logging::System) << args[0] << " exited with status " << boost::lexical_cast<std::string>(WEXITSTATUS(status)) << "\n";
			}
		} else {
			if(WIFSIGNALED(status))
				logging::error(logging::System) << args[0] << " terminated by signal: " << strsignal(WTERMSIG(status)) << "\n";
			else
				logging::error(logging::System) << args[0] << " exited abnormally\n";
		}

		return { status, output };
	}
}

boost::array<unsigned char, 16> MD5::hash_file(const std::string& filename)
{
	MD5 md5;
	FILE* file = fopen(filename.c_str(), "r");
	BOOST_SCOPE_EXIT( (&file) ) {
		fclose(file);
	} BOOST_SCOPE_EXIT_END

	if(file == nullptr) throw boost::system::system_error(errno, boost::system::system_category(), "util::MD5::hash_file: Failed to open " + filename);
	while(!feof(file)) {
		const int blocksize = 4096;
		unsigned char buffer[blocksize];

		size_t count = fread(buffer, 1, blocksize, file);
		if(ferror(file)) throw boost::system::system_error(errno, boost::system::system_category(), "util::MD5::hash_file: Failed to read " + filename);

		md5.append(buffer, count);
	}
	return md5.finish();
}

}
