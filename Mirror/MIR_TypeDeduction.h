#pragma once

// From: https://stackoverflow.com/questions/9407367/determine-if-a-type-is-an-stl-container-at-compile-time/31105859#31105859

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Specialize a type for all of the STL containers.
namespace is_stl_container_impl {
	template <typename T>       struct is_stl_container :std::false_type {};

	template <typename T, std::size_t N> struct is_stl_container<std::array    <T, N>> :std::true_type {};

	template <typename... Args> struct is_stl_container<std::deque             <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::forward_list      <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::list              <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::map               <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::multimap          <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::multiset          <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::queue             <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::set               <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::stack             <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::pair			   <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::priority_queue    <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::tuple             <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::unordered_set     <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::unordered_multiset<Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::unordered_map     <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::unordered_multimap<Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_container<std::vector            <Args...>> :std::true_type {};
}

namespace is_stl_array_impl {
	template <typename T>				 struct is_stl_array:std::false_type {};
	template <typename T, std::size_t N> struct is_stl_array<std::array <T, N>> :std::true_type {};
}

namespace is_stl_deque_impl {
	template <typename T>       struct is_stl_deque:std::false_type {};
	template <typename... Args> struct is_stl_deque<std::deque <Args...>> :std::true_type {};
}

namespace is_stl_forward_list_impl {
	template <typename T>       struct is_stl_forward_list:std::false_type {};
	template <typename... Args> struct is_stl_forward_list<std::forward_list <Args...>> :std::true_type {};
}

namespace is_stl_list_impl {
	template <typename T>       struct is_stl_list:std::false_type {};
	template <typename... Args> struct is_stl_list<std::list <Args...>> :std::true_type {};
}

namespace is_stl_map_impl { // #TODO Review coupling unordered and ordered types
	template <typename T>       struct is_stl_map:std::false_type {};
	template <typename... Args> struct is_stl_map<std::map <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_map<std::unordered_map <Args...>> :std::true_type {};
}

namespace is_stl_multi_map_impl { // #TODO Review coupling unordered and ordered types
	template <typename T>       struct is_stl_multi_map:std::false_type {};
	template <typename... Args> struct is_stl_multi_map<std::multimap <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_multi_map<std::unordered_multimap <Args...>> :std::true_type {};
}

namespace is_stl_multi_set_impl { // #TODO Review coupling unordered and ordered types
	template <typename T>       struct is_stl_multi_set:std::false_type {};
	template <typename... Args> struct is_stl_multi_set<std::multiset <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_multi_set<std::unordered_multiset <Args...>> :std::true_type {};
}

namespace is_stl_queue_impl {
	template <typename T>       struct is_stl_queue:std::false_type {};
	template <typename... Args> struct is_stl_queue<std::queue <Args...>> :std::true_type {};
}

namespace is_stl_set_impl { // #TODO Review coupling unordered and ordered types
	template <typename T>       struct is_stl_set:std::false_type {};
	template <typename... Args> struct is_stl_set<std::set <Args...>> :std::true_type {};
	template <typename... Args> struct is_stl_set<std::unordered_set <Args...>> :std::true_type {};
}

namespace is_stl_stack_impl {
	template <typename T>       struct is_stl_stack:std::false_type {};
	template <typename... Args> struct is_stl_stack<std::stack <Args...>> :std::true_type {};
}

namespace is_stl_pair_impl {
	template <typename T>       struct is_stl_pair:std::false_type {};
	template <typename... Args> struct is_stl_pair<std::pair <Args...>> :std::true_type {};
}

namespace is_stl_priority_queue_impl {
	template <typename T>       struct is_stl_priority_queue:std::false_type {};
	template <typename... Args> struct is_stl_priority_queue<std::priority_queue <Args...>> :std::true_type {};
}

namespace is_stl_tuple_impl {
	template <typename T>       struct is_stl_tuple:std::false_type {};
	template <typename... Args> struct is_stl_tuple<std::tuple <Args...>> :std::true_type {};
}

namespace is_stl_vector_impl {
	template <typename T>       struct is_stl_vector:std::false_type {};
	template <typename... Args> struct is_stl_vector<std::vector <Args...>> :std::true_type {};
}

// Type trait to utilize the implementation type traits as well as decay the type
template <typename T> struct is_stl_container {
	static constexpr bool const value = is_stl_container_impl::is_stl_container<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_array {
	static constexpr bool const value = is_stl_array_impl::is_stl_array<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_deque {
	static constexpr bool const value = is_stl_deque_impl::is_stl_deque<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_forward_list {
	static constexpr bool const value = is_stl_forward_list_impl::is_stl_forward_list<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_list {
	static constexpr bool const value = is_stl_list_impl::is_stl_list<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_map {
	static constexpr bool const value = is_stl_map_impl::is_stl_map<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_multi_map {
	static constexpr bool const value = is_stl_multi_map_impl::is_stl_multi_map<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_multi_set {
	static constexpr bool const value = is_stl_multi_set_impl::is_stl_multi_set<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_queue {
	static constexpr bool const value = is_stl_queue_impl::is_stl_queue<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_set {
	static constexpr bool const value = is_stl_set_impl::is_stl_set<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_stack {
	static constexpr bool const value = is_stl_stack_impl::is_stl_stack<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_pair {
	static constexpr bool const value = is_stl_pair_impl::is_stl_pair<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_priority_queue {
	static constexpr bool const value = is_stl_priority_queue_impl::is_stl_priority_queue<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_tuple {
	static constexpr bool const value = is_stl_tuple_impl::is_stl_tuple<std::decay_t<T>>::value;
};

template <typename T> struct is_stl_vector {
	static constexpr bool const value = is_stl_vector_impl::is_stl_vector<std::decay_t<T>>::value;
};
