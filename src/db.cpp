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

#include "db.hpp"
#include "node_properties.hpp"

#include <iostream>

#include <sqlite3.h>

#include <boost/lexical_cast.hpp>

namespace SQLite
{
	
template<typename T> struct sqlite3_column_impl;
template<typename T> struct sqlite3_column_impl<boost::optional<T> >
{
	static boost::optional<T> sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		if(sqlite3_column_type(stmt, i) == SQLITE_NULL)
			return boost::optional<T>();
		else
			return sqlite3_column_impl<T>::sqlite3_column(stmt, i);
	}
};
template<> struct sqlite3_column_impl<int>
{
	static int sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int(stmt, i);
	}
};
template<> struct sqlite3_column_impl<bool>
{
	static bool sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int(stmt, i);
	}
};
template<> struct sqlite3_column_impl<time_t>
{
	static time_t sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return sqlite3_column_int64(stmt, i);
	}
};
template<> struct sqlite3_column_impl<std::string>
{
	static std::string sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		return std::string((const char*)sqlite3_column_text(stmt, i));
	}
};
template<typename T, std::size_t n> struct sqlite3_column_impl<boost::array<T, n> >
{
	static boost::array<T, n> sqlite3_column(sqlite3_stmt* stmt, int i)
	{
		boost::array<T, n> result;
		memcpy(result.c_array(), sqlite3_column_blob(stmt, i), n);
		assert(sqlite3_column_bytes(stmt, i) == n);
		return result;
	}
};

template<typename T> inline T sqlite3_column(sqlite3_stmt* stmt, int i)
{
	return sqlite3_column_impl<T>::sqlite3_column(stmt, i);
}

inline void sqlite3_bind(sqlite3_stmt* stmt, int i, int val)
{
	sqlite3_bind_int(stmt, i, val);
}
inline void sqlite3_bind(sqlite3_stmt* stmt, int i, time_t val)
{
	sqlite3_bind_int64(stmt, i, val);
}
inline void sqlite3_bind(sqlite3_stmt* stmt, int i, const std::string& val)
{
	sqlite3_bind_text(stmt, i, val.c_str(), val.size(), SQLITE_TRANSIENT);
}
template<typename T, std::size_t n> void sqlite3_bind(sqlite3_stmt* stmt, int i, boost::array<T, n> val)
{
	sqlite3_bind_blob(stmt, i, val.data(), n, SQLITE_TRANSIENT);
}
template<typename T> inline void sqlite3_bind(sqlite3_stmt* stmt, int i, boost::optional<T> val)
{
	if(val)
		sqlite3_bind(stmt, i, val.get());
	else
		sqlite3_bind_null(stmt, i);
}

class Statement : public boost::noncopyable
{
	sqlite3_stmt* statement;
	sqlite3* db;
	public:
	Statement(sqlite3* db, const std::string& sql) : db(db)
	{
		int result = sqlite3_prepare_v2(db, sql.c_str(), sql.size() + 1, &statement, 0);
		if(result != SQLITE_OK) {
			throw std::runtime_error(std::string("sqlite error when preparing statement: \n" + sql + "\n: " +  sqlite3_errmsg(db)));
		}
	}
	~Statement()
	{
		sqlite3_finalize(statement);
	}


	int step(bool handle_constraint = true)
	{
		int result = sqlite3_step(statement);
		if(result == SQLITE_ROW || result == SQLITE_DONE || (!handle_constraint && result == SQLITE_CONSTRAINT))
			return result;
		throw std::runtime_error(std::string("sqlite error when evaluating statement: ") + sqlite3_errmsg(db));
	}
	void reset()
	{
		sqlite3_reset(statement);
	}
	int column_type(int i)
	{
		return sqlite3_column_type(statement, i);
	}
	template<typename T> T column(int i)
	{
		return sqlite3_column<T>(statement, i);
	}
	template<typename T> void bind(int i, T val)
	{
		sqlite3_bind(statement, i, val);
	}


