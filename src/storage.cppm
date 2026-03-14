// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "macros.h"
#include "third_party/sqlite3/sqlite3.h"
#include "util/diagnostics.h"

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

struct MultiMapTable {
	std::string ctx, name;
	sqlite3_stmt* insert = nullptr;
	sqlite3_stmt* get = nullptr;
	sqlite3_stmt* contains = nullptr;
};

struct TupleMapTable {
	std::string ctx, name;
	size_t arity = 0; // fixed width of table (n value columns)
	sqlite3_stmt* insert = nullptr;
	sqlite3_stmt* get = nullptr;
	sqlite3_stmt* contains = nullptr;
};

// _________________________________________________________________________________________________
export class PersistentStorageSqlite {
  private:
	// SQLite settings
	const sqlite3_int64 HEAP_BYTES_;                   // hard heap cap (process-wide)
	static constexpr double CACHE_FRAC_ = 0.7;         // fraction of heap to devote to cache
	static constexpr double SOFT_FRAC_ = 0.9;          // soft heap = soft_frac * hard heap
	static constexpr uint32_t FLUSH_OPS_ = 100'000;    // commit after N write ops
	static constexpr uint32_t PAGE_SIZE_ = 32768;      // 32KB pages for better cache efficiency
	static constexpr uint32_t BUSY_TIMEOUT_MS_ = 1000; // wait up to 1s if DB is locked (robustness)
	static constexpr bool TEMP_STORE_FILE_ = true;     // predictable RAM
	static constexpr bool EXCLUSIVE_LOCK_ = true;      // speed, single-process
	static constexpr bool JOURNAL_OFF_ = true;         // speed, temp DB (unsafe on crash)
	const std::string TEMP_DIR_NAME_ = ".gtfs2rdf_storage"; // subdir for temp DB files

  public:
	explicit PersistentStorageSqlite(diagnostics::WarningCollector& wcol,
	                                 diagnostics::VerbosityLevelStats verbosity_level_stats,
	                                 double heap_mb,
	                                 const std::string& tmp_dir)
	    : HEAP_BYTES_(static_cast<sqlite3_int64>(heap_mb) * 1024LL * 1024LL)
	    , wcol_(wcol)
	    , verbosity_level_stats_(verbosity_level_stats) {
		// process-wide heap cap
		sqlite3_hard_heap_limit64(HEAP_BYTES_);

		// make sqlite respect soft heap limit
		sqlite3_soft_heap_limit64(static_cast<sqlite3_int64>(HEAP_BYTES_ * SOFT_FRAC_));

		std::string db_dir = tmp_dir + "/" + TEMP_DIR_NAME_;

		auto date_time_stamp =
		    std::format("{:%Y%m%d_%H%M%S}",
		                std::chrono::zoned_time{std::chrono::current_zone(),
		                                        std::chrono::floor<std::chrono::seconds>(
		                                            std::chrono::system_clock::now())});

		// create temporary directory and file
		// ensure directory didn't exist before to avoid accidental user data overwrite
		if (!std::filesystem::create_directory(db_dir)) {
			wcol_.addLeaf("Could not create temporary directory for sqlite database at " + db_dir +
			                  ". Perhaps there is an artifact from a previous faulty run?",
			              diagnostics::WarningLevel::WARNING,
			              true);
			bool dir_created = false;
			for (int i = 2; i <= 10; ++i) {
				if (std::filesystem::create_directory(db_dir + std::to_string(i))) {
					db_path_ = db_dir + std::to_string(i) + "/" + date_time_stamp + ".db";
					dir_created = true;
					break;
				}
			}

			// this is to ensure that a user doesn't end up having too many db artifacts lieing
			// around unnoticed
			if (!dir_created) {
				throw diagnostics::Error(
				    "Storage error: could not create temporary directory for sqlite database at " +
				    db_dir +
				    "1-10/.runtime_storage.db. There might be artifacts from previous faulty "
				    "runs.");
			}
		} else {
			db_path_ = db_dir + "/" + date_time_stamp + ".db";
		}

		// NOMUTEX is fine if you guarantee single-thread access to this connection
		const int FLAGS = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
		if (sqlite3_open_v2(db_path_.string().c_str(), &db_, FLAGS, nullptr) != SQLITE_OK) {
			throw diagnostics::Error("Storage error: sqlite open failed: " +
			                         std::string(sqlite3_errmsg(db_)));
		}
		sqlite3_busy_timeout(db_, BUSY_TIMEOUT_MS_);
		sqlite3_extended_result_codes(db_, 1);

		// set pragmas
		// cache size: negative means KiB, default would be -2000 (~2MB) -> ideally high for
		// quick lookups
		auto cache_kib = static_cast<int64_t>(HEAP_BYTES_ / 1024.0 * CACHE_FRAC_);
		exec_(("PRAGMA main.cache_size = -" + std::to_string(cache_kib) + ";").c_str());

		exec_(("PRAGMA page_size = " + std::to_string(PAGE_SIZE_) + ";").c_str());

		if (TEMP_STORE_FILE_) {
			exec_("PRAGMA temp_store = 1;"); // force temp store in file, not RAM
		}

		if (EXCLUSIVE_LOCK_) {
			exec_("PRAGMA locking_mode = EXCLUSIVE;");
		}
		if (JOURNAL_OFF_) {
			exec_("PRAGMA journal_mode = OFF;");
			exec_("PRAGMA synchronous = OFF;");
		} else {
			exec_("PRAGMA journal_mode = WAL;");
			exec_("PRAGMA synchronous = NORMAL;");
		}

		exec_("PRAGMA foreign_keys = OFF;");

#if GTFS2RDF_FULL_STATS
		const auto init_end = std::chrono::steady_clock::now();
		init_ns_ +=
		    std::chrono::duration_cast<std::chrono::nanoseconds>(init_end - T_START).count();
#endif
	}

