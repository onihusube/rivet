#pragma once

#include <version>

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges < 202202L
  #if defined(__GNUC__) && __GNUC__ == 11
    #define RIVET_GCC11
  #elif defined(__GNUC__) && __GNUC__ == 10
    #define RIVET_GCC10
  #elif defined(__clang__)
    #define RIVET_CLANG
  #elif defined(_MSC_VER)
    #define RIVET_MSVC
  #else
    #error No support compiler
  #endif
#elif defined(__cpp_lib_ranges)
  #define RIVET_P2387
#else
  #error This environment does not implement <ranges>
#endif

#include <ranges>
#include <concepts>

namespace rivet::detail {

#ifndef RIVET_GCC10

  template<typename Adoptor, int Arity = 2>
  struct range_adaptor_base_impl
#ifdef RIVET_GCC11
    : public std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adoptor>>
#endif
  {
#ifdef RIVET_GCC11
    using std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adoptor>>::operator();

    static constexpr int _S_arity = Arity;
    static constexpr bool _S_has_simple_call_op = true;

    template <std::ranges::viewable_range R, typename... Args>
    constexpr auto operator()(R&& r, Args&&... args) const {
      const auto& self = static_cast<const Adoptor&>(*this);
      return self(std::forward<R>(r), std::forward<Args>(args)...);
    }
#else

    template<typename Args>
      requires std::constructible_from<std::decay_t<Args>, Args>
    constexpr auto operator()(Args&& args) const noexcept {
  #if defined(RIVET_CLANG)
      return std::__range_adaptor_closure_t(std::__bind_back(*this, std::forward<Arg>(arg)));
  #elif defined(RIVET_MSVC)
      return std::ranges::_Range_closure<range_adaptor_base_impl<Adoptor, false>, std::decay_t<Arg>>{std::forward<Arg>(arg)};
  #endif
    }
#endif
  };

#else

  template<typename Adoptor, auto...>
  struct range_adaptor_base_impl {

    template <typename... Args>
    constexpr auto operator()(Args&&... args) const {
      if constexpr (std::default_initializable<Adoptor>) {
        auto closure = [... args(std::forward<Args>(args))]<typename R>(R &&r) {
          return Adoptor{}(std::forward<R>(r), args...);
        };

        return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
      } else {
        static_assert([]{return false;}(), "Adoptor must be default_initializable.");
      }
    }
  };

  template <typename Adoptor>
  struct range_adoptor_closure_base {

    // 1. range | RACO -> view
    template <std::ranges::viewable_range R>
    [[nodiscard]]
    friend constexpr auto operator|(R &&r, const Adoptor& self) {
      return self(std::forward<R>(r));
    }

    // 2. RACO | RACO -> RACO
    template<typename LHS, typename RHS>
    [[nodiscard]]
    friend constexpr auto operator|(LHS lhs, RHS rhs) {
      auto closure = [l = std::move(lhs), r = std::move(rhs)]<typename R>(R &&range) {
        return std::forward<R>(range) | l | r;
      };

      return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
    }
  };

#endif


  template<typename Adoptor, auto...>
  struct dispatcher {
#ifdef RIVET_GCC11
    using type = std::views::__adaptor::_RangeAdaptorClosure;
#elif defined(RIVET_GCC10)
    using type = range_adoptor_closure_base<Adoptor>;
#elif defined(RIVET_CLANG)
    using type = std::__range_adaptor_closure<Adoptor>;
#elif defined(RIVET_MSVC)
    using type = std::ranges::_Pipe::_Base<Adoptor>;
#endif
  };

  template<typename Adoptor, int Arity>
  struct dispatcher<Adoptor, false, Arity> {
    using type = range_adaptor_base_impl<Adoptor, Arity>;
  };
}

namespace rivet {

  template <typename Adoptor, bool IsClosure = false, int Arity = 2>
  using range_adaptor_base = typename detail::dispatcher<std::decay_t<Adoptor>, IsClosure, Arity>::type;
}

#define RIVET_ENABLE_ADOPTOR(this_type) using rivet::range_adaptor_base<this_type>::operator()

#undef RIVET_GCC11
#undef RIVET_GCC10
#undef RIVET_CLANG
#undef RIVET_MSVC
#undef RIVET_P2387