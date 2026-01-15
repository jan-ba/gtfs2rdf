module;

#include <string>
#include <stdexcept>
#include <cctype>
#include <charconv>

export module t_lib;
import field_transforms;

using namespace field_transforms;

// library of field transforms that could be useful for multiple schemas
// functions will need to be registered in the TransformRegistry (see below) in order to be available
namespace t_lib {

    // _____________________________________________________________________________________________
    // factory for range-checking transform
    // Use like this in a schema file:
    // registry.registerTransform("in_range_0_100", t_lib::in_range(0.0, 100.0));

    
    // _____________________________________________________________________________________________
    // Functions for type correctness checks
    void is_int(const ArgSpan& args, std::string& out) {
        const std::string& s = args[0];
        if (s.empty()) {
            throw std::runtime_error(
                "❌ Faulty data: expected integer value, got empty string"
            );
        }

        int value = 0;
        const char* first = s.data();
        const char* last  = first + s.size();

        auto [ptr, ec] = std::from_chars(first, last, value);

        if (ec == std::errc{} && ptr == last) {
            out = s;
            return;
        } else {
            throw std::runtime_error(
                "❌ Faulty data: expected integer value, got '" + s + "'"
            );
        }
    }

    void is_decimal(const ArgSpan& args, std::string& out) {
        const std::string& s = args[0];
        auto fail = [&]() {
            throw std::runtime_error(
                "❌ Faulty data: expected decimal value, got '" + s + "'"
            );
        };

        if (s.empty()) {
            fail();
        }

        std::size_t i = 0;

        // Optional sign
        if (s[i] == '+' || s[i] == '-') {
            ++i;
            if (i == s.size()) {
                // string was just "+" or "-" -> invalid
                fail();
            }
        }

        // Zero or more digits before the decimal point
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            ++i;
        }

        // Must have a '.'
        if (i >= s.size() || s[i] != '.') {
            fail();
        }
        ++i; // skip '.'

        // one or more digits after the decimal point
        std::size_t digits_after_dot = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            ++digits_after_dot;
            ++i;
        }

        if (digits_after_dot == 0) {
            // must have at least one digit after '.'
            fail();
        }

        // no trailing junk allowed
        if (i != s.size()) {
            fail();
        }

        out = s;
    }
    
    // _____________________________________________________________________________________________ 
    // convert GTFS date "YYYYMMDD" to xsd:date "YYYY-MM-DD"
    void convert_date(const ArgSpan& args, std::string& out) {
        const std::string& s = args[0];
        if (s.empty()) {
            return;  // leave empty
        } else if (s.size() != 8) {
            throw std::runtime_error(
                "❌ Transform error: expected date in format YYYYMMDD, got '" + s + "'"
            );
        }
        out = s.substr(0,4) + "-" + s.substr(4,2) + "-" + s.substr(6,2);
    }


    // TODO: remove later / include in testing only
    void debug_wrap(const ArgSpan& args, std::string& out) {
        out = "DEBUG(" + args[0] + ")";
    }

    export void register_lib_transforms(TransformRegistry& registry) {
        registry.registerTransform("is_int", is_int);
        registry.registerTransform("is_decimal", is_decimal);
        registry.registerTransform("convert2xsd:date", convert_date);
        registry.registerTransform("debug_wrap", debug_wrap);
    }
}