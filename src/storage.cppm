module;

#include "makros.h"
#include "third_party/sqlite3/sqlite3.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module storage;

import util;

using namespace util::strings;
using namespace util::misc;

namespace storage {

// timings for storage operations
// TODO: inline this, since its only used in PersistentStorageSqlite
struct StorageTimings {
	uint64_t init_ns = 0;
	uint64_t store_ns = 0;
	uint64_t read_ns = 0;
	uint64_t clear_ns = 0;
	uint64_t clean_up_ns = 0;
};

struct MultiMapTable {
	std::string ctx, name;
	sqlite3_stmt *insert = nullptr;
	sqlite3_stmt *get = nullptr;
	sqlite3_stmt *contains = nullptr;
};

struct TupleMapTable {
	std::string ctx, name;
	size_t arity = 0; // fixed width of table (1 key + n value columns)
	sqlite3_stmt *insert = nullptr;
	sqlite3_stmt *get = nullptr;
	sqlite3_stmt *contains = nullptr;
};

// creates descriptive table name for every (ctx, name) pair used in multimap/tuplemap
std::string make_table_name(std::string_view prefix, std::string_view ctx, std::string_view name) {
	if (!valid_ctx_name(ctx)) {
		throw std::runtime_error("Invalid context name for sqlite table: " + std::string(ctx));
	}
	auto [file_name, file_ext] = split_at(ctx, '.');
	for (char c : name) {
		if (!is_gtfs_file_char(c)) {
			throw std::runtime_error("Invalid multimap/tuplemap name for sqlite table: " +
			                         std::string(name));
		}
	}
	return std::string(prefix) + "_" + std::string(file_name) + "_" + std::string(name);
}

// _________________________________________________________________________________________________
export class PersistentStorageSqlite {
  private:
	// SQLite settings
	const sqlite3_int64 heap_bytes_;      // hard heap cap (process-wide)
	const double cache_frac_ = 0.7;       // Anteil von heap_mb für main.cache_size
	const double soft_frac_ = 0.9;        // soft heap = soft_frac * hard heap
	const uint32_t flush_ops_ = 100'000;  // commit after N write ops
	const uint32_t page_size_ = 32768;    // 32KB pages for larger cache efficiency
	const bool temp_store_file_ = true;   // predictable RAM for GROUP/ORDER
	const bool exclusive_lock_ = true;    // speed, single-process
	const bool journal_off_ = true;       // speed, temp DB (unsafe on crash)
	const std::string db_dir_ = "./.tmp"; // path to temporary DB file directory

  public:
	explicit PersistentStorageSqlite(double heap_mb)
	    : heap_bytes_(static_cast<sqlite3_int64>(heap_mb) * 1024LL * 1024LL) {
		// process-wide heap cap
		sqlite3_hard_heap_limit64(heap_bytes_);

		// make sqlite respect soft heap limit
		sqlite3_soft_heap_limit64(static_cast<sqlite3_int64>(heap_bytes_ * soft_frac_));

		// create temporary directory and file
		// ensure directory didn't exist before to avoid accidental user data overwrite
		if (std::filesystem::create_directory(db_dir_) == false) {
			std::cerr << "⚠️  Warning: Could not create temporary directory for sqlite database at "
			          << db_dir_ << " . Perhaps there is an artifact from a previous faulty run?"
			          << std::endl;
			bool dir_created = false;
			for (int i = 1; i <= 10; ++i) {
				if (std::filesystem::create_directory(db_dir_ + std::to_string(i))) {
					db_path_ = db_dir_ + std::to_string(i) + "/.runtime_storage.db";
					dir_created = true;
					break;
				}
			}
			if (!dir_created) {
				throw std::runtime_error(
				    "❌  Error: could not create temporary directory for sqlite database at " +
				    db_dir_ +
				    "1-10/.runtime_storage.db. There might be artifacts from previous faulty "
				    "runs.");
			}
		} else {
			db_path_ = db_dir_ + "/.runtime_storage.db";
		}

		// NOMUTEX is fine if you guarantee single-thread access to this connection
		const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
		if (sqlite3_open_v2(db_path_.string().c_str(), &db_, flags, nullptr) != SQLITE_OK) {
			throw std::runtime_error(std::string("sqlite open failed: ") + sqlite3_errmsg(db_));
		}
		sqlite3_extended_result_codes(db_, 1);

		// set pragmas
		// cache size: negative means KiB, default would be -2000 (~2MB) -> ideally high for
		// quick lookups
		const int64_t cache_kib = static_cast<int64_t>(heap_bytes_ / 1024.0 * cache_frac_);
		exec_(("PRAGMA main.cache_size = -" + std::to_string(cache_kib) + ";").c_str());

		exec_(("PRAGMA page_size = " + std::to_string(page_size_) + ";").c_str());

		if (temp_store_file_) {
			exec_("PRAGMA temp_store = 1;"); // force temp store in file, not RAM
		}

		if (exclusive_lock_)
			exec_("PRAGMA locking_mode = EXCLUSIVE;");
		if (journal_off_) {
			exec_("PRAGMA journal_mode = OFF;");
			exec_("PRAGMA synchronous = OFF;");
		} else {
			exec_("PRAGMA journal_mode = WAL;");
			exec_("PRAGMA synchronous = NORMAL;");
		}

		exec_("PRAGMA foreign_keys = OFF;");

#if GTFS2RDF_FULL_STATS
		const auto init_end = std::chrono::steady_clock::now();
		timings_.init_ns +=
		    std::chrono::duration_cast<std::chrono::nanoseconds>(init_end - init_start_).count();
#endif
	}

