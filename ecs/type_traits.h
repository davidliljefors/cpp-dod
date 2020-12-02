#pragma once
#include <type_traits>


namespace ttraits {

	template<typename First, typename...Rest>
	struct next_variadic_type
	{
		using type = First;
		using next = next_variadic_type<Rest...>;
	};

	template<typename First>
	struct next_variadic_type<First>
	{
		using type = First;
	};

	template<typename... Ts>
	using next_variadic_type_t = typename next_variadic_type<Ts...>::type;

	template<typename... Ts>
	using next_variadic_type2_t = typename next_variadic_type<Ts...>::next::type;

	template<size_t index, typename This, typename... Ts>
	struct variadic_type
	{
		using type = typename variadic_type<index - 1, Ts...>::type;
	};

	template<typename This, typename... Rest>
	struct variadic_type<0, This, Rest...>
	{
		using type = This;
	};

	template<typename This>
	struct variadic_type<0, This>
	{
		using type = This;
	};

	template<size_t index, typename... Ts>
	using variadic_type_t = typename variadic_type<index, Ts...>::type;

}