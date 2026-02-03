#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace diagnostics {

struct Error : std::runtime_error {
	explicit Error(const std::string &message)
	    : std::runtime_error(message) {
	}
};

inline void wrap_and_rethrow([[maybe_unused]] const diagnostics::Error &e,
                             std::string_view context_message) {
	std::throw_with_nested(Error(std::string(context_message)));
}

// helper function to wrap and rethrow exceptions with additional context
// const Error& to enforce correct usage
inline void print_error_chain(const diagnostics::Error &e) {
	bool printed_context = false;

	auto rec = [&](auto &&self, const diagnostics::Error &ex) -> void {
		try {
			std::rethrow_if_nested(ex);
		} catch (const diagnostics::Error &inner) {
			self(self, inner); // print leaf first

			if (!printed_context) {
				std::cerr << "Context:\n";
				printed_context = true;
			}
			std::cerr << "  - " << ex.what() << "\n";
			return;
		}
		// leaf
		std::cerr << "❌ " << ex.what() << "\n";
	};

	rec(rec, e);
}

} // namespace diagnostics