	~PersistentStorageSqlite() {
#if GTFS2RDF_FULL_STATS
		stats();
		{ // inner scope for timing
			SCOPED_TIMER_NS(timings_.clean_up_ns);
#endif
			try {
				flush();
			} catch (...) {
			}
			finalise_all();

			if (db_)
				sqlite3_close(db_);
			// delete temporary directory and file
			std::filesystem::remove_all(db_path_.parent_path());
#if GTFS2RDF_FULL_STATS
		} // closes inner scope for timing
		std::cout << "  Cleanup time (deleting tmp db file etc.) [s]: "
		          << timings_.clean_up_ns / 1e9 << "\n";
#endif
	}

	// --- VARIABLES ---
	void store_variable(std::string_view ctx, std::string_view name, std::string_view value) {
		SCOPED_TIMER_NS(timings_.store_ns);
		variables_[std::string(ctx)][std::string(name)] = std::string(value);
	}

	// TODO: perhaps refactor to string_view later
	const std::string &get_variable(std::string_view ctx, std::string_view name) {
		auto ctx_it = variables_.find(std::string(ctx));
		if (ctx_it == variables_.end()) {
			return empty_variable_;
		}
		auto name_it = ctx_it->second.find(std::string(name));
		if (name_it == ctx_it->second.end()) {
			return empty_variable_;
		}
		return name_it->second;
	}

	// ---- MULTIMAP ----
	void store_value(std::string_view ctx,
	                 std::string_view name,
	                 std::string_view key,
	                 std::string_view value) {
		SCOPED_TIMER_NS(timings_.store_ns);
		begin_if_needed();
		auto &T = ensure_mm_table_(ctx, name);
		bind_text(T.insert, 1, key);
		bind_text(T.insert, 2, value);
		step_done(T.insert);
		if (++pending_ >= flush_ops_)
			flush();
	}

	void store_value(std::string_view ctx,
	                 std::string_view name,
	                 std::initializer_list<std::string_view> key_parts,
	                 std::string_view value) {
		store_value(ctx, name, concat_key_parts_(key_parts), value);
	}

	void store_value(std::string_view ctx,
	                 std::string_view name,
	                 std::span<const std::string_view> key_parts,
	                 std::string_view value) {
		store_value(ctx, name, concat_key_parts_(key_parts), value);
	}

	bool contains_value(std::string_view ctx,
	                    std::string_view name,
	                    std::string_view key,
	                    std::string_view val) {
		SCOPED_TIMER_NS(timings_.read_ns);
		const std::string tbl = make_table_name("mm", ctx, name);
		auto it = mm_tables_.find(tbl);
		if (it == mm_tables_.end())
			return false; // table doesn't exist yet
		auto &T = it->second;

		bind_text(T.contains, 1, key);
		bind_text(T.contains, 2, val);
		const int rc = sqlite3_step(T.contains);
		reset_stmt(T.contains);
		return rc == SQLITE_ROW;
	}

