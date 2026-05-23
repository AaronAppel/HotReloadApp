#pragma once

// See README.md for documentation

// Original inspiration for Mirror : https://github.com/danbar0/StaticJsonReflection/tree/master

#include "MIR_Structs.h"
#include "MIR_TypeDeduction.h"
#include "MIR_Ids.h"
#include "MIR_StlCollections.h"

// #TODO Update argument naming

template <typename T>
constexpr Mirror::TypeInfoCategories GetCategory() {

	static_assert(!std::is_reference_v<T>, "Reference(s) currently unsupported");
	static_assert(!std::is_function_v<T> || !std::is_function_v<std::remove_pointer_t<T>>,  "Function object(s) and function pointer(s) currently unsupported");

	if (std::is_enum_v<T>) return Mirror::TypeInfoCategory_Primitive; // #NOTE Should come before class in case of enum class

	// #TODO Review if (std::is_array_v<T>) return Mirror::TypeInfoCategory_Collection;
	if (std::is_array_v<T>) return Mirror::TypeInfoCategory_Collection;
	if (std::is_pointer_v<T>) return Mirror::TypeInfoCategory_Pointer;

	if (is_stl_container<T>::value) return Mirror::TypeInfoCategory_Collection;
	if (std::is_class_v<T>) return Mirror::TypeInfoCategory_Class; // #NOTE Should come after container/collection check as stl containers are classes

	MIR_ASSERT(false && "Unsupported type found!");

	return Mirror::TypeInfoCategory_Primitive;
}

template <class TYPE>
static const Mirror::TypeInfo* Mirror::InfoForType(TYPE& typeObj) {
	return Mirror::InfoForType<TYPE>();
}

// #NOTE Required to avoid sizeof(void) MSVC C2070 compiler error
#define MIR_TYPE_NON_VOID(TYPE)																												\
static_assert(sizeof(TYPE_WRAP(TYPE)) <= MIR_TYPE_SIZE_MAX, "Size is larger than member can hold!");										\
localStaticTypeInfo.size = sizeof(TYPE_WRAP(TYPE));

// #NOTE Must be a macro to avoid default specialization issues causing "multiply defined..." errors dependent on order of compilation.
#define MIR_TYPE_COMMON(TYPE)																												\
template <>																																	\
static const Mirror::TypeInfo* Mirror::InfoForType<TYPE_WRAP(TYPE)>() {																		\
	static Mirror::TypeInfo localStaticTypeInfo;																							\
																																			\
	/* Cannot use .name if removed from release builds: if (!localStaticTypeInfo.stringName.empty()) { return &localStaticTypeInfo; } */	\
	/* Cannot use .size if null type: if (!localStaticTypeInfo.stringName.empty()) { return &localStaticTypeInfo; }	*/						\
	if (localStaticTypeInfo.id != MIR_NULL_ID) { return &localStaticTypeInfo; }																\
																																			\
	localStaticTypeInfo.category = GetCategory<TYPE_WRAP(TYPE)>();																			\
	localStaticTypeInfo.stringName = TYPE_WRAP_STRING(TYPE);																				\
	localStaticTypeInfo.id = Mirror::IdForType<TYPE_WRAP(TYPE)>();																			\

// #NOTE Using __VA_ARGS__ to handle macro calls with comma(s) ',' like MIR_INFO_FOR_TYPE(std::map<int, bool>)
// #NOTE Below switch fallthrough compiler warning 26819 cannot be handled within this macro
#define MIR_TYPE(...) MIR_TYPE_IMPL((__VA_ARGS__))
#define MIR_TYPE_IMPL(TYPE)																													\
MIR_TYPE_COMMON(TYPE)																														\
MIR_TYPE_NON_VOID(TYPE)																														\
																																			\
	switch (localStaticTypeInfo.category)																									\
	{																																		\
	case TypeInfoCategory_Collection: /* #NOTE Intentional case fall through as a collection is also a class */								\
		SetCollectionLambdas<TYPE_WRAP(TYPE)>(&localStaticTypeInfo, is_stl_container_impl::is_stl_container<TYPE_WRAP(TYPE)>::type());		\
		/*[[fallthrough]]*/																													\
	case TypeInfoCategory_Class:																											\
		SetConstructionLambda<TYPE_WRAP(TYPE)>(&localStaticTypeInfo, std::is_class<TYPE_WRAP(TYPE)>::type());								\
		break;																																\
																																			\
	case TypeInfoCategory_Pointer:																											\
		localStaticTypeInfo.pointerDereferencedTypeInfo = Mirror::InfoForType<std::remove_pointer_t<TYPE_WRAP(TYPE)>>();					\
		break;																																\
																																			\
	case TypeInfoCategory_Primitive:																										\
		SetConstructionLambda<TYPE_WRAP(TYPE)>(&localStaticTypeInfo, std::is_same<TYPE_WRAP(TYPE), std::string>::type());					\
		break;																																\
	}																																		\
																																			\
	return &localStaticTypeInfo;																											\
}

// #NOTE Avoids sizeof(void) C2070 compile error
#define MIR_TYPE_VOID(...) MIR_TYPE_VOID_IMPL((__VA_ARGS__))
#define MIR_TYPE_VOID_IMPL(TYPE)																											\
MIR_TYPE_COMMON(TYPE)																														\
	return &localStaticTypeInfo;																											\
}

#ifndef MIR_MEMBER_FIELDS_COUNT_DEFAULT
#define MIR_MEMBER_FIELDS_COUNT_DEFAULT 3
#endif