	~PersistentStorageSqlite() {
		try {
			flush_();
		} catch (...) {
			std::cerr << "Warning: exception during "
			             "PersistentStorageSqlite::~PersistentStorageSqlite flush_\n";
		}
		finaliseAll_();

		if (db_) {
			sqlite3_close(db_);
		}
		// delete temporary directory and file
		std::filesystem::remove_all(db_path_.parent_path());
	}

	// _____________________________________________________________________________________________
	// VARIABLE API
	// _____________________________________________________________________________________________

	void storeVariable(std::string_view ctx, std::string_view name, std::string_view value) {
		SCOPED_TIMER_NS(store_ns_);
		if (!isValidCTXName(ctx)) {
			throw diagnostics::Error("Storage error: invalid context name '" + std::string(ctx) +
			                         "' for variable");
		}
		if (name.empty()) {
			throw diagnostics::Error(
			    "Storage error: empty variable name for variable in context '" + std::string(ctx) +
			    "'");
		}
		variables_[std::string(ctx)][std::string(name)] = std::string(value);
	}

	const std::string& getVariable(std::string_view ctx, std::string_view name) {
		// auto ctx_it = variables_.find(std::string(ctx));
		auto ctx_it = variables_.find(ctx);
		if (ctx_it == variables_.end()) {
			return EMPTY_VARIABLE_;
		}
		// auto name_it = ctx_it->second.find(std::string(name));
		auto name_it = ctx_it->second.find(name);
		if (name_it == ctx_it->second.end()) {
			return EMPTY_VARIABLE_;
		}
		return name_it->second;
	}

	// _____________________________________________________________________________________________
	// MULTIMAP API
	// _____________________________________________________________________________________________

	void storeValue(std::string_view ctx,
	                std::string_view name,
	                std::string_view key,
	                std::string_view value) {
		SCOPED_TIMER_NS(store_ns_);
		if (key.empty()) {
			throw diagnostics::Error("Storage error: empty key for multimap '" + std::string(name) +
			                         "'");
		}
		ensureTransaction_();
		auto& mm_tab = ensureMMTable_(ctx, name);
		bindText_(mm_tab.insert, 1, key);
		bindText_(mm_tab.insert, 2, value);
		stepDone_(mm_tab.insert);
		if (++pending_ >= FLUSH_OPS_) {
			flush_();
		}
	}

	void storeValue(std::string_view ctx,
	                std::string_view name,
	                std::initializer_list<std::string_view> key_parts,
	                std::string_view value) {
		storeValue(ctx, name, concatKeyParts_(key_parts), value);
	}