	template<typename T> std::vector<T> exec()
	{
		std::vector<T> result;
		while(step() == SQLITE_ROW) {
			result.push_back(column<T>(0));
		}
		reset();
		return result;
	}
	template<typename T, typename T1> std::vector<T> exec(T1 val)
	{
		bind(1, val);
		return exec<T>();
	}
	template<typename T, typename T1, typename T2> std::vector<T> exec(T1 val1, T2 val2)
	{
		bind(1, val1);
		bind(2, val2);
		return exec<T>();
	}
};

Db::Db(const std::string& filename)
{
	int result = sqlite3_open(filename.c_str(), &db);
	if(result != SQLITE_OK) {
		throw std::runtime_error(std::string("sqlite error when opening ") + filename + ": " + sqlite3_errmsg(db));
	}
	exec("begin");
}
Db::~Db()
{
	exec("end");
	sqlite3_close(db);
}

void Db::exec(const std::string& sql)
{
	Statement stmt(db, sql);
	while(stmt.step() != SQLITE_DONE) {}
}

template<class T>
T Db::exec(const std::string& sql)
{
	Statement stmt(db, sql);
	int result = stmt.step();
	assert(result == SQLITE_ROW);
	return stmt.column<T>(0);
}

}

namespace sconspp
{

PersistentNodeData::PersistentNodeData(SQLite::Db& db, int id) : db(db), skip_write_(true), archive_record_(true)
{
	SQLite::Statement read_data(db.handle(),
		"select id, node_id, generation, type, name, existed, timestamp, signature, task_signature, task_status from nodes where id == ?1;");
	read_data.bind(1, id);
	int read_data_result = read_data.step();
	assert(read_data_result == SQLITE_ROW);

	id_ = read_data.column<boost::optional<int> >(0);
	node_id_ = read_data.column<int>(1);
	generation_ = read_data.column<int>(2);
	type_ = read_data.column<std::string>(3);
	name_ = read_data.column<std::string>(4);
	existed_ = read_data.column<boost::optional<bool> >(5);
	timestamp_ = read_data.column<boost::optional<int> >(6);
	signature_ = read_data.column<boost::optional<boost::array<unsigned char, 16> > >(7);
	task_signature_ = read_data.column<boost::optional<boost::array<unsigned char, 16> > >(8);
	task_status_ = read_data.column<boost::optional<int> >(9);
}

PersistentNodeData::PersistentNodeData(SQLite::Db& db, Node node)
	: type_(graph[node]->type()), name_(graph[node]->name()), db(db), node(node), skip_write_(false)
{
	SQLite::Statement prepare_record(db.handle(),
		"insert into nodes (generation,type,name,node_id) select 1, ?1, ?2, coalesce(max(node_id) + 1, 1) from nodes;");
	prepare_record.bind(1, type_);
	prepare_record.bind(2, name_);
	int prepare_result = prepare_record.step(false);
	if(prepare_result == SQLITE_CONSTRAINT)
		skip_write_ = true;
	else
		assert(prepare_result == SQLITE_DONE);

	SQLite::Statement get_generation(db.handle(),
		"select max(generation) from nodes where type == ?1 and name == ?2");
	get_generation.bind(1, type_);
	get_generation.bind(2, name_);
	int get_generation_result = get_generation.step();
	assert(get_generation_result == SQLITE_ROW);

	generation_ = get_generation.column<int>(0);

	SQLite::Statement read_data(db.handle(),
		"select id, node_id, existed, timestamp, signature, task_signature, task_status from nodes where generation == ?1 and type == ?2 and name == ?3;");
	read_data.bind(1, generation_);
	read_data.bind(2, type_);
	read_data.bind(3, name_);
	int read_data_result = read_data.step();
	assert(read_data_result == SQLITE_ROW  || read_data_result == SQLITE_DONE);

	id_ = read_data.column<boost::optional<int> >(0);
	assert(id_);
	node_id_ = read_data.column<int>(1);
	existed_ = read_data.column<boost::optional<bool> >(2);
	timestamp_ = read_data.column<boost::optional<int> >(3);
	signature_ = read_data.column<boost::optional<boost::array<unsigned char, 16> > >(4);
	task_signature_ = read_data.column<boost::optional<boost::array<unsigned char, 16> > >(5);
	task_status_ = read_data.column<boost::optional<int> >(6);
}

PersistentNodeData::~PersistentNodeData()
{
	if(skip_write_)
		return;
	try {
		graph[node]->record_persistent_data(*this);
		SQLite::Statement write_data(db.handle(), 
			"insert or replace into nodes (id, node_id, generation, type, name, existed, timestamp, signature, task_signature, task_status) values (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
		write_data.bind(1, id_);
		write_data.bind(2, node_id_);
		write_data.bind(3, generation_);
		write_data.bind(4, type_);
		write_data.bind(5, name_);
		write_data.bind(6, existed_);
		write_data.bind(7, timestamp_);
		write_data.bind(8, signature_);
		write_data.bind(9, task_signature_);
		write_data.bind(10, task_status_);
		while(write_data.step() != SQLITE_DONE) {}
		write_scanner_cache();

	} catch(const std::exception& e) {
		std::cout << "An exception occured when recording node " << type_ << "::" << name_ << ": " << e.what() << std::endl;
	}
}

std::set<int> PersistentNodeData::dependencies()
{
	std::set<int> result;
	SQLite::Statement get_dependencies(db.handle(),
		"select source_id from dependencies where target_id == ?1");
	get_dependencies.bind(1, node_id_);
	while(get_dependencies.step() != SQLITE_DONE)
		result.insert(get_dependencies.column<int>(0));
	return result;
}

boost::optional<int> PersistentNodeData::map_to_archive_dep(int id)
{
	SQLite::Statement map_dependency(db.handle(),
		"select source_id from dependencies where target_id == ?1"
		" intersect "
		"select id from nodes where node_id in (select node_id from nodes where id == ?2)");
	map_dependency.bind(1, node_id());
	map_dependency.bind(2, id);
	auto result = map_dependency.step();
	if(result == SQLITE_DONE)
		return {};
	if(result == SQLITE_ROW)
		return map_dependency.column<int>(0);
	assert(false);
}

void PersistentNodeData::bump_generation()
{
	if(!archive_record_ && skip_write_)
	{
		generation_++; skip_write_ = false;
		prev_id_ = id_;
		SQLite::Statement write_new_generation(db.handle(),
			"insert into nodes (node_id, generation, type, name) values (?1, ?2, ?3, ?4)");
		write_new_generation.bind(1, node_id_);
		write_new_generation.bind(2, generation_);
		write_new_generation.bind(3, type_);
		write_new_generation.bind(4, name_);
		while(write_new_generation.step() != SQLITE_DONE) {}

		id_ = boost::optional<int>(sqlite3_last_insert_rowid(db.handle()));
	}
}

void PersistentNodeData::read_scanner_cache()
{
	static SQLite::Statement get_includes(db.handle(),
		"select include, system from scanner_cache where node_id == ?1");
	get_includes.bind(1, node_id_);
	scanner_cache_ = IncludeDeps();
	IncludeDeps& deps = scanner_cache_.get();
	while(get_includes.step() != SQLITE_DONE)
		deps.insert(std::make_pair(get_includes.column<bool>(1), get_includes.column<std::string>(0)));
	get_includes.reset();
}

void PersistentNodeData::write_scanner_cache()
{
	if(scanner_cache_)
	{
		static SQLite::Statement clear_record(db.handle(),
			"delete from scanner_cache where node_id = ?1");
		clear_record.bind(1, node_id_);
		while(clear_record.step() != SQLITE_DONE) {}
		clear_record.reset();

		static SQLite::Statement write_record(db.handle(),
			"insert into scanner_cache values (?1, ?2, ?3)");
		for(const IncludeDep& dep : scanner_cache_.get()) {
			write_record.bind(1, node_id_);
			write_record.bind(2, dep.second);
			write_record.bind(3, dep.first);
			while(write_record.step() != SQLITE_DONE) {}
			write_record.reset();
		}
	}
}

PersistentData::PersistentData(const std::string& filename) : db_(filename)
{
	db_.exec("PRAGMA foreign_keys=ON");
	db_.exec("PRAGMA journal_mode=OFF");

	const int current_db_version = 5;
	int db_version = db_.exec<int>("PRAGMA user_version");
	if(db_version < current_db_version) {
		if(db_version > 0) {
			std::cout << "Signature database has older version. It will be reinitialized." << std::endl;
			db_.exec("drop table if exists scanner_cache");
			db_.exec("drop table if exists dependencies");
			db_.exec("drop table if exists nodes");
		}
		// Assume user_version == 0 means newly created db.

		db_.exec("PRAGMA user_version = " + boost::lexical_cast<std::string>(current_db_version));

		db_.exec("create table if not exists nodes "
			"(id INTEGER PRIMARY KEY, node_id INTEGER, generation INTEGER, type TEXT, name TEXT, existed INTEGER, timestamp INTEGER, signature BLOB, task_signature BLOB, task_status INTEGER)");
		db_.exec("create index if not exists node_identity_index on nodes (type, name)");
		db_.exec("create unique index if not exists node_archive_index on nodes (generation, type, name)");
		db_.exec("create index if not exists node_id_index on nodes (node_id)");
		db_.exec("create table if not exists dependencies "
			"(target_id INTEGER, source_id INTEGER, "
			"FOREIGN KEY(source_id) REFERENCES nodes(id))");
		db_.exec("create index if not exists source_dep_index on dependencies(source_id)");
		db_.exec("create index if not exists target_dep_index on dependencies(target_id)");
		db_.exec("create table if not exists scanner_cache "
			"(node_id INTEGER, include INTEGER, system BOOLEAN)");
		db_.exec("create index if not exists scanner_cache_index on scanner_cache(node_id)");
	}
}

PersistentData::~PersistentData()
{
	SQLite::Statement clear_dependency(db_.handle(),
		"delete from dependencies where target_id = ?1");
	SQLite::Statement write_dependency(db_.handle(), 
		"insert into dependencies values (?1, ?2)");
	for(Nodes::value_type& target_pair : nodes_) {
		int target_id = target_pair.second->node_id();
		Node target_node = target_pair.first;

		clear_dependency.bind(1, target_id);
		while(clear_dependency.step() != SQLITE_DONE) {}
		clear_dependency.reset();

		write_dependency.bind(1, target_pair.second->node_id());

		for(const Edge dependency : boost::make_iterator_range(out_edges(target_node, graph))) {
			write_dependency.bind(2, this->record_current_data(target(dependency, graph)).id());
			while(write_dependency.step() != SQLITE_DONE) {}
			write_dependency.reset();
		}
	}
	if(do_clean_db_) {
		SQLite::Statement clean_archive_nodes(db_.handle(),
			"delete from nodes where id in"
				" (select id from nodes where (id, generation) not in (select id, max(generation) from nodes group by node_id)"
				" intersect "
				"select id from nodes where id not in (select source_id from dependencies))");
		while(clean_archive_nodes.step() != SQLITE_DONE) {}
	}
}

PersistentNodeData& PersistentData::record_current_data(Node node)
{
	Nodes::iterator node_iter = nodes_.find(node);
	if(node_iter == nodes_.end())
		nodes_[node].reset(new PersistentNodeData(db_, node));
	return *(nodes_[node]);
}

PersistentNodeData& PersistentData::get_archive_data(int id)
{
	Archive::iterator archive_iter = archive_.find(id);
	if(archive_iter == archive_.end())
		archive_[id].reset(new PersistentNodeData(db_, id));
	return *(archive_[id]);
}

PersistentData& get_global_db(bool flush)
{
	static std::unique_ptr<PersistentData> data;
	if(!data || flush) {
		data.reset();
		data.reset(new PersistentData{"sconsppsign.sqlite"});
	}
	return *data.get();
}

}
