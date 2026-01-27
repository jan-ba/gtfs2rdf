module;

#include "third_party/cxxopts/cxxopts.hpp"
#include "third_party/sqlite3/sqlite3.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

export module runtime;

import util;
import field_transforms;

namespace runtime {

export class SqliteBackingStore {
  public:
	explicit SqliteBackingStore() {
		// create temporary directory and file
		// ensure directory didn't exist before to avoid accidental user data overwrite
		if (std::filesystem::exists("./.tmp/"))
			throw std::runtime_error(
			    "Temporary directory './.tmp/' already exists. Aborting to avoid data loss.");
		std::filesystem::create_directories("./.tmp/");
		std::filesystem::path path = "./.tmp/.runtime_storage.db";

		if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
			throw std::runtime_error(sqlite3_errmsg(db_));

		exec("PRAGMA journal_mode=WAL;");
		exec("PRAGMA synchronous=NORMAL;");
		exec("CREATE TABLE IF NOT EXISTS multimap (ctx TEXT NOT NULL, name TEXT NOT NULL, k TEXT "
		     "NOT NULL, v TEXT NOT NULL, PRIMARY KEY(ctx,name,k,v)) WITHOUT ROWID;");
		exec("CREATE TABLE IF NOT EXISTS tuplemap (ctx TEXT NOT NULL, name TEXT NOT NULL, k TEXT "
		     "NOT NULL, v BLOB NOT NULL, PRIMARY KEY(ctx,name,k,v)) WITHOUT ROWID;");

		prep(insert_mm_, "INSERT OR IGNORE INTO multimap(ctx,name,k,v) VALUES (?,?,?,?);");
		prep(get_mm_, "SELECT v FROM multimap WHERE ctx=? AND name=? AND k=? ORDER BY v;");
		prep(has_mm_, "SELECT 1 FROM multimap WHERE ctx=? AND name=? AND k=? AND v=? LIMIT 1;");

		prep(insert_tm_, "INSERT OR IGNORE INTO tuplemap(ctx,name,k,v) VALUES (?,?,?,?);");
		prep(get_tm_, "SELECT v FROM tuplemap WHERE ctx=? AND name=? AND k=?;");
		prep(del_ctx_, "DELETE FROM multimap WHERE ctx=?; DELETE FROM tuplemap WHERE ctx=?;");
	}

	~SqliteBackingStore() {
		flush();
		finalize_all();
		if (db_)
			sqlite3_close(db_);
		// delete temporary directory and file
		std::filesystem::remove_all("./.tmp/");
	}

	void store_multimap(const std::string &ctx,
	                    const std::string &name,
	                    const std::string &key,
	                    std::string_view val) {
		begin_if_needed();
		bind4(insert_mm_, ctx, name, key, val);
		step_reset(insert_mm_);
		bump_flush();
	}

	bool contains_multimap(const std::string &ctx,
	                       const std::string &name,
	                       const std::string &key,
	                       const std::string &val) {
		bind4(has_mm_, ctx, name, key, val);
		int rc = sqlite3_step(has_mm_);
		sqlite3_reset(has_mm_);
		return rc == SQLITE_ROW;
	}

	std::vector<std::string>
	get_multimap(const std::string &ctx, const std::string &name, const std::string &key) {
		std::vector<std::string> out;
		bind3(get_mm_, ctx, name, key);
		while (sqlite3_step(get_mm_) == SQLITE_ROW) {
			const unsigned char *txt = sqlite3_column_text(get_mm_, 0);
			out.emplace_back(reinterpret_cast<const char *>(txt ? txt : (const unsigned char *)""));
		}
		sqlite3_reset(get_mm_);
		return out; // already sorted by v
	}

	void clear_context(const std::string &ctx) {
		begin_if_needed();
		// del_ctx_ has two statements; easiest is exec with parameterized two statements split,
		// or prepare two delete statements instead.
	}

	void flush() {
		if (in_tx_) {
			exec("COMMIT;");
			in_tx_ = false;
			pending_ = 0;
		}
	}

  private:
	sqlite3 *db_ = nullptr;
	sqlite3_stmt *insert_mm_ = nullptr, *get_mm_ = nullptr, *has_mm_ = nullptr;
	sqlite3_stmt *insert_tm_ = nullptr, *get_tm_ = nullptr, *del_ctx_ = nullptr;

	bool in_tx_ = false;
	int pending_ = 0;
	static constexpr int kFlushOps = 1000;

