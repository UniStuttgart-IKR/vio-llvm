//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___CXX03___ALGORITHM_RANGES_MAX_H
#define _LIBCPP___CXX03___ALGORITHM_RANGES_MAX_H

#include <__cxx03/__algorithm/ranges_min_element.h>
#include <__cxx03/__assert>
#include <__cxx03/__concepts/copyable.h>
#include <__cxx03/__config>
#include <__cxx03/__functional/identity.h>
#include <__cxx03/__functional/invoke.h>
#include <__cxx03/__functional/ranges_operations.h>
#include <__cxx03/__iterator/concepts.h>
#include <__cxx03/__iterator/projected.h>
#include <__cxx03/__ranges/access.h>
#include <__cxx03/__ranges/concepts.h>
#include <__cxx03/__type_traits/is_trivially_copyable.h>
#include <__cxx03/__utility/move.h>
#include <__cxx03/initializer_list>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

#if _LIBCPP_STD_VER >= 20

_LIBCPP_PUSH_MACROS
#  include <__cxx03/__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

namespace ranges {
namespace __max {
struct __fn {
  template <class _Tp,
            class _Proj                                                    = identity,
            indirect_strict_weak_order<projected<const _Tp*, _Proj>> _Comp = ranges::less>
  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr const _Tp&
  operator()(_LIBCPP_LIFETIMEBOUND const _Tp& __a,
             _LIBCPP_LIFETIMEBOUND const _Tp& __b,
             _Comp __comp = {},
             _Proj __proj = {}) const {
    return std::invoke(__comp, std::invoke(__proj, __a), std::invoke(__proj, __b)) ? __b : __a;
  }

  template <copyable _Tp,
            class _Proj                                                    = identity,
            indirect_strict_weak_order<projected<const _Tp*, _Proj>> _Comp = ranges::less>
  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr _Tp
  operator()(initializer_list<_Tp> __il, _Comp __comp = {}, _Proj __proj = {}) const {
    _LIBCPP_ASSERT_VALID_ELEMENT_ACCESS(
        __il.begin() != __il.end(), "initializer_list must contain at least one element");

    auto __comp_lhs_rhs_swapped = [&](auto&& __lhs, auto&& __rhs) -> bool { return std::invoke(__comp, __rhs, __lhs); };
    return *ranges::__min_element_impl(__il.begin(), __il.end(), __comp_lhs_rhs_swapped, __proj);
  }

  template <input_range _Rp,
            class _Proj                                                         = identity,
            indirect_strict_weak_order<projected<iterator_t<_Rp>, _Proj>> _Comp = ranges::less>
    requires indirectly_copyable_storable<iterator_t<_Rp>, range_value_t<_Rp>*>
  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr range_value_t<_Rp>
  operator()(_Rp&& __r, _Comp __comp = {}, _Proj __proj = {}) const {
    auto __first = ranges::begin(__r);
    auto __last  = ranges::end(__r);

    _LIBCPP_ASSERT_VALID_ELEMENT_ACCESS(__first != __last, "range must contain at least one element");

    if constexpr (forward_range<_Rp> && !__is_cheap_to_copy<range_value_t<_Rp>>) {
      auto __comp_lhs_rhs_swapped = [&](auto&& __lhs, auto&& __rhs) -> bool {
        return std::invoke(__comp, __rhs, __lhs);
      };
      return *ranges::__min_element_impl(std::move(__first), std::move(__last), __comp_lhs_rhs_swapped, __proj);
    } else {
      range_value_t<_Rp> __result = *__first;
      while (++__first != __last) {
        if (std::invoke(__comp, std::invoke(__proj, __result), std::invoke(__proj, *__first)))
          __result = *__first;
      }
      return __result;
    }
  }
};
} // namespace __max

inline namespace __cpo {
inline constexpr auto max = __max::__fn{};
} // namespace __cpo
} // namespace ranges

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP_STD_VER >= 20

#endif // _LIBCPP___CXX03___ALGORITHM_RANGES_MAX_H
