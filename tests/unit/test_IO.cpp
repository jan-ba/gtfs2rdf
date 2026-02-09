// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <zip.h>

import runtime;
import writer;
import gtfs_parser;
import schema;
import field_transforms;

namespace {

class TempZipFile {
  public:
	TempZipFile(std::string_view content) {
		zip_file_path_ =
		    "./tmp_" + std::to_string(std::hash<std::string_view>{}(content)) + "/content.zip";
		int err_code = 0;
		zip_t* zip = zip_open(zip_file_path_.c_str(), ZIP_CREATE | ZIP_EXCL, &err_code);
		if (!zip) {
			zip_error_t zip_error;
			zip_error_init_with_code(&zip_error, err_code);
			std::string error_message =
			    "Failed to create zip file: " + std::string(zip_error_strerror(&zip_error));
			zip_error_fini(&zip_error);
			throw std::runtime_error(error_message);
		}
		zip_source_t* source = zip_source_buffer(zip, content.data(), content.size(), 0);
		if (!source) {
			zip_close(zip);
			throw std::runtime_error("Failed to create zip source");
		}
		if (zip_file_add(zip, "content.csv", source, ZIP_FL_OVERWRITE) < 0) {
			zip_source_free(source);
			zip_close(zip);
			throw std::runtime_error("Failed to add file to zip");
		}
		if (zip_close(zip) < 0) {
			throw std::runtime_error("Failed to close zip file");
		}
	}

	~TempZipFile() {
		// close handles and delete the temporary zip file
		if (zip_handle_) {
			zip_close(zip_handle_);
		}
		if (file_handle_) {
			zip_fclose(file_handle_);
		}
		std::filesystem::remove(zip_file_path_);
	}

	zip_file_t* getZipHandle() {
		return file_handle_;
	}

  private:
	std::filesystem::path zip_file_path_;
	zip_t* zip_handle_ = nullptr;
	zip_file_t* file_handle_ = nullptr;
};

runtime::RuntimeContainer makeRTC(double read_mb, double write_mb) {
	runtime::Settings settings(false, false, read_mb, write_mb);
	field_transforms::TransformRegistry registry;

	return runtime::RuntimeContainer(settings, registry);
}

} // namespace