	bool contains_value(std::string_view ctx,
	                    std::string_view name,
	                    std::initializer_list<std::string_view> key_parts,
	                    std::string_view val) {
		return contains_value(ctx, name, concat_key_parts_(key_parts), val);
	}

	bool contains_value(std::string_view ctx,
	                    std::string_view name,
	                    std::span<const std::string_view> key_parts,
	                    std::string_view val) {
		return contains_value(ctx, name, concat_key_parts_(key_parts), val);
	}

	std::vector<std::string>
	get_values(std::string_view ctx, std::string_view name, std::string_view key) {
		SCOPED_TIMER_NS(timings_.read_ns);
		std::vector<std::string> out;
		const std::string tbl = make_table_name("mm", ctx, name);
		auto it = mm_tables_.find(tbl);
		if (it == mm_tables_.end())
			return out; // table doesn't exist yet
		auto &T = it->second;

		bind_text(T.get, 1, key);
		while (sqlite3_step(T.get) == SQLITE_ROW) {
			const char *txt = reinterpret_cast<const char *>(sqlite3_column_text(T.get, 0));
			int bytes = sqlite3_column_bytes(T.get, 0);
			if (txt)
				out.emplace_back(txt, bytes);
		}
		reset_stmt(T.get);
		return out;
	}

	std::vector<std::string> get_values(std::string_view ctx,
	                                    std::string_view name,
	                                    std::initializer_list<std::string_view> key_parts) {
		return get_values(ctx, name, concat_key_parts_(key_parts));
	}

	std::vector<std::string> get_values(std::string_view ctx,
	                                    std::string_view name,
	                                    std::span<const std::string_view> key_parts) {
		return get_values(ctx, name, concat_key_parts_(key_parts));
	}

