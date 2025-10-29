module;
#include <vector>
#include <iostream>
#include <map>

export module utility;

export namespace util {
    template<typename T>
    std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
        os << "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            os << vec[i];
            if (i < vec.size() - 1) {
                os << ", ";
            }
        }
        os << "]";
        return os;
    }

    template<typename K, typename V>
    std::ostream& operator<<(std::ostream& os, const std::map<K,V>& map) {
        os << "{";
        for (auto it = map.begin(); it != map.end(); ++it) {
            os << it->first << ": " << it->second;
            if (std::next(it) != map.end()) {
                os << ", ";
            }
        }
        os << "}";
        return os;
    }

} // namespace