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

#ifndef DB_HPP
#define DB_HPP

#include <boost/optional.hpp>
#include <boost/utility.hpp>
#include <boost/array.hpp>

#include "dependency_graph.hpp"

class sqlite3;

namespace SQLite
{

class Db : public boost::noncopyable
{
	sqlite3* db;
	public:
	explicit Db(const std::string& filename);
	~Db();
	sqlite3* handle() const { return db; }
	void exec(const std::string& sql);
	template<class T>
	T exec(const std::string& sql);
};

}

namespace sconspp
{

class PersistentNodeData : public boost::noncopyable
{
	boost::optional<int> id_;
	boost::optional<int> prev_id_;
	int node_id_;
	int generation_;
	std::string type_;
	std::string name_;
	boost::optional<bool> existed_;
	boost::optional<time_t> timestamp_;
	boost::optional<boost::array<unsigned char, 16> > signature_;
	boost::optional<boost::array<unsigned char, 16> > task_signature_;
	boost::optional<int> task_status_;

	typedef std::pair<bool, std::string> IncludeDep;
	typedef std::set<IncludeDep> IncludeDeps;
	boost::optional<IncludeDeps> scanner_cache_;

	SQLite::Db& db;
	Node node;

	bool skip_write_;
	bool archive_record_ = false;

	public:
	PersistentNodeData(SQLite::Db& db, int id);
	PersistentNodeData(SQLite::Db& db, Node node);
	~PersistentNodeData();

	boost::optional<bool> existed() const { return existed_; }
	boost::optional<time_t> timestamp() const { return timestamp_; }
	boost::optional<boost::array<unsigned char, 16> > signature() const { return signature_; }
	boost::optional<bool>& existed() { return existed_; }
	boost::optional<time_t>& timestamp() { return timestamp_; }
	boost::optional<boost::array<unsigned char, 16> >& signature() { return signature_; }
	boost::optional<boost::array<unsigned char, 16> >& task_signature() { return task_signature_; }
	boost::optional<int> task_status() const { return task_status_; }
	boost::optional<int>& task_status() { return task_status_; }

	int id() const { return id_.get(); }
	int node_id() const { return node_id_; }
	int generation() const { return generation_; }

	std::set<int> dependencies();
	boost::optional<int> map_to_archive_dep(int id);

	IncludeDeps& scanner_cache() {
		if(!scanner_cache_) read_scanner_cache();
		return scanner_cache_.get();
	}
	void bump_generation();
	boost::optional<int> prev_id() const { return prev_id_; }
	bool is_archive() const { return archive_record_; }
	private:
	void read_scanner_cache();
	void write_scanner_cache();
};

class PersistentData : public boost::noncopyable
{
	SQLite::Db db_;
	typedef std::map<Node, boost::shared_ptr<PersistentNodeData> > Nodes;
	Nodes nodes_;
	typedef std::map<int, boost::shared_ptr<PersistentNodeData> > Archive;
	Archive archive_;
	bool do_clean_db_ = false;
	public:
	explicit PersistentData(const std::string& filename);
	~PersistentData();
	PersistentNodeData& record_current_data(Node);
	PersistentNodeData& get_archive_data(int);

	void schedule_clean_db() { do_clean_db_ = true; }
};

PersistentData& get_global_db(bool flush = false);

}

#endif