	void storeValue(std::string_view ctx,
	                std::string_view name,
	                std::span<const std::string_view> key_parts,
	                std::string_view value) {
		storeValue(ctx, name, concatKeyParts_(key_parts), value);
	}

	bool containsValue(std::string_view ctx,
	                   std::string_view name,
	                   std::string_view key,
	                   std::string_view val) {
		SCOPED_TIMER_NS(read_ns_);
		const std::string TBL_NAME = makeTableName_("mm", ctx, name);
		auto itr_mm = mm_tables_.find(TBL_NAME);
		if (itr_mm == mm_tables_.end()) {
			return false; // table doesn't exist yet
		}
		auto& mm_tab = itr_mm->second;

		bindText_(mm_tab.contains, 1, key);
		bindText_(mm_tab.contains, 2, val);
		const int RET_CODE = sqlite3_step(mm_tab.contains);
		resetStmt_(mm_tab.contains);
		if (RET_CODE == SQLITE_DONE) {
			return false;
		}
		if (RET_CODE != SQLITE_ROW) {
			std::string msg = sqlite3_errmsg(db_);
			throw diagnostics::Error("Storage error: sqlite step failed in containsValue: " + msg);
		}
		return true;
	}

	bool containsValue(std::string_view ctx,
	                   std::string_view name,
	                   std::initializer_list<std::string_view> key_parts,
	                   std::string_view val) {
		return containsValue(ctx, name, concatKeyParts_(key_parts), val);
	}

	bool containsValue(std::string_view ctx,
	                   std::string_view name,
	                   std::span<const std::string_view> key_parts,
	                   std::string_view val) {
		return containsValue(ctx, name, concatKeyParts_(key_parts), val);
	}

