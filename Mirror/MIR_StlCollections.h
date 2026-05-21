#pragma once

#include "MIR_Structs.h"
#include "MIR_TypeDeduction.h"
#include "MIR_Ids.h"

// #NOTE Support for reflecting std collections enabled if 1, or removed if 0
									   // Testing vs Final release (TBD)
#define MIR_COLLECTION_STD_ARRAY			1 // 1
#define MIR_COLLECTION_STD_DEQUE			1 // 0
#define MIR_COLLECTION_STD_FORWARD_LIST		1 // 0
#define MIR_COLLECTION_STD_LIST				1 // 0
#define MIR_COLLECTION_STD_MAP				1 // 1
#define MIR_COLLECTION_STD_MULTI_MAP		1 // 0
#define MIR_COLLECTION_STD_MULTI_SET		1 // 0
#define MIR_COLLECTION_STD_QUEUE			1 // 0
#define MIR_COLLECTION_STD_SET				1 // 0
#define MIR_COLLECTION_STD_STACK			1 // 0
#define MIR_COLLECTION_STD_PAIR				1 // 1
#define MIR_COLLECTION_STD_PRIORITY_QUEUE	1 // 0
#define MIR_COLLECTION_STD_TUPLE			1 // 0
#define MIR_COLLECTION_STD_VECTOR			1 // 1

#define MIR_COLLECTION_STD_ALL				0 // #TODO Look at a more convenient way to just support all STL/STD types

// #TODO multimap forced const key type
#define GENERIC_ITERATE_LAMBDA														\
	mutableTypeInfo->collectionIterateCurrentFunc =									\
		[](const void* collectionObjAddress, size_t aIndex) -> char* {				\
		T* collection = (T*)collectionObjAddress;									\
		T* lastCollectionAddress = nullptr;											\
		static auto iterator = collection->begin();									\
		static size_t lastIndex = UINT64_MAX;										\
																					\
		if (aIndex < lastIndex || lastCollectionAddress != collectionObjAddress)    \
		{																			\
			lastCollectionAddress = (T*)collectionObjAddress;						\
			iterator = collection->begin();											\
			for (size_t i = 0; i < aIndex; i++)										\
			{																		\
				++iterator;															\
				if (collection->end() == iterator)									\
				{																	\
					return nullptr;													\
				}																	\
			}																		\
		}																			\
		else {																		\
			for (size_t i = 0; i < aIndex - lastIndex; i++)							\
			{																		\
				++iterator;															\
				if (collection->end() == iterator)									\
				{																	\
					return nullptr;													\
				}																	\
			}																		\
		}																			\
		lastIndex = aIndex;															\
																					\
		if (collection->end() == iterator)											\
		{																			\
			return nullptr;															\
		}																			\
		return (char*)&(*iterator);													\
	};