#define MIR_CLASS(...) MIR_CLASS_IMPL((__VA_ARGS__))
#define MIR_CLASS_IMPL(TYPE)																												\
MIR_TYPE_COMMON(TYPE)																														\
MIR_TYPE_NON_VOID(TYPE)																														\
																																			\
    if (localStaticTypeInfo.fields.size() > 0) { return &localStaticTypeInfo; }																\
	const int fieldsCount = MIR_MEMBER_FIELDS_COUNT_DEFAULT;																				\
	localStaticTypeInfo.fields.reserve(fieldsCount);																						\
																																			\
	using ClassType = TYPE_WRAP(TYPE);																										\
	enum { BASE = __COUNTER__ };

#ifndef MIR_OMIT_FLAGS
#define MIR_CLASS_MEMBER_FLAGS(MEMBER_NAME, FLAGS) MIR_CLASS_MEMBER_IMPL(MEMBER_NAME, FLAGS)
#define MIR_CLASS_MEMBER_FLAGS_IMPL(MEMBER_NAME, FLAGS)																						\
	localStaticTypeInfo.fields[MEMBER_NAME##Index].flags = FLAGS;
#else
#define MIR_CLASS_MEMBER_FLAGS_IMPL(MEMBER_NAME, FLAGS)
#endif // !MIR_OMIT_FLAGS

// #TODO Consider user exposed macros, etc, to have shorter or nicer (MirClass(), MirType(), MirId(), etc) names?
#define MIR_CLASS_MEMBER(MEMBER_NAME)  MIR_CLASS_MEMBER_IMPL(MEMBER_NAME, 0)
#define MIR_CLASS_MEMBER_IMPL(MEMBER_NAME, FLAGS)																							\
	enum { MEMBER_NAME##Index = __COUNTER__ - BASE - 1 };																					\
	Mirror::Field MEMBER_NAME##field;																										\
	localStaticTypeInfo.fields.push_back(MEMBER_NAME##field);																				\
	localStaticTypeInfo.fields[MEMBER_NAME##Index].typeInfo = Mirror::InfoForType<decltype(ClassType::MEMBER_NAME)>();						\
	localStaticTypeInfo.fields[MEMBER_NAME##Index].name = #MEMBER_NAME;																		\
	localStaticTypeInfo.fields[MEMBER_NAME##Index].offset = offsetof(ClassType, MEMBER_NAME);												\
	MIR_CLASS_MEMBER_FLAGS_IMPL(MEMBER_NAME, FLAGS)

// #TODO Review. Does construction ordering/priority (serialize, construct, re-serialize?) need to be an option exposed to users?
#define MIR_CONSTRUCT_USING_MEMBER(MEMBER_NAME)																								\
	localStaticTypeInfo.typeConstructorFunc = [](void* instanceAddress) {																	\
		using MemberType = decltype(ClassType::MEMBER_NAME);																				\
		char* memberAddress = (char*)instanceAddress + offsetof(ClassType, MEMBER_NAME);													\
		new(instanceAddress) ClassType(*(MemberType*)memberAddress);																		\
	};

#define MIR_DERIVED_CLASSES(x)	/* #TODO Finish implementing from QwerkE */																	\
	using DerivedTypes = TypesList<x>;																										\
																																			\
	template <typename SuperClass, typename... SubClass>																					\
	static void MirrorSubClass(Mirror::TypeInfo& localStaticTypeInfo)																		\
	{																																		\
		([&]()																																\
			{																																\
				static_assert(std::is_base_of<SuperClass, SubClass>::value, "SubClass class must derive from SuperClass.");					\
				const Mirror::TypeInfo* subclassTypeInfo = Mirror::InfoForType<SubClass>();													\
				localStaticTypeInfo.derivedTypes.push_back(subclassTypeInfo);																\
				const_cast<Mirror::TypeInfo*>(subclassTypeInfo)->superTypeInfo = &localStaticTypeInfo;										\
				const_cast<Mirror::TypeInfo*>(subclassTypeInfo)->typeDynamicCastFunc =														\
					[](const void* pointerToInstance) -> bool {																				\
					SubClass* subClass = (SubClass*)pointerToInstance;																		\
					return dynamic_cast<SubClass*>(*(SuperClass**)pointerToInstance) != nullptr;											\
					};																														\
			}(), ...);																														\
	}																																		\
																																			\
	template<typename SuperClass, typename... SubClass>																						\
	static void MirrorSubClasses(TemplateArgumentList<SubClass...>, Mirror::TypeInfo& localStaticTypeInfo)									\
	{																																		\
		MirrorSubClass<SuperClass, SubClass...>(localStaticTypeInfo);																		\
	}																																		\
																																			\
	MirrorSubClasses<ClassType>(DerivedTypes{}, localStaticTypeInfo);

// #TODO Check for duplicate(s) derived type pointers
// #TODO Check if immediate child or derived further in inheritance structure/tree
#define MIR_BASE_CLASS(BASE_TYPE)																											\
	static_assert(std::is_base_of<BASE_TYPE, ClassType>::value, "Child class type must derive from base type <" #BASE_TYPE ">.");			\
	const Mirror::TypeInfo* BASE_TYPE##Info = Mirror::InfoForType<BASE_TYPE>();																\
	const_cast<Mirror::TypeInfo*>(BASE_TYPE##Info)->derivedTypes.push_back(&localStaticTypeInfo);											\
	localStaticTypeInfo.superTypeInfo = BASE_TYPE##Info;

#define MIR_CLASS_END																														\
	/* #TODO Sanity checks: if (collection) { assert(collectionTypeInfoFirst) }, etc  */													\
	return &localStaticTypeInfo;																											\
}
