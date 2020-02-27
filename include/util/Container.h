
#ifndef UTIL_CONTAINER_H_2020227_
#define UTIL_CONTAINER_H_2020227_

#include <algorithm>
#include <array>
#include <type_traits>

namespace util {

template <class T, size_t N, class U = T>
constexpr void shl(std::array<T, N> &buffer, U value = T()) {
	static_assert(std::is_convertible<T, U>::value, "U must be convertable to the type contained in the array!");
	std::rotate(buffer.begin(), buffer.begin() + 1, buffer.end());
	buffer[N - 1] = value;
}

template <class T, size_t N, class U = T>
constexpr void shr(std::array<T, N> &buffer, U value = T()) {
	static_assert(std::is_convertible<T, U>::value, "U must be convertable to the type contained in the array!");
	std::rotate(buffer.rbegin(), buffer.rbegin() + 1, buffer.rend());
	buffer[0] = value;
}

template <class T, size_t N>
constexpr void rol(std::array<T, N> &buffer) {
	std::rotate(buffer.begin(), buffer.begin() + 1, buffer.end());
}

template <class T, size_t N>
constexpr void ror(std::array<T, N> &buffer) {
	std::rotate(buffer.rbegin(), buffer.rbegin() + 1, buffer.rend());
}

template <typename T, typename... Tail>
constexpr auto make_array(T head, Tail... tail) -> std::array<T, 1 + sizeof...(Tail)> {
	return std::array<T, 1 + sizeof...(Tail)>{head, tail...};
}

template <typename Container, typename Element>
bool contains(const Container &container, const Element &element) {
	return std::find(std::begin(container), std::end(container), element) != std::end(container);
}

template <typename Container, typename Pred>
bool contains_if(const Container &container, Pred pred) {
	return std::find_if(std::begin(container), std::end(container), pred) != std::end(container);
}

}

#endif