#define DEEP_COPY_ITERATE_FUNC																										\
mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionObjAddress, size_t aIndex) -> char* {						\
		/* #TODO See if copying the collection can be avoided in order to non-destructively iterate non-random access collection*/	\
		static T cache;																												\
		static uint64_t cacheSize = 0;																								\
		static const void* lastInstanceAddress = nullptr;																			\
		static uint64_t lastIndex = 0;																								\
																																	\
		if (!collectionObjAddress || (!lastInstanceAddress && !collectionObjAddress))												\
		{																															\
			return nullptr;																											\
		}																															\
																																	\
		if (collectionObjAddress != lastInstanceAddress || aIndex < lastIndex)														\
		{																															\
			cache = *(T*)collectionObjAddress;																						\
			cacheSize = cache.size();																								\
			lastInstanceAddress = collectionObjAddress;																				\
			lastIndex = 0;																											\
		}																															\
																																	\
		if (aIndex >= cacheSize)																									\
		{																															\
			return nullptr;																											\
		}																															\
																																	\
		uint64_t range = aIndex - lastIndex;																						\
																																	\
		for (size_t i = 0; i < range; i++)																							\
		{																															\
			cache.pop();																											\
		}																															\
																																	\
		lastIndex = aIndex;

template <typename T>
static void SetConstructionLambda(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetConstructionLambda(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(std::is_class_v<T> || std::is_same_v<T, std::string>, "Type T is not a class!");
	mutableTypeInfo->typeConstructorFunc = [](void* preallocatedMemoryAddress) { new(preallocatedMemoryAddress) T; };
}

#if defined(MIR_COLLECTION_STD_ARRAY) && MIR_COLLECTION_STD_ARRAY
template <typename T>
static void SetCollectionLambdasArray(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasArray(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_array<T>::value, "Type T is not of type stl::array!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t elementIndex, const void* elementFirst) {
		T& arrRef = *(T*)collectionObjAddress;
		arrRef[elementIndex] = (*(typename T::value_type*)elementFirst);
	};
	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionObjAddress, size_t elementIndex) -> char* {
		static size_t staticIndex = 0; // #TODO Support iterating backwards over a collection, and random access
		T* vector = ((T*)collectionObjAddress);
		if (elementIndex < staticIndex) staticIndex = elementIndex;
		if (staticIndex >= vector->size()) { staticIndex = 0; return nullptr; }
		return (char*)vector->data() + (sizeof(T::value_type) * staticIndex++);
	};
}
#endif // defined(MIR_COLLECTION_STD_ARRAY) && MIR_COLLECTION_STD_ARRAY

#if defined(MIR_COLLECTION_STD_DEQUE) && MIR_COLLECTION_STD_DEQUE
template <typename T>
static void SetCollectionLambdasDeque(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasDeque(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_deque<T>::value, "Type T is not of type stl::deque!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->push_back(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_DEQUE) && MIR_COLLECTION_STD_DEQUE

#if defined(MIR_COLLECTION_STD_FORWARD_LIST) && MIR_COLLECTION_STD_FORWARD_LIST
template <typename T>
static void SetCollectionLambdasForwardList(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasForwardList(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_forward_list<T>::value, "Type T is not of type stl::forward_list!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t aIndex, const void* elementFirst) {
		// #NOTE Assumed reversed order on serialize
		((T*)collectionObjAddress)->emplace_front(*(typename T::value_type*)elementFirst);
	};

	// #TODO May need to read the entire list into a vector, then return indices in reversed order so deserialize can keep order.
	// The issue is that the linked list is on directional, or singly linked so push back is not possible
	// GENERIC_ITERATE_LAMBDA

	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionObjAddress, size_t aIndex) -> char* {
		// #NOTE Must reverse ordering of collection during serialization as deserializing forward_list can only add to front of songly linked list
		T* collection = (T*)collectionObjAddress;
		static size_t lastIndex = 0;
		static std::vector<T::value_type> cacheVector;
		const void* lastCollectionAddress = nullptr;

		if (!collectionObjAddress || (!collectionObjAddress && lastCollectionAddress))
		{
			return nullptr;
		}

		if (lastCollectionAddress != collectionObjAddress || aIndex < lastIndex)
		{
			lastIndex = 0;
			cacheVector.clear();
			for (auto& element : *collection)
			{
				cacheVector.emplace_back(element); // #TODO Avoid additional dynamic/heap re-allocations and memory copies/moves
			}
			// std::reverse(cacheVector.begin(), cacheVector.end()); // #TODO Reverse index instead of moving all elements in vector
			lastCollectionAddress = collectionObjAddress;
		}

		if (aIndex >= cacheVector.size())
		{
			return nullptr;
		}
		lastIndex = aIndex;

		uint64_t reversedIndex = cacheVector.size() - 1 - aIndex; // #NOTE Reverse index to avoid reversing the entire cacheVector after all allocations
		return (char*)&cacheVector[reversedIndex];
	};
}
#endif // defined(MIR_COLLECTION_STD_FORWARD_LIST) && MIR_COLLECTION_STD_LIST

#if defined(MIR_COLLECTION_STD_LIST) && MIR_COLLECTION_STD_LIST
template <typename T>
static void SetCollectionLambdasList(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasList(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_list<T>::value, "Type T is not of type stl::list!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->emplace_back(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_LIST) && MIR_COLLECTION_STD_LIST

#if defined(MIR_COLLECTION_STD_MAP) && MIR_COLLECTION_STD_MAP
template <typename T>
static void SetCollectionLambdasMap(Mirror::TypeInfo* mutableTypeInfo, std::false_type) { }

template <typename T>
static void SetCollectionLambdasMap(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_map<T>::value, "Type T is not a map!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->insert(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_MAP) && MIR_COLLECTION_STD_MAP

#if defined(MIR_COLLECTION_STD_MULTI_MAP) && MIR_COLLECTION_STD_MULTI_MAP
template <typename T>
static void SetCollectionLambdasMultiMap(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasMultiMap(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_multi_map<T>::value, "Type T is not of type std::multimap!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionAddress)->insert(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_MULTI_MAP) && MIR_COLLECTION_STD_MULTI_MAP

#if defined(MIR_COLLECTION_STD_MULTI_SET) && MIR_COLLECTION_STD_MULTI_SET
template <typename T>
static void SetCollectionLambdasMultiSet(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasMultiSet(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_multi_set<T>::value, "Type T is not of type std::multiset!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->insert(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_MULTI_SET) && MIR_COLLECTION_STD_MULTI_SET

#if defined(MIR_COLLECTION_STD_QUEUE) && MIR_COLLECTION_STD_QUEUE
template <typename T>
static void SetCollectionLambdasQueue(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasQueue(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_queue<T>::value, "Type T is not of type std::queue!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->push(*(typename T::value_type*)elementFirst);
	};
	DEEP_COPY_ITERATE_FUNC
		return (char*)&cache.front();
	};

}
#endif // defined(MIR_COLLECTION_STD_QUEUE) && MIR_COLLECTION_STD_QUEUE

#if defined(MIR_COLLECTION_STD_SET) && MIR_COLLECTION_STD_SET
template <typename T>
static void SetCollectionLambdasSet(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasSet(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_set<T>::value, "Type T is not of type std::set!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->insert(*(typename T::value_type*)elementFirst);
	};
	GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_SET) && MIR_COLLECTION_STD_SET

#if defined(MIR_COLLECTION_STD_STACK) && MIR_COLLECTION_STD_STACK
template <typename T>
static void SetCollectionLambdasStack(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasStack(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_stack<T>::value, "Type T is not of type std::stack!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>()); // #TODO Review collectionTypeInfos for non-pair or tuple types
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionObjAddress)->push(*(typename T::value_type*)elementFirst);
	};
	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionObjAddress, size_t aIndex) -> char* {
		/* #TODO See if copying the collection can be avoided in order to non-destructively iterate non-random access collection*/
		static std::vector<T::value_type> cacheVector;
		static T cache;
		static uint64_t cacheSize = 0;
		static const void* lastInstanceAddress = nullptr;
		static uint64_t lastIndex = 0;

		if (!collectionObjAddress || (!lastInstanceAddress && !collectionObjAddress))
		{
			return nullptr;
		}

		if (collectionObjAddress != lastInstanceAddress || aIndex < lastIndex)
		{
			// #TODO Flip ordering to display in text matching RAM ordering, so deserialize logic is similar to other collections
			cacheVector.clear();
			while (!cache.empty())
			{
				cache.pop();
			}
			cache = *(T*)collectionObjAddress;
			cacheSize = cache.size();

			cacheVector.resize(cacheSize);
			for (int i = cacheSize - 1; i >= 0; i--)
			{
				cacheVector[i] = cache.top();
				cache.pop();
			}

			lastInstanceAddress = collectionObjAddress;
			lastIndex = 0;
		}

		if (aIndex >= cacheSize)
		{
			return nullptr;
		}

		lastIndex = aIndex;
		return (char*)&cacheVector[lastIndex];
	};
}
#endif // defined(MIR_COLLECTION_STD_STACK) && MIR_COLLECTION_STD_STACK

#if defined(MIR_COLLECTION_STD_PAIR) && MIR_COLLECTION_STD_PAIR
template <typename T>
static void SetCollectionLambdasPair(Mirror::TypeInfo* mutableTypeInfo, std::false_type) { }

template <typename T>
static void SetCollectionLambdasPair(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_pair<T>::value, "Type T is not a pair!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::first_type>());
	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::second_type>());
	mutableTypeInfo->collectionAddFunc = [](void* pairObjAddress, size_t aIndex, const void* elementFirst) {
		T* pair = (T*)pairObjAddress;
		if (0 == aIndex) memcpy((void*)&pair->first, elementFirst, sizeof(T::first_type));
		else if (1 == aIndex) memcpy((void*)&pair->second, elementFirst, sizeof(T::second_type));
	};
	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* pairObjAddress, size_t aIndex) -> char* {
		T* pair = (T*)pairObjAddress;
		if (0 == aIndex) return (char*)&pair->first;
		else if (1 == aIndex) return (char*)&pair->second;
		else return nullptr;
	};
}
#endif // defined(MIR_COLLECTION_STD_PAIR) && MIR_COLLECTION_STD_PAIR

#if defined(MIR_COLLECTION_STD_PRIORITY_QUEUE) && MIR_COLLECTION_STD_PRIORITY_QUEUE
template <typename T>
static void SetCollectionLambdasPriorityQueue(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasPriorityQueue(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_priority_queue<T>::value, "Type T is not of type std::priority_queue!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionObjAddress, size_t /*aIndex*/, const void* elementFirst) {
		((T*)collectionObjAddress)->push(*(typename T::value_type*)elementFirst);
	};
	DEEP_COPY_ITERATE_FUNC
		return (char*)&cache.top();
	};
}
#endif // defined(MIR_COLLECTION_STD_PRIORITY_QUEUE) && MIR_COLLECTION_STD_PRIORITY_QUEUE

#if defined(MIR_COLLECTION_STD_TUPLE) && MIR_COLLECTION_STD_TUPLE
template <typename T>
static void SetCollectionLambdasTuple(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

// #TODO lambdas loop over args
template <typename T, std::size_t... Is>
void TupleTypeInfos(std::index_sequence<Is...>, Mirror::TypeInfo* mutableTypeInfo) {
	using Swallow = int[];
	(void)Swallow {
		0, (void( mutableTypeInfo->collectionTypeInfos.push_back( Mir::InfoForType<std::tuple_element_t<Is, T>>() ) ), 0) ...
	};
}

#include <functional>
template <typename Tuple, std::size_t... Is>
auto make_address_getters2(std::index_sequence<Is...>) {
	using getter_t = std::function<char* (Tuple&)>;
	return std::array<getter_t, sizeof...(Is)>{
		{ [](Tuple& t) -> char* {
			return reinterpret_cast<char*>(&std::get<Is>(t));
			}... }
	};
}

template <typename... Args>
char* get_tuple_member_address_by_index2(std::tuple<Args...>& t, std::size_t index) {
	static auto getters = make_address_getters2<std::tuple<Args...>>(std::index_sequence_for<Args...>{});
	if (index >= getters.size()) {
		return nullptr;
	}
	return getters[index](t);
}

template <typename Tuple, std::size_t... Is>
auto make_address_sizes(std::index_sequence<Is...>) {
	using size_func_t = std::function<size_t(Tuple&)>;
	return std::array<size_func_t, sizeof...(Is)>{
		{ [](Tuple& t) -> size_t {
			return sizeof(std::tuple_element<Is, Tuple>::type);
			}... }
	};
}

template <typename... Args>
size_t get_tuple_member_size_by_index(std::tuple<Args...>& t, std::size_t index) {
	static auto getters = make_address_sizes<std::tuple<Args...>>(std::index_sequence_for<Args...>{});
	if (index >= getters.size()) {
		return 0;
	}
	return getters[index](t);
}

template <typename T>
static void SetCollectionLambdasTuple(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_tuple<T>::value, "Type T is not a tuple!");

	constexpr std::size_t numberOfTupleElements = std::tuple_size<T>::value;
	TupleTypeInfos<T>(std::make_index_sequence<numberOfTupleElements>{}, mutableTypeInfo);

	mutableTypeInfo->collectionAddFunc = [](void* tupleObjAddress, size_t aIndex, const void* elementFirst) -> void {
		T& tuple = *((T*)tupleObjAddress);
		const int elementSize = get_tuple_member_size_by_index(tuple, aIndex);
		void* elementAddress = (void*)get_tuple_member_address_by_index2(tuple, aIndex);
		memcpy(elementAddress, elementFirst, elementSize);
	};
	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* tupleObjAddress, size_t aIndex) -> char* {
		T& tuple = *((T*)tupleObjAddress);
		if (aIndex < Mirror::InfoForType<T>()->collectionTypeInfos.size()) // #NOTE Capturing non-const mutableTypeInfo changes function pointer signature
		{
			return get_tuple_member_address_by_index2(tuple, aIndex);
		}
		return nullptr;
	};
	mutableTypeInfo->typeConstructorFunc = [](void* preallocatedMemoryAddress) { new(preallocatedMemoryAddress) T; };
}
#endif // defined(MIR_COLLECTION_STD_TUPLE) && MIR_COLLECTION_STD_TUPLE

#if defined(MIR_COLLECTION_STD_VECTOR) && MIR_COLLECTION_STD_VECTOR
template <typename T>
static void SetCollectionLambdasVector(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {}

template <typename T>
static void SetCollectionLambdasVector(Mirror::TypeInfo* mutableTypeInfo, std::true_type) {
	static_assert(is_stl_vector_impl::is_stl_vector<T>::type(), "Type T is not a vector!");

	mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<T::value_type>());
	mutableTypeInfo->collectionAddFunc = [](void* collectionAddress, size_t /*index*/, const void* elementFirst) {
		((T*)collectionAddress)->emplace_back(*(typename T::value_type*)elementFirst);
	};
	mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionAddress, size_t aIndex) -> char* {
		static size_t index = 0; // #TODO Support iterating backwards over a collection, and random access
		T* vector = ((T*)collectionAddress);
		if (aIndex < index) index = aIndex;
		if (index >= vector->size()) { index = 0; return nullptr; }
		return (char*)vector->data() + (sizeof(T::value_type) * index++);
	};
	// #TODO Pros/cons of unique versus generic macro implementation
	// GENERIC_ITERATE_LAMBDA
}
#endif // defined(MIR_COLLECTION_STD_VECTOR) && MIR_COLLECTION_STD_VECTOR

template <typename T>
static void SetCollectionLambdas(Mirror::TypeInfo* mutableTypeInfo, std::false_type) {
	if (std::is_array_v<T>)
	{
		typedef typename std::remove_all_extents<T>::type ArrayElementType;
		mutableTypeInfo->collectionTypeInfos.emplace_back(Mirror::InfoForType<ArrayElementType>());
		mutableTypeInfo->collectionAddFunc = [](void* collectionAddress, size_t index, const void* elementFirst) {
			memcpy((char*)collectionAddress + (sizeof(ArrayElementType) * index), elementFirst, sizeof(ArrayElementType));
		};
		mutableTypeInfo->collectionIterateCurrentFunc = [](const void* collectionAddress, size_t aIndex) -> char* {
			static size_t index = 0;
			if (aIndex < index) index = aIndex;
			if (index >= sizeof(T) / sizeof(ArrayElementType)) { index = 0; return nullptr; }
			return (char*)collectionAddress + (sizeof(ArrayElementType) * index++);
		};
	}
}

template <typename T>
static void SetCollectionLambdas(Mirror::TypeInfo* constTypeInfo, std::true_type) {
	static_assert(is_stl_container<T>::value, "Type T is not a collection!");
	Mirror::TypeInfo* mutableTypeInfo = const_cast<Mirror::TypeInfo*>(constTypeInfo);

#ifdef MIR_COLLECTION_STD_ARRAY
	SetCollectionLambdasArray<T>(mutableTypeInfo, is_stl_array_impl::is_stl_array<T>::type());
#endif // MIR_COLLECTION_STD_ARRAY

#ifdef MIR_COLLECTION_STD_DEQUE
	SetCollectionLambdasDeque<T>(mutableTypeInfo, is_stl_deque_impl::is_stl_deque<T>::type());
#endif // MIR_COLLECTION_STD_DEQUE

#ifdef MIR_COLLECTION_STD_FORWARD_LIST
	SetCollectionLambdasForwardList<T>(mutableTypeInfo, is_stl_forward_list_impl::is_stl_forward_list<T>::type());
#endif // MIR_COLLECTION_STD_FORWARD_LIST

#ifdef MIR_COLLECTION_STD_LIST
	SetCollectionLambdasList<T>(mutableTypeInfo, is_stl_list_impl::is_stl_list<T>::type());
#endif // MIR_COLLECTION_STD_LIST

#ifdef MIR_COLLECTION_STD_MAP
	SetCollectionLambdasMap<T>(mutableTypeInfo, is_stl_map_impl::is_stl_map<T>::type());
#endif // MIR_COLLECTION_STD_MAP

#ifdef MIR_COLLECTION_STD_MULTI_MAP
	SetCollectionLambdasMultiMap<T>(mutableTypeInfo, is_stl_multi_map_impl::is_stl_multi_map<T>::type());
#endif // MIR_COLLECTION_STD_MULTI_MAP

#ifdef MIR_COLLECTION_STD_MULTI_SET
	SetCollectionLambdasMultiSet<T>(mutableTypeInfo, is_stl_multi_set_impl::is_stl_multi_set<T>::type());
#endif // MIR_COLLECTION_STD_MULTI_SET

#ifdef MIR_COLLECTION_STD_QUEUE
	SetCollectionLambdasQueue<T>(mutableTypeInfo, is_stl_queue_impl::is_stl_queue<T>::type());
#endif // MIR_COLLECTION_STD_QUEUE

#ifdef MIR_COLLECTION_STD_SET
	SetCollectionLambdasSet<T>(mutableTypeInfo, is_stl_set_impl::is_stl_set<T>::type());
#endif // MIR_COLLECTION_STD_SET

#ifdef MIR_COLLECTION_STD_STACK
	SetCollectionLambdasStack<T>(mutableTypeInfo, is_stl_stack_impl::is_stl_stack<T>::type());
#endif // MIR_COLLECTION_STD_STACK

#ifdef MIR_COLLECTION_STD_PAIR
	SetCollectionLambdasPair<T>(mutableTypeInfo, is_stl_pair_impl::is_stl_pair<T>::type());
#endif // MIR_COLLECTION_STD_PAIR

#ifdef MIR_COLLECTION_STD_PRIORITY_QUEUE
	SetCollectionLambdasPriorityQueue<T>(mutableTypeInfo, is_stl_priority_queue_impl::is_stl_priority_queue<T>::type());
#endif // MIR_COLLECTION_STD_PRIORITY_QUEUE

#ifdef MIR_COLLECTION_STD_TUPLE
	SetCollectionLambdasTuple<T>(mutableTypeInfo, is_stl_tuple_impl::is_stl_tuple<T>::type());
#endif MIR_COLLECTION_STD_TUPLE

#ifdef MIR_COLLECTION_STD_VECTOR
	SetCollectionLambdasVector<T>(mutableTypeInfo, is_stl_vector_impl::is_stl_vector<T>::type());
#endif // MIR_COLLECTION_STD_VECTOR
}