	void exec(const char *sql) {
		char *err = nullptr;
		if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
			std::string msg = err ? err : "sqlite error";
			sqlite3_free(err);
			throw std::runtime_error(msg);
		}
	}
	void prep(sqlite3_stmt *&st, const char *sql) {
		if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
			throw std::runtime_error(sqlite3_errmsg(db_));
	}
	void begin_if_needed() {
		if (!in_tx_) {
			exec("BEGIN IMMEDIATE;");
			in_tx_ = true;
		}
	}
	void bump_flush() {
		if (++pending_ >= kFlushOps)
			flush();
	}

	// helpers to bind and step
	static void step_reset(sqlite3_stmt *st) {
		int rc = sqlite3_step(st);
		sqlite3_reset(st);
		if (rc != SQLITE_DONE)
			throw std::runtime_error("sqlite step failed");
	}
	static void
	bind3(sqlite3_stmt *st, const std::string &a, const std::string &b, const std::string &c) {
		sqlite3_bind_text(st, 1, a.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(st, 2, b.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(st, 3, c.c_str(), -1, SQLITE_TRANSIENT);
	}
	static void bind4(sqlite3_stmt *st,
	                  const std::string &a,
	                  const std::string &b,
	                  const std::string &c,
	                  std::string_view d) {
		sqlite3_bind_text(st, 1, a.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(st, 2, b.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(st, 3, c.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(st, 4, d.data(), (int)d.size(), SQLITE_TRANSIENT);
	}

	void finalize_all() {
		auto fin = [&](sqlite3_stmt *&s) {
			if (s)
				sqlite3_finalize(s), s = nullptr;
		};
		fin(insert_mm_);
		fin(get_mm_);
		fin(has_mm_);
		fin(insert_tm_);
		fin(get_tm_);
		fin(del_ctx_);
	}
};

// Persistent storage for variables, multimaps, and tuplemaps across schema executions / gtfs file
// reads. Data is namespaced by context (usually file name).
class PersistentStorage {
  public:
	// --- VARIABLES API ---

	void store(const std::string &ctx, const std::string &variable, std::string_view value) {
		variables_[ctx][variable] = value;
	}

	const std::string &get(const std::string &ctx, const std::string &variable) const {
		auto it = variables_.find(ctx);
		if (it == variables_.end())
			return empty_;
		auto it2 = it->second.find(variable);
		if (it2 == it->second.end())
			return empty_;
		return it2->second;
	}

	// --- MULTIMAPS API ---

	void store(const std::string &ctx,
	           const std::string &multimap,
	           const std::string &key,
	           std::string_view value) {
		multimaps_[ctx][multimap][key].push_back(std::string(value));
	}

	const std::vector<std::string> &
	get(const std::string &ctx, const std::string &multimap, const std::string &key) const {
		auto it = multimaps_.find(ctx);
		if (it == multimaps_.end())
			return empty_vector_;
		auto it2 = it->second.find(multimap);
		if (it2 == it->second.end())
			return empty_vector_;
		auto it3 = it2->second.find(key);
		if (it3 == it2->second.end())
			return empty_vector_;
		return it3->second;
	}

	// sorts all value lists in a given multimap for a given context for quick lookups
	void finalise_multimaps(const std::string &ctx) {
		auto it = multimaps_.find(ctx);
		if (it == multimaps_.end())
			return;
		for (auto &[multimap_name, map] : it->second) {
			for (auto &[key, values] : map) {
				std::sort(values.begin(), values.end());
			}
		}
	}

	// Check if a value exists in a multimap
	// invariant: multimaps are finalised before this is called, else binary search won't work
	bool contains(const std::string &ctx,
	              const std::string &multimap,
	              const std::string &key,
	              const std::string &value) const {
		const auto &vec = get(ctx, multimap, key);
		if (vec.empty())
			return false;
		return std::binary_search(vec.begin(), vec.end(), value);
	}

	// --- TUPLEMAPS API ---

	void store(const std::string &ctx,
	           const std::string &tuplemap,
	           const std::vector<std::string> &key,
	           const std::vector<std::string> &value_tuple) {
		tuplemaps_[ctx][tuplemap][util::concat(key)].push_back(value_tuple);
	}

	const std::vector<std::vector<std::string>> &get(const std::string &ctx,
	                                                 const std::string &tuplemap,
	                                                 const std::vector<std::string> &key) const {
		std::string key_combined = util::concat(key);
		auto it = tuplemaps_.find(ctx);
		if (it == tuplemaps_.end())
			return empty_vector_of_vectors_;
		auto it2 = it->second.find(tuplemap);
		if (it2 == it->second.end())
			return empty_vector_of_vectors_;
		auto it3 = it2->second.find(key_combined);
		if (it3 == it2->second.end())
			return empty_vector_of_vectors_;
		return it3->second;
	}

	// clear all stored data for a given context once it goes out of scope
	void clear_context(const std::string &ctx) {
		variables_.erase(ctx);
		multimaps_.erase(ctx);
		tuplemaps_.erase(ctx);
	}

	// getters for testing
	const auto &getAllVariables() const { return variables_; }
	const auto &getAllMultimaps() const { return multimaps_; }
	const auto &getAllTuplemaps() const { return tuplemaps_; }

  private:
	// context (this is data for one file/schema) -> (variable name -> value)
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> variables_;

	// context -> (multimap name -> (key -> list(values)))
	std::unordered_map<
	    std::string,
	    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>>>
	    multimaps_;

	// context -> (tuplemap name -> (key -> list(value_tuples)))
	std::unordered_map<
	    std::string,
	    std::unordered_map<std::string,
	                       std::unordered_map<std::string, std::vector<std::vector<std::string>>>>>
	    tuplemaps_;

	static inline const std::string empty_ = "";

	static inline const std::vector<std::string> empty_vector_ = {};

	static inline const std::vector<std::vector<std::string>> empty_vector_of_vectors_ = {};
};

export class Settings {
  public:
	// checks validity of command line arguments and sets settings accordingly
	Settings(int argc, char *argv[]) {
		cxxopts::Options opts("gtfs2rdf",
		                      "GTFS->RDF converter\n"
		                      "Note: garbage in, garbage out. Validate your GTFS feed first.\n"
		                      "\n"
		                      "Examples:\n"
		                      "  gtfs2rdf feed.zip --format nt\n"
		                      "  gtfs2rdf --dataset feed.zip --output out/\n");

		opts.custom_help("[options]");

		opts.add_options("Standard")(
		    "d,dataset", "Path to GTFS .zip archive", cxxopts::value<std::string>())(
		    "o,output", "Output directory", cxxopts::value<std::string>()->default_value("."))(
		    "format", "Output format: ttl|nt", cxxopts::value<std::string>()->default_value("ttl"))(
		    "overwrite",
		    "Overwrite existing output files.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("h,help",
		                                                                            "Show help");

		opts.add_options("RAM / runtime")(
		    "read-buffer-size",
		    "Read buffer size in MB (bigger = more RAM, often faster)",
		    cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT)))(
		    "write-buffer-size",
		    "Write buffer size in MB (bigger = more RAM, fewer flushes)",
		    cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT)))(
		    "grouping",
		    "Enable grouping in schemas with buffer size in MB (0 = disabled)",
		    cxxopts::value<double>()->default_value("0"));

		opts.add_options("Diagnostics / advanced")(
		    "spec-dump",
		    "Dump ontology spec to disk",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "warning-level",
		    "Warning verbosity: quiet|default|verbose",
		    cxxopts::value<std::string>()->default_value("quiet"))(
		    "strict",
		    "Fail fast: treat warnings/issues as errors.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "stats",
		    "Statistics mode: none|verbose|dry-verbose",
		    cxxopts::value<std::string>()->default_value("none")->implicit_value("verbose"));

		opts.parse_positional({"dataset"});
		opts.positional_help("GTFS_ZIP");

		auto result = opts.parse(argc, argv);

		if (result.count("help")) {
			std::cout << opts.help({"Standard", "RAM / runtime", "Diagnostics / advanced"}) << "\n";
			std::exit(0);
		}

		if (!result.count("dataset")) {
			std::cerr << "Input GTFS dataset required. Type --help for more information!\n";
			std::exit(1);
		}

		// output format
		std::string format = result["format"].as<std::string>();
		if (format != "ttl" && format != "nt") {
			throw std::runtime_error("❌  Error: Unsupported output format '" + format +
			                         "'. "
			                         "Supported formats are 'ttl' and 'nt'.\n");
		}
		ntriples_output_ = (format == "nt");

		spec_dump_ = result["spec-dump"].as<bool>();

		// read buffer size
		read_buffer_size_mb_ = result["read-buffer-size"].as<double>();
		if (read_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Warning: read buffer size must be positive. Using default value of "
			          << READ_CHUNK_SIZE_DEFAULT << " mb.\n";
			read_buffer_size_mb_ = READ_CHUNK_SIZE_DEFAULT;
		}

		// write buffer size
		write_buffer_size_mb_ = result["write-buffer-size"].as<double>();
		if (write_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Warning: write buffer size must be positive. Using default value of "
			          << WRITE_CHUNK_SIZE_DEFAULT << " mb.\n";
			write_buffer_size_mb_ = WRITE_CHUNK_SIZE_DEFAULT;
		}

		// input path
		inputPath_ = result["dataset"].as<std::string>();
		if (!std::filesystem::is_regular_file(inputPath_) || inputPath_.extension() != ".zip") {
			throw std::runtime_error("❌  Error: Input '" + inputPath_.string() +
			                         "' doesn't exist or "
			                         "is not a zip file.\n");
		}

		// output path
		std::string file_ext = ntriples_output_ ? ".nt" : ".ttl";
		outputPath_ =
		    result["output"].as<std::string>() + "/" + inputPath_.stem().string() + file_ext;

		overwrite_output_ = result["overwrite"].as<bool>();
	}

	// GETTERs
	bool isNTriplesOutput() const { return ntriples_output_; }
	bool isDebugWarnings() const { return debug_warnings_; }
	bool isSpecDump() const { return spec_dump_; }
	double ReadBufferSizeMB() const { return read_buffer_size_mb_; }
	double WriteBufferSizeMB() const { return write_buffer_size_mb_; }
	bool isOverwriteOutput() const { return overwrite_output_; }
	const std::filesystem::path &InputPath() const { return inputPath_; }
	const std::filesystem::path &OutputPath() const { return outputPath_; }

  private:
	// default parameters
	const double READ_CHUNK_SIZE_DEFAULT = 10.0;
	const double WRITE_CHUNK_SIZE_DEFAULT = 20.0;

	bool ntriples_output_;
	bool debug_warnings_;
	bool spec_dump_;
	double read_buffer_size_mb_;
	double write_buffer_size_mb_;
	bool overwrite_output_;

	std::filesystem::path inputPath_;
	std::filesystem::path outputPath_;
};

// container for runtime settings, transform registry, and persistent storage across GTFS files
export class RuntimeContainer {
  public:
	RuntimeContainer(const Settings &settings, field_transforms::TransformRegistry &registry)
	    : settings_(settings)
	    , registry_(registry) {}

