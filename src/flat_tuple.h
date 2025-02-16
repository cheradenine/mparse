#ifndef __FLAT_TUPLE_H__
#define __FLAT_TUPLE_H__

#include <tuple>
#include <type_traits>

#include <variant>

using unit = std::monostate;

// Helper to detect tuple types
template<typename T>
struct is_tuple : std::false_type {};

template<typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

// Helper to check if type is unit
template<typename T>
struct is_unit : std::is_same<std::remove_cvref_t<T>, unit> {};

// Helper to filter unit types from tuple
template<typename Tuple>
struct filter_unit;

template<typename... Ts>
struct filter_unit<std::tuple<Ts...>> {
    template<typename T>
    using keep_if_not_unit = std::conditional_t<!is_unit<T>::value, 
                                               std::tuple<T>, 
                                               std::tuple<>>;
    
    using type = decltype(std::tuple_cat(
        std::declval<keep_if_not_unit<Ts>>()...
    ));
};

// Helper to make tuple without unit
template<typename... Ts>
auto make_tuple_no_unit() {
    return typename filter_unit<std::tuple<Ts...>>::type{};
}

template<typename T>
auto make_single_if_not_unit(T&& value) {
    if constexpr (is_unit<T>::value) {
        return std::tuple<>();
    } else {
        return std::make_tuple(std::forward<T>(value));
    }
}

// Primary template for when neither A nor B is a tuple
template<typename A, typename B>
auto make_flat_tuple(A&& a, B&& b) -> 
    std::enable_if_t<!is_tuple<std::remove_reference_t<A>>::value && 
                     !is_tuple<std::remove_reference_t<B>>::value,
    typename filter_unit<std::tuple<std::decay_t<A>, std::decay_t<B>>>::type> {
    return std::tuple_cat(
        make_single_if_not_unit(std::forward<A>(a)),
        make_single_if_not_unit(std::forward<B>(b))
    );
}

// Specialization for when A is a tuple and B is not
template<typename... As, typename B>
auto make_flat_tuple(const std::tuple<As...>& a, B&& b) {
    return std::tuple_cat(
        typename filter_unit<std::tuple<As...>>::type(a),
        make_single_if_not_unit(std::forward<B>(b))
    );
}

// Specialization for when B is a tuple and A is not
template<typename A, typename... Bs>
auto make_flat_tuple(A&& a, const std::tuple<Bs...>& b) {
    return std::tuple_cat(
        make_single_if_not_unit(std::forward<A>(a)),
        typename filter_unit<std::tuple<Bs...>>::type(b)
    );
}

// Specialization for when both A and B are tuples
template<typename... As, typename... Bs>
auto make_flat_tuple(const std::tuple<As...>& a, const std::tuple<Bs...>& b) {
    return std::tuple_cat(
        typename filter_unit<std::tuple<As...>>::type(a),
        typename filter_unit<std::tuple<Bs...>>::type(b)
    );
}

std::string_view make_flat_tuple(std::string_view a, std::string_view b) {
    return std::string_view(a.data(), a.size() + b.size());
}

#endif  // __FLAT_TUPLE_H__