	// _____________________________________________________________________________________________
	// ---- tuplemap ----
	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::string_view key,
	                 std::span<const std::string_view> tuple) {
		SCOPED_TIMER_NS(timings_.store_ns);
		begin_if_needed();
		auto &T = ensure_tm_table_(ctx, name, tuple.size());
		bind_text(T.insert, 1, key);
		for (size_t i = 0; i < tuple.size(); ++i) {
			bind_text(T.insert, 2 + i, tuple[i]);
		}
		step_done(T.insert);
		if (++pending_ >= flush_ops_)
			flush();
	}

	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::initializer_list<std::string_view> key_parts,
	                 std::span<const std::string_view> tuple) {
		store_tuple(ctx, name, concat_key_parts_(key_parts), tuple);
	}

	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::string_view key,
	                 std::initializer_list<std::string_view> tuple_parts) {
		store_tuple(ctx,
		            name,
		            key,
		            std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::initializer_list<std::string_view> key_parts,
	                 std::initializer_list<std::string_view> tuple_parts) {
		store_tuple(ctx,
		            name,
		            concat_key_parts_(key_parts),
		            std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::span<const std::string_view> key,
	                 std::span<const std::string_view> tuple) {
		store_tuple(ctx, name, concat_key_parts_(key), tuple);
	}

	void store_tuple(std::string_view ctx,
	                 std::string_view name,
	                 std::span<const std::string_view> key,
	                 std::initializer_list<std::string_view> tuple_parts) {
		store_tuple(ctx,
		            name,
		            concat_key_parts_(key),
		            std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	std::vector<std::vector<std::string>>
	get_tuples(std::string_view ctx, std::string_view name, std::string_view key) {
		SCOPED_TIMER_NS(timings_.read_ns);
		std::vector<std::vector<std::string>> out;

		const std::string tbl = make_table_name("tm", ctx, name);
		auto it = tm_tables_.find(tbl);
		if (it == tm_tables_.end())
			return out; // table doesn't exist yet
		auto &T = it->second;
		bind_text(T.get, 1, key);
		while (sqlite3_step(T.get) == SQLITE_ROW) {
			std::vector<std::string> tuple;
			tuple.reserve(T.arity);
			for (size_t i = 0; i < T.arity; ++i) {
				const char *txt = reinterpret_cast<const char *>(sqlite3_column_text(T.get, i));
				int bytes = sqlite3_column_bytes(T.get, i);
				if (txt)
					tuple.emplace_back(std::string(txt, bytes));
				else
					tuple.emplace_back("");
			}
			out.emplace_back(std::move(tuple));
		}
		reset_stmt(T.get);
		return out;
	}

	std::vector<std::vector<std::string>>
	get_tuples(std::string_view ctx,
	           std::string_view name,
	           std::initializer_list<std::string_view> key_parts) {
		return get_tuples(ctx, name, concat_key_parts_(key_parts));
	}

	std::vector<std::vector<std::string>>
	get_tuples(std::string_view ctx, std::string_view name, std::span<const std::string_view> key) {
		return get_tuples(ctx, name, concat_key_parts_(key));
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::string_view key,
	                    std::span<const std::string_view> tuple) {
		SCOPED_TIMER_NS(timings_.read_ns);
		const std::string tbl = make_table_name("tm", ctx, name);
		auto it = tm_tables_.find(tbl);
		if (it == tm_tables_.end())
			return false; // table doesn't exist yet
		auto &T = it->second;
		if (T.arity != tuple.size()) {
			throw std::runtime_error(
			    "❌  SqliteBackingStore::contains_tuple: arity mismatch for table " + tbl +
			    ": expected " + std::to_string(T.arity) + ", got " + std::to_string(tuple.size()));
		}
		bind_text(T.contains, 1, key);
		for (size_t i = 0; i < T.arity; ++i) {
			bind_text(T.contains, 2 + i, tuple[i]);
		}
		const int rc = sqlite3_step(T.contains);
		reset_stmt(T.contains);
		return rc == SQLITE_ROW;
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::initializer_list<std::string_view> key_parts,
	                    std::span<const std::string_view> tuple) {
		return contains_tuple(ctx, name, concat_key_parts_(key_parts), tuple);
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::string_view key,
	                    std::initializer_list<std::string_view> tuple_parts) {
		return contains_tuple(
		    ctx,
		    name,
		    key,
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::initializer_list<std::string_view> key_parts,
	                    std::initializer_list<std::string_view> tuple_parts) {
		return contains_tuple(
		    ctx,
		    name,
		    concat_key_parts_(key_parts),
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::span<const std::string_view> key,
	                    std::span<const std::string_view> tuple) {
		return contains_tuple(ctx, name, concat_key_parts_(key), tuple);
	}

	bool contains_tuple(std::string_view ctx,
	                    std::string_view name,
	                    std::span<const std::string_view> key,
	                    std::initializer_list<std::string_view> tuple_parts) {
		return contains_tuple(
		    ctx,
		    name,
		    concat_key_parts_(key),
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void clear_context(std::string_view ctx) {
		SCOPED_TIMER_NS(timings_.clear_ns);
		variables_.erase(std::string(ctx));
		flush();

		// Drop multimap tables for ctx
		if (auto it = ctx_mm_tables_.find(ctx); it != ctx_mm_tables_.end()) {
			for (const auto &tbl : it->second)
				drop_table_and_finalise_mm_(tbl);
			ctx_mm_tables_.erase(it);
		}

		// Drop tuplemap tables for ctx
		if (auto it = ctx_tm_tables_.find(ctx); it != ctx_tm_tables_.end()) {
			for (const auto &tbl : it->second)
				drop_table_and_finalise_tm_(tbl);
			ctx_tm_tables_.erase(it);
		}
	}

	void flush() {
		if (in_tx_) {
			exec_("COMMIT;");
			in_tx_ = false;
			pending_ = 0;
		}
	}

	double get_heap_limit_mb() const {
		return static_cast<double>(heap_bytes_) / (1024.0 * 1024.0);
	}

	void stats() {
		flush();

		std::cout << "\n--------------------------------------------------------------------\n";
		std::cout << "🗄️  PERSISTENT STORAGE SUMMARY\n\n";

		// In-memory
		std::cout << "[In-memory] Variables\n";
		util::operator<<(std::cout, variables_);
		std::cout << "\n\n";

		auto count_table = [&](std::string_view tbl) -> sqlite3_int64 {
			sqlite3_stmt *st = nullptr;
			const std::string sql = "SELECT COUNT(*) FROM " + std::string(tbl) + ";";
			prep_(st, sql.c_str());
			sqlite3_int64 cnt = 0;
			if (sqlite3_step(st) == SQLITE_ROW)
				cnt = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
			return cnt;
		};

		// SQLite multimaps
		std::cout << "[SQLite] Multimap counts (ctx, name)\n\n";
		for (const auto &[tbl, T] : mm_tables_) {
			std::cout << "• [" << T.ctx << "] " << T.name << "  rows=" << count_table(tbl) << "\n";
		}
		std::cout << "\n";

		// SQLite tuplemaps
		std::cout << "[SQLite] Tuplemap counts (ctx, name)\n\n";
		for (const auto &[tbl, T] : tm_tables_) {
			std::cout << "• [" << T.ctx << "] " << T.name << "  rows=" << count_table(tbl) << "\n";
		}
		std::cout << "\n";

		// DB size
		std::error_code ec;
		const auto db_size = std::filesystem::file_size(db_path_, ec);
		if (!ec) {
			std::cout << "Summary\n"
			          << "  approx DB size: " << db_size << " bytes\n\n";
		}

#if GTFS2RDF_FULL_STATS
		// Timing block
		std::cout << "Timing statistics\n"
		          << "  initialization: " << std::fixed << std::setprecision(2)
		          << timings_.init_ns / 1e9 << " s\n"
		          << "  store:          " << std::fixed << std::setprecision(2)
		          << timings_.store_ns / 1e9 << " s\n"
		          << "  read:           " << std::fixed << std::setprecision(2)
		          << timings_.read_ns / 1e9 << " s\n"
		          << "  clear:          " << std::fixed << std::setprecision(2)
		          << timings_.clear_ns / 1e9 << " s\n\n";
#endif

		std::cout << "--------------------------------------------------------------------\n";
	}

  private:
	sqlite3 *db_ = nullptr;

	std::filesystem::path db_path_;

#if GTFS2RDF_FULL_STATS
	const std::chrono::steady_clock::time_point init_start_ = std::chrono::steady_clock::now();
	mutable StorageTimings timings_;
#endif

	// variables in-memory
	std::unordered_map<std::string,
	                   std::unordered_map<std::string, std::string, string_hash, std::equal_to<>>,
	                   string_hash,
	                   std::equal_to<>>
	    variables_;

	// maps (ctx,name) to its table struct with prepared statements (and arity)
	mutable std::unordered_map<std::string, MultiMapTable, string_hash, std::equal_to<>> mm_tables_;
	mutable std::unordered_map<std::string, TupleMapTable, string_hash, std::equal_to<>> tm_tables_;

	// keep track which tables belong to which ctx -> enables fast clear_context(ctx)
	std::unordered_map<std::string, std::vector<std::string>, string_hash, std::equal_to<>>
	    ctx_mm_tables_;
	std::unordered_map<std::string, std::vector<std::string>, string_hash, std::equal_to<>>
	    ctx_tm_tables_;

	mutable std::string key_buf_; // reusable buffer for multi-value keys

	bool in_tx_ = false;
	uint32_t pending_ = 0;

	// static const emptiness objects
	static inline const std::string empty_variable_ = "";
	static inline const std::vector<std::string> empty_multimap_ = {};
	static inline const std::vector<std::vector<std::string>> empty_tuplemap_ = {};

	// _____________________________________________________________________________________________
	// PRIVATE METHODS

	// concatenate key parts with null byte separators into key_buf_
	std::string_view concat_key_parts_(std::span<const std::string_view> parts) {
		key_buf_.clear();
		size_t total = 0;
		for (auto p : parts)
			total += p.size();
		if (total == 0) {
			throw std::runtime_error("❌  SqliteBackingStore::concat_key_parts: empty key parts");
		}
		key_buf_.reserve(total + parts.size() - 1); // size + null separators
		bool first = true;
		for (auto p : parts) {
			if (!first)
				key_buf_.push_back('\0');
			first = false;
			key_buf_.append(p.data(), p.size());
		}
		return key_buf_;
	}

	std::string_view concat_key_parts_(std::initializer_list<std::string_view> parts) {
		return concat_key_parts_(std::span<const std::string_view>(parts.begin(), parts.size()));
	}

	// sqlite wrappers
	void exec_(const char *sql) {
		char *err = nullptr;
		if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
			std::string msg = err ? err : sqlite3_errmsg(db_);
			sqlite3_free(err);
			throw std::runtime_error("sqlite exec_ failed: " + msg);
		}
	}

	void prep_(sqlite3_stmt *&st, const char *sql) {
		if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK) {
			throw std::runtime_error(std::string("sqlite prepare failed: ") + sqlite3_errmsg(db_));
		}
	}

	void begin_if_needed() {
		if (!in_tx_) {
			exec_("BEGIN;");
			in_tx_ = true;
		}
	}

	static void bind_text(sqlite3_stmt *st, int idx, const std::string &s) {
		sqlite3_bind_text(st, idx, s.c_str(), -1, SQLITE_TRANSIENT);
	}
	static void bind_text(sqlite3_stmt *st, int idx, std::string_view sv) {
		sqlite3_bind_text(st, idx, sv.data(), (int)sv.size(), SQLITE_TRANSIENT);
	}

	static void reset_stmt(sqlite3_stmt *st) {
		sqlite3_reset(st);
		sqlite3_clear_bindings(st);
	}

	static void step_done(sqlite3_stmt *st) {
		const int rc = sqlite3_step(st);
		if (rc != SQLITE_DONE) {
			sqlite3 *db = sqlite3_db_handle(st);
			std::string msg = db ? sqlite3_errmsg(db) : "sqlite step failed";
			reset_stmt(st);
			throw std::runtime_error("sqlite step failed: " + msg);
		}
		reset_stmt(st);
	}

	void finalise_all() {
		for (auto &[tbl, T] : mm_tables_) {
			if (T.insert)
				sqlite3_finalize(T.insert);
			if (T.get)
				sqlite3_finalize(T.get);
			if (T.contains)
				sqlite3_finalize(T.contains);
		}
		mm_tables_.clear();

		for (auto &[tbl, T] : tm_tables_) {
			if (T.insert)
				sqlite3_finalize(T.insert);
			if (T.get)
				sqlite3_finalize(T.get);
			if (T.contains)
				sqlite3_finalize(T.contains);
		}
		tm_tables_.clear();

		ctx_mm_tables_.clear();
		ctx_tm_tables_.clear();
	}

	void drop_table_and_finalise_mm_(std::string_view tbl) {
		auto it = mm_tables_.find(tbl);
		if (it != mm_tables_.end()) {
			if (it->second.insert)
				sqlite3_finalize(it->second.insert);
			if (it->second.get)
				sqlite3_finalize(it->second.get);
			if (it->second.contains)
				sqlite3_finalize(it->second.contains);
			mm_tables_.erase(it);
		}
		exec_(("DROP TABLE IF EXISTS " + std::string(tbl) + ";").c_str());
	}

	void drop_table_and_finalise_tm_(std::string_view tbl) {
		auto it = tm_tables_.find(tbl);
		if (it != tm_tables_.end()) {
			if (it->second.insert)
				sqlite3_finalize(it->second.insert);
			if (it->second.get)
				sqlite3_finalize(it->second.get);
			if (it->second.contains)
				sqlite3_finalize(it->second.contains);
			tm_tables_.erase(it);
		}
		exec_(("DROP TABLE IF EXISTS " + std::string(tbl) + ";").c_str());
	}

	// Create + prepare statements lazily (on first access).
	MultiMapTable &ensure_mm_table_(std::string_view ctx, std::string_view name) {
		const std::string tbl = make_table_name("mm", ctx, name);
		auto &T = mm_tables_[tbl];

		if (!T.insert) {
			// minimal schema; PRIMARY KEY(key,val) keeps values sorted for a given key.
			const std::string create_sql =
			    "CREATE TABLE IF NOT EXISTS " + tbl +
			    " (key TEXT NOT NULL, val TEXT NOT NULL, PRIMARY KEY(key,val)) WITHOUT ROWID;";
			exec_(create_sql.c_str());

			// prepare per-table statements
			T.ctx = ctx;
			T.name = name;
			prep_(T.insert, ("INSERT OR IGNORE INTO " + tbl + "(key,val) VALUES(?,?);").c_str());
			prep_(T.get, ("SELECT val FROM " + tbl + " WHERE key=? ORDER BY val;").c_str());
			prep_(T.contains, ("SELECT 1 FROM " + tbl + " WHERE key=? AND val=? LIMIT 1;").c_str());

			// Track for fast clear_context
			auto &v = get_or_insert(ctx_mm_tables_, ctx);
			if (std::find(v.begin(), v.end(), tbl) == v.end())
				v.push_back(tbl);
		}
		return T;
	}

	TupleMapTable &
	ensure_tm_table_(std::string_view ctx, std::string_view name, size_t tuple_arity) {
		if (tuple_arity == 0) {
			throw std::runtime_error("❌  SqliteBackingStore::ensure_tm_table: zero arity");
		}
		const std::string tbl = make_table_name("tm", ctx, name);
		auto &T = tm_tables_[tbl];

		if (!T.insert) {
			T.ctx = ctx;
			T.name = name;
			T.arity = tuple_arity;

			// create table with dynamic number of value columns
			std::string create_sql = "CREATE TABLE IF NOT EXISTS " + tbl + " (key TEXT NOT NULL";
			for (size_t i = 0; i < tuple_arity; ++i) {
				create_sql += ", val" + std::to_string(i) + " TEXT NOT NULL";
			}
			create_sql += ", PRIMARY KEY(key";
			for (size_t i = 0; i < tuple_arity; ++i) {
				create_sql += ", val" + std::to_string(i);
			}
			create_sql += ")) WITHOUT ROWID;";
			exec_(create_sql.c_str());

			// prepare per-table statements
			// INSERT statement for storing
			std::string insert_sql = "INSERT OR IGNORE INTO " + tbl + " (key";
			for (size_t i = 0; i < tuple_arity; ++i) {
				insert_sql += ", val" + std::to_string(i);
			}
			insert_sql += ") VALUES (?";
			for (size_t i = 0; i < tuple_arity; ++i) {
				insert_sql += ", ?";
			}
			insert_sql += ");";
			prep_(T.insert, insert_sql.c_str());

			// SELECT statement for getting
			std::string get_sql = "SELECT ";
			for (size_t i = 0; i < tuple_arity; ++i) {
				if (i > 0)
					get_sql += ", ";
				get_sql += "val" + std::to_string(i);
			}
			get_sql += " FROM " + tbl + " WHERE key=? ORDER BY "; // ensures ordered output
			for (size_t i = 0; i < tuple_arity; ++i) {
				if (i > 0)
					get_sql += ", ";
				get_sql += "val" + std::to_string(i);
			}
			get_sql += ";";
			prep_(T.get, get_sql.c_str());

			// SELECT statement for contains
			std::string contains_sql = "SELECT 1 FROM " + tbl + " WHERE key=?";
			for (size_t i = 0; i < tuple_arity; ++i) {
				contains_sql += " AND val" + std::to_string(i) + "=?";
			}
			contains_sql += " LIMIT 1;";
			prep_(T.contains, contains_sql.c_str());

			// tracking ctx tables for fast clear_context
			auto &v = get_or_insert(ctx_tm_tables_, ctx);
			if (std::find(v.begin(), v.end(), tbl) == v.end())
				v.push_back(tbl);
		} else {
			// ensure arity matches on subsequent accesses
			if (T.arity != tuple_arity) {
				throw std::runtime_error(
				    "❌  SqliteBackingStore::ensure_tm_table: arity mismatch for table " + tbl +
				    ": expected " + std::to_string(T.arity) + ", got " +
				    std::to_string(tuple_arity));
			}
		}
		return T;
	}
};

} // namespace storage