	const Settings &getSettings() const { return settings_; }
	field_transforms::TransformRegistry &getTransformRegistry() { return registry_; }
	const field_transforms::TransformRegistry &getConstTransformRegistry() const {
		return registry_;
	}
	PersistentStorage &getStorage() { return storage_; }

  private:
	const Settings &settings_;
	field_transforms::TransformRegistry &registry_;
	PersistentStorage storage_;
	// SqliteBackingStore sqlite_store_;
};

// container for runtime statistics collected during GTFS processing
export class Statistics {
  public:
	uint32_t chunks = 0;
	uint64_t rows = 0;
	uint64_t triples = 0;
	double parse_s = 0.;
	double write_s = 0.;

	std::string fancyPrint(const std::string &name) {
		std::ostringstream oss;
		oss << "\n📊 Statistics for " << name << ":\n"
		    << "  Chunks processed:  " << chunks << "\n"
		    << "  Rows parsed:       " << rows << "\n"
		    << "  Triples generated: " << triples << "\n"
		    << "  Parse time:        " << std::fixed << std::setprecision(2) << parse_s << "s\n"
		    << "  Write time:        " << std::fixed << std::setprecision(2) << write_s << "s\n"
		    << "  Total time:        " << std::fixed << std::setprecision(2) << (parse_s + write_s)
		    << "s\n";
		return oss.str();
	}

	std::string briefPrint(const std::string &name) {
		std::ostringstream oss;
		oss << "📈 " << name << ": " << rows << " rows, " << triples << " triples, " << std::fixed
		    << std::setprecision(2) << parse_s << "s parse + " << write_s << "s write";
		return oss.str();
	}

	Statistics operator+(const Statistics &other) const {
		Statistics result;
		result.chunks = chunks + other.chunks;
		result.rows = rows + other.rows;
		result.triples = triples + other.triples;
		result.parse_s = parse_s + other.parse_s;
		result.write_s = write_s + other.write_s;
		return result;
	}
};

} // namespace runtime