	std::vector<std::string>
	getValues(std::string_view ctx, std::string_view name, std::string_view key) {
		SCOPED_TIMER_NS(read_ns_);
		std::vector<std::string> out;
		const std::string TBL_NAME = makeTableName_("mm", ctx, name);
		auto itr_mm = mm_tables_.find(TBL_NAME);
		if (itr_mm == mm_tables_.end()) {
			return out; // table doesn't exist yet
		}
		auto& mm_tab = itr_mm->second;

		bindText_(mm_tab.get, 1, key);
		while (true) {
			const int RET_CODE = sqlite3_step(mm_tab.get);
			if (RET_CODE == SQLITE_DONE) {
				// we're done compiling the results
				break;
			}
			if (RET_CODE != SQLITE_ROW) {
				// an error occured
				std::string msg = sqlite3_errmsg(db_);
				resetStmt_(mm_tab.get);
				throw diagnostics::Error("Storage error: sqlite step failed in getValues: " + msg);
			} else {
				const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(mm_tab.get, 0));
				int bytes = sqlite3_column_bytes(mm_tab.get, 0);
				if (txt) {
					out.emplace_back(txt, bytes);
				}
			}
		}
		resetStmt_(mm_tab.get);
		return out;
	}

	std::vector<std::string> getValues(std::string_view ctx,
	                                   std::string_view name,
	                                   std::initializer_list<std::string_view> key_parts) {
		return getValues(ctx, name, concatKeyParts_(key_parts));
	}

	std::vector<std::string> getValues(std::string_view ctx,
	                                   std::string_view name,
	                                   std::span<const std::string_view> key_parts) {
		return getValues(ctx, name, concatKeyParts_(key_parts));
	}

	// _____________________________________________________________________________________________
	// TUPLEMAP API
	// _____________________________________________________________________________________________

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::string_view key,
	                std::span<const std::string_view> tuple) {
		SCOPED_TIMER_NS(store_ns_);
		if (key.empty()) {
			throw diagnostics::Error("Storage error: empty key for tuplemap '" + std::string(name) +
			                         "'");
		}
		ensureTransaction_();
		auto& tm_tab = ensureTMTable_(ctx, name, tuple.size());
		bindText_(tm_tab.insert, 1, key);
		for (size_t i = 0; i < tuple.size(); ++i) {
			bindText_(tm_tab.insert, 2 + i, tuple[i]);
		}
		stepDone_(tm_tab.insert);
		if (++pending_ >= FLUSH_OPS_) {
			flush_();
		}
	}

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::initializer_list<std::string_view> key_parts,
	                std::span<const std::string_view> tuple) {
		storeTuple(ctx, name, concatKeyParts_(key_parts), tuple);
	}

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::string_view key,
	                std::initializer_list<std::string_view> tuple_parts) {
		storeTuple(ctx,
		           name,
		           key,
		           std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::initializer_list<std::string_view> key_parts,
	                std::initializer_list<std::string_view> tuple_parts) {
		storeTuple(ctx,
		           name,
		           concatKeyParts_(key_parts),
		           std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::span<const std::string_view> key,
	                std::span<const std::string_view> tuple) {
		storeTuple(ctx, name, concatKeyParts_(key), tuple);
	}

	void storeTuple(std::string_view ctx,
	                std::string_view name,
	                std::span<const std::string_view> key,
	                std::initializer_list<std::string_view> tuple_parts) {
		storeTuple(ctx,
		           name,
		           concatKeyParts_(key),
		           std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	std::vector<std::vector<std::string>>
	getTuples(std::string_view ctx, std::string_view name, std::string_view key) {
		SCOPED_TIMER_NS(read_ns_);
		std::vector<std::vector<std::string>> out;

		const std::string TBL_NAME = makeTableName_("tm", ctx, name);
		auto itr_tm = tm_tables_.find(TBL_NAME);
		if (itr_tm == tm_tables_.end()) {
			return out; // table doesn't exist yet
		}
		auto& tm_tab = itr_tm->second;
		bindText_(tm_tab.get, 1, key);
		while (true) {
			const int RET_CODE = sqlite3_step(tm_tab.get);
			if (RET_CODE == SQLITE_DONE) {
				// we're done compiling the results
				break;
			}
			if (RET_CODE != SQLITE_ROW) {
				// an error occured
				std::string msg = sqlite3_errmsg(db_);
				resetStmt_(tm_tab.get);
				throw diagnostics::Error("Storage error: sqlite step failed in getTuples: " + msg);
			} else {
				// read one tuple
				std::vector<std::string> tuple;
				tuple.reserve(tm_tab.arity);
				for (size_t i = 0; i < tm_tab.arity; ++i) {
					const char* txt =
					    reinterpret_cast<const char*>(sqlite3_column_text(tm_tab.get, i));
					int bytes = sqlite3_column_bytes(tm_tab.get, i);
					if (txt) {
						tuple.emplace_back(txt, bytes);
					} else {
						tuple.emplace_back("");
					}
				}
				out.emplace_back(std::move(tuple));
			}
		}
		resetStmt_(tm_tab.get);
		return out;
	}

	std::vector<std::vector<std::string>>
	getTuples(std::string_view ctx,
	          std::string_view name,
	          std::initializer_list<std::string_view> key_parts) {
		return getTuples(ctx, name, concatKeyParts_(key_parts));
	}

	std::vector<std::vector<std::string>>
	getTuples(std::string_view ctx, std::string_view name, std::span<const std::string_view> key) {
		return getTuples(ctx, name, concatKeyParts_(key));
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::string_view key,
	                   std::span<const std::string_view> tuple) {
		SCOPED_TIMER_NS(read_ns_);
		const std::string TBL_NAME = makeTableName_("tm", ctx, name);
		auto itr_tm = tm_tables_.find(TBL_NAME);
		if (itr_tm == tm_tables_.end()) {
			return false; // table doesn't exist yet
		}
		auto& tm_tab = itr_tm->second;
		if (tm_tab.arity != tuple.size()) {
			throw diagnostics::Error("Storage error: arity mismatch for table " + TBL_NAME +
			                         ": expected " + std::to_string(tm_tab.arity) + ", got " +
			                         std::to_string(tuple.size()));
		}
		bindText_(tm_tab.contains, 1, key);
		for (size_t i = 0; i < tm_tab.arity; ++i) {
			bindText_(tm_tab.contains, 2 + i, tuple[i]);
		}
		const int RET_CODE = sqlite3_step(tm_tab.contains);
		resetStmt_(tm_tab.contains);
		if (RET_CODE == SQLITE_DONE) {
			return false;
		}
		if (RET_CODE != SQLITE_ROW) {
			std::string msg = sqlite3_errmsg(db_);
			throw diagnostics::Error("Storage error: sqlite step failed in containsTuple: " + msg);
		}
		return true;
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::initializer_list<std::string_view> key_parts,
	                   std::span<const std::string_view> tuple) {
		return containsTuple(ctx, name, concatKeyParts_(key_parts), tuple);
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::string_view key,
	                   std::initializer_list<std::string_view> tuple_parts) {
		return containsTuple(
		    ctx,
		    name,
		    key,
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::initializer_list<std::string_view> key_parts,
	                   std::initializer_list<std::string_view> tuple_parts) {
		return containsTuple(
		    ctx,
		    name,
		    concatKeyParts_(key_parts),
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::span<const std::string_view> key,
	                   std::span<const std::string_view> tuple) {
		return containsTuple(ctx, name, concatKeyParts_(key), tuple);
	}

	bool containsTuple(std::string_view ctx,
	                   std::string_view name,
	                   std::span<const std::string_view> key,
	                   std::initializer_list<std::string_view> tuple_parts) {
		return containsTuple(
		    ctx,
		    name,
		    concatKeyParts_(key),
		    std::span<const std::string_view>(tuple_parts.begin(), tuple_parts.size()));
	}

	void clearContext(std::string_view ctx) {
		SCOPED_TIMER_NS(clear_ns_);
		variables_.erase(std::string(ctx));
		flush_();

		// Drop multimap tables for ctx
		if (auto itr_mm_names = ctx_mm_tables_.find(ctx); itr_mm_names != ctx_mm_tables_.end()) {
			for (const auto& MM_NAME : itr_mm_names->second) {
				dropTableAndFinaliseMM_(MM_NAME);
			}
			ctx_mm_tables_.erase(itr_mm_names);
		}

		// Drop tuplemap tables for ctx
		if (auto itr_tm_names = ctx_tm_tables_.find(ctx); itr_tm_names != ctx_tm_tables_.end()) {
			for (const auto& TM_NAME : itr_tm_names->second) {
				dropTableAndFinaliseTM_(TM_NAME);
			}
			ctx_tm_tables_.erase(itr_tm_names);
		}
	}

	double getHeapLimit_MB() const {
		return static_cast<double>(HEAP_BYTES_) / (1024.0 * 1024.0);
	}

	void stats() {
		flush_();

		std::cerr << "💿  PERSISTENT STORAGE SUMMARY\n";

// only if debug flag is set in compiler
#ifndef NDEBUG
		// In-memory
		std::cerr << "[In-memory] Variables\n";
		util::operator<<(std::cerr, variables_);
		std::cerr << "\n\n";

		auto count_table = [&](std::string_view tbl_name) -> sqlite3_int64 {
			sqlite3_stmt* stmt = nullptr;
			const std::string SEL = "SELECT COUNT(*) FROM " + std::string(tbl_name) + ";";
			prep_(stmt, SEL.c_str());
			sqlite3_int64 cnt = 0;
			if (sqlite3_step(stmt) == SQLITE_ROW)
				cnt = sqlite3_column_int64(stmt, 0);
			sqlite3_finalize(stmt);
			return cnt;
		};

		// SQLite multimaps
		std::cerr << "[SQLite] Multimap counts (ctx, name)\n\n";
		for (const auto& [MM_NAME, tm_tab] : mm_tables_) {
			std::cerr << "• [" << tm_tab.ctx << "] " << tm_tab.name
			          << "  rows=" << count_table(MM_NAME) << "\n";
		}
		std::cerr << "\n";

		// SQLite tuplemaps
		std::cerr << "[SQLite] Tuplemap counts (ctx, name)\n\n";
		for (const auto& [TM_NAME, tm_tab] : tm_tables_) {
			std::cerr << "• [" << tm_tab.ctx << "] " << tm_tab.name
			          << "  rows=" << count_table(TM_NAME) << "\n";
		}
		std::cerr << "\n";
#endif

		// DB size
		std::error_code err_code;
		const auto DB_SIZE = std::filesystem::file_size(db_path_, err_code);
		if (!err_code) {
			std::cerr << "Summary\n"
			          << "  approx DB size: " << formatValueWithPaddedUnits(DB_SIZE, UnitType::SIZE)
			          << "\n\n";
		}

#if GTFS2RDF_FULL_STATS
		// Timing block
		std::cerr << "Timing statistics\n"
		          << "  initialisation: " << formatValueWithPaddedUnits(init_ns_, UnitType::TIME)
		          << " \n"
		          << "  store:          " << formatValueWithPaddedUnits(store_ns_, UnitType::TIME)
		          << " \n"
		          << "  read:           " << formatValueWithPaddedUnits(read_ns_, UnitType::TIME)
		          << " \n"
		          << "  clear:          " << formatValueWithPaddedUnits(clear_ns_, UnitType::TIME)
		          << " \n\n";
#endif
	}

  private:
	// _____________________________________________________________________________________________
	// PRIVATE METHODS
	// _____________________________________________________________________________________________

	// creates descriptive table name for every (ctx, name) pair used in multimap/tuplemap
	static std::string
	makeTableName_(std::string_view prefix, std::string_view ctx, std::string_view name) {
		if (name.empty()) {
			throw diagnostics::Error(
			    "Storage error: empty multimap/tuplemap name for sqlite table");
		}
		if (!isValidCTXName(ctx)) {
			throw diagnostics::Error("Storage error: invalid context name '" + std::string(ctx) +
			                         "' for sqlite table");
		}
		auto [file_name, file_ext] = splitAt(ctx, '.');
		for (char c : name) {
			if (!isValidGtfsChar(c)) {
				throw diagnostics::Error("Storage error: invalid multimap/tuplemap name '" +
				                         std::string(name) + "' for sqlite table");
			}
		}
		return std::string(prefix) + "_" + std::string(file_name) + "_" + std::string(name);
	}

	// concatenate key parts with null byte separators into key_buf_
	std::string_view concatKeyParts_(std::span<const std::string_view> parts) {
		key_buf_.clear();
		size_t total = 0;
		for (auto part : parts) {
			total += part.size();
		}
		if (total == 0) {
			throw diagnostics::Error(
			    "Storage error: empty key parts in PersistentStorageSqlite::concat_key_parts");
		}
		key_buf_.reserve(total + parts.size() - 1); // size + null separators
		bool first = true;
		for (auto part : parts) {
			if (!first) {
				key_buf_.push_back('\0');
			}
			first = false;
			key_buf_.append(part.data(), part.size());
		}
		return key_buf_;
	}

	std::string_view concatKeyParts_(std::initializer_list<std::string_view> parts) {
		return concatKeyParts_(std::span<const std::string_view>(parts.begin(), parts.size()));
	}

	// _____________________________________________________________________________________________
	// SQLITE WRAPPERS
	// _____________________________________________________________________________________________

	void flush_() {
		if (transaction_active_) {
			exec_("COMMIT;");
			transaction_active_ = false;
			pending_ = 0;
		}
	}

	void exec_(const char* SQL) {
		char* err = nullptr;
		if (sqlite3_exec(db_, SQL, nullptr, nullptr, &err) != SQLITE_OK) {
			std::string msg = err ? err : sqlite3_errmsg(db_);
			sqlite3_free(err);
			throw diagnostics::Error("Storage error: sqlite exec_ failed: " + msg);
		}
	}

	void prep_(sqlite3_stmt*& stmt, const char* SQL) {
		if (sqlite3_prepare_v2(db_, SQL, -1, &stmt, nullptr) != SQLITE_OK) {
			throw diagnostics::Error(std::string("Storage error: sqlite prepare failed: ") +
			                         sqlite3_errmsg(db_));
		}
	}

	void ensureTransaction_() {
		if (!transaction_active_) {
			exec_("BEGIN;");
			transaction_active_ = true;
		}
	}

	static void bindText_(sqlite3_stmt* stmt, int idx, const std::string& str) {
		sqlite3_bind_text(stmt, idx, str.c_str(), -1, SQLITE_TRANSIENT);
	}
	static void bindText_(sqlite3_stmt* stmt, int idx, std::string_view svw) {
		sqlite3_bind_text(stmt, idx, svw.data(), (int)svw.size(), SQLITE_TRANSIENT);
	}

	static void resetStmt_(sqlite3_stmt* stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}

	static void stepDone_(sqlite3_stmt* stmt) {
		const int RET_CODE = sqlite3_step(stmt);
		if (RET_CODE != SQLITE_DONE) {
			sqlite3* db =
			    sqlite3_db_handle(stmt); // NOLINT(readability-identifier-length) : 'db' unambiguous
			std::string msg = db ? sqlite3_errmsg(db) : "unknown error";
			resetStmt_(stmt);
			throw diagnostics::Error("Storage error: sqlite step failed: " + msg);
		}
		resetStmt_(stmt);
	}

	void finaliseAll_() {
		for (auto& [MM_NAME, mm_tab] : mm_tables_) {
			if (mm_tab.insert) {
				sqlite3_finalize(mm_tab.insert);
			}
			if (mm_tab.get) {
				sqlite3_finalize(mm_tab.get);
			}
			if (mm_tab.contains) {
				sqlite3_finalize(mm_tab.contains);
			}
		}
		mm_tables_.clear();

		for (auto& [TM_NAME, tm_tab] : tm_tables_) {
			if (tm_tab.insert) {
				sqlite3_finalize(tm_tab.insert);
			}
			if (tm_tab.get) {
				sqlite3_finalize(tm_tab.get);
			}
			if (tm_tab.contains) {
				sqlite3_finalize(tm_tab.contains);
			}
		}
		tm_tables_.clear();

		ctx_mm_tables_.clear();
		ctx_tm_tables_.clear();
	}

	void dropTableAndFinaliseMM_(std::string_view mm_name) {
		auto itr_mm = mm_tables_.find(mm_name);
		if (itr_mm != mm_tables_.end()) {
			if (itr_mm->second.insert) {
				sqlite3_finalize(itr_mm->second.insert);
			}
			if (itr_mm->second.get) {
				sqlite3_finalize(itr_mm->second.get);
			}
			if (itr_mm->second.contains) {
				sqlite3_finalize(itr_mm->second.contains);
			}
			mm_tables_.erase(itr_mm);
		}
		exec_(("DROP TABLE IF EXISTS " + std::string(mm_name) + ";").c_str());
	}

	void dropTableAndFinaliseTM_(std::string_view tm_name) {
		auto itr_tm = tm_tables_.find(tm_name);
		if (itr_tm != tm_tables_.end()) {
			if (itr_tm->second.insert) {
				sqlite3_finalize(itr_tm->second.insert);
			}
			if (itr_tm->second.get) {
				sqlite3_finalize(itr_tm->second.get);
			}
			if (itr_tm->second.contains) {
				sqlite3_finalize(itr_tm->second.contains);
			}
			tm_tables_.erase(itr_tm);
		}
		exec_(("DROP TABLE IF EXISTS " + std::string(tm_name) + ";").c_str());
	}

	// Create + prepare statements lazily (on first access).
	MultiMapTable& ensureMMTable_(std::string_view ctx, std::string_view name) {
		const std::string MM_NAME = makeTableName_("mm", ctx, name);
		auto& mm_tab = mm_tables_[MM_NAME];

		if (!mm_tab.insert) {
			// minimal schema; PRIMARY KEY(key,val) keeps values sorted for a given key.
			const std::string CREATE_SQL =
			    "CREATE TABLE IF NOT EXISTS " + MM_NAME +
			    " (key TEXT NOT NULL, val TEXT NOT NULL, PRIMARY KEY(key,val)) WITHOUT ROWID;";
			exec_(CREATE_SQL.c_str());

			// prepare per-table statements
			mm_tab.ctx = ctx;
			mm_tab.name = name;
			prep_(mm_tab.insert,
			      ("INSERT OR IGNORE INTO " + MM_NAME + "(key,val) VALUES(?,?);").c_str());
			prep_(mm_tab.get,
			      ("SELECT val FROM " + MM_NAME + " WHERE key=? ORDER BY val;").c_str());
			prep_(mm_tab.contains,
			      ("SELECT 1 FROM " + MM_NAME + " WHERE key=? AND val=? LIMIT 1;").c_str());

			// Track for fast clearContext
			auto& val = getOrInsert(ctx_mm_tables_, ctx);
			if (std::find(val.begin(), val.end(), MM_NAME) == val.end()) {
				val.push_back(MM_NAME);
			}
		}
		return mm_tab;
	}

	TupleMapTable& ensureTMTable_(std::string_view ctx, std::string_view name, size_t tuple_arity) {
		if (tuple_arity == 0) {
			throw diagnostics::Error(
			    "Storage error: PersistentStorageSqlite::ensure_tm_table: zero arity");
		}
		const std::string TM_NAME = makeTableName_("tm", ctx, name);
		auto& tm_tab = tm_tables_[TM_NAME];

		if (!tm_tab.insert) {
			tm_tab.ctx = ctx;
			tm_tab.name = name;
			tm_tab.arity = tuple_arity;

			// create table with dynamic number of value columns
			std::string create_sql =
			    "CREATE TABLE IF NOT EXISTS " + TM_NAME + " (key TEXT NOT NULL";
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
			std::string insert_sql = "INSERT OR IGNORE INTO " + TM_NAME + " (key";
			for (size_t i = 0; i < tuple_arity; ++i) {
				insert_sql += ", val" + std::to_string(i);
			}
			insert_sql += ") VALUES (?";
			for (size_t i = 0; i < tuple_arity; ++i) {
				insert_sql += ", ?";
			}
			insert_sql += ");";
			prep_(tm_tab.insert, insert_sql.c_str());

			// SELECT statement for getting
			std::string get_sql = "SELECT ";
			for (size_t i = 0; i < tuple_arity; ++i) {
				if (i > 0) {
					get_sql += ", ";
				}
				get_sql += "val" + std::to_string(i);
			}
			get_sql += " FROM " + TM_NAME + " WHERE key=? ORDER BY "; // ensures ordered output
			for (size_t i = 0; i < tuple_arity; ++i) {
				if (i > 0) {
					get_sql += ", ";
				}
				get_sql += "val" + std::to_string(i);
			}
			get_sql += ";";
			prep_(tm_tab.get, get_sql.c_str());

			// SELECT statement for contains
			std::string contains_sql = "SELECT 1 FROM " + TM_NAME + " WHERE key=?";
			for (size_t i = 0; i < tuple_arity; ++i) {
				contains_sql += " AND val" + std::to_string(i) + "=?";
			}
			contains_sql += " LIMIT 1;";
			prep_(tm_tab.contains, contains_sql.c_str());

			// tracking ctx tables for fast clearContext
			auto& val = getOrInsert(ctx_tm_tables_, ctx);
			if (std::find(val.begin(), val.end(), TM_NAME) == val.end()) {
				val.push_back(TM_NAME);
			}
		} else {
			// ensure arity matches on subsequent accesses
			if (tm_tab.arity != tuple_arity) {
				throw diagnostics::Error("Storage error: PersistentStorageSqlite::ensure_tm_table: "
				                         "arity mismatch for table " +
				                         TM_NAME + ": expected " + std::to_string(tm_tab.arity) +
				                         ", got " + std::to_string(tuple_arity));
			}
		}
		return tm_tab;
	}

	// _____________________________________________________________________________________________
	// PRIVATE MEMBERS
	// _____________________________________________________________________________________________

	diagnostics::WarningCollector& wcol_;
	[[maybe_unused]] diagnostics::VerbosityLevelStats verbosity_level_stats_;
	sqlite3* db_ = nullptr;

	std::filesystem::path db_path_;

#if GTFS2RDF_FULL_STATS
	const std::chrono::steady_clock::time_point T_START = std::chrono::steady_clock::now();
	mutable uint64_t init_ns_ = 0;
	mutable uint64_t store_ns_ = 0;
	mutable uint64_t read_ns_ = 0;
	mutable uint64_t clear_ns_ = 0;
#endif

	// variables in-memory
	std::unordered_map<std::string,
	                   std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>,
	                   StringHash,
	                   std::equal_to<>>
	    variables_;

	// maps (ctx,name) to its table struct with prepared statements (and arity)
	mutable std::unordered_map<std::string, MultiMapTable, StringHash, std::equal_to<>> mm_tables_;
	mutable std::unordered_map<std::string, TupleMapTable, StringHash, std::equal_to<>> tm_tables_;

	// keep track which tables belong to which ctx -> enables fast clearContext(ctx)
	std::unordered_map<std::string, std::vector<std::string>, StringHash, std::equal_to<>>
	    ctx_mm_tables_;
	std::unordered_map<std::string, std::vector<std::string>, StringHash, std::equal_to<>>
	    ctx_tm_tables_;

	mutable std::string key_buf_; // reusable buffer for many-value keys

	bool transaction_active_ = false;
	uint32_t pending_ = 0;

	// static const emptiness objects
	static inline const std::string EMPTY_VARIABLE_;
};

} // namespace storage