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

  template<typename Adaptor, int Arity = 2>
  struct range_adaptor_base_impl
#ifdef RIVET_GCC11
    : public std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>
#endif
  {
#ifdef RIVET_GCC11
    using std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>::operator();

    static constexpr int _S_arity = Arity;
    static constexpr bool _S_has_simple_call_op = true;

    template <std::ranges::viewable_range R, typename... Args>
    constexpr auto operator()(R&& r, Args&&... args) const {
      const auto& self = static_cast<const Adaptor&>(*this);
      return self(std::forward<R>(r), std::forward<Args>(args)...);
    }
#else

    template<typename Arg>
      requires std::constructible_from<std::decay_t<Arg>, Arg>
    constexpr auto operator()(Arg&& arg) const noexcept {
  #if defined(RIVET_CLANG)
      return std::__range_adaptor_closure_t(std::__bind_back(*this, std::forward<Arg>(arg)));
  #elif defined(RIVET_MSVC)
      return std::ranges::_Range_closure<Adaptor, std::decay_t<Arg>>{std::forward<Arg>(arg)};
  #endif
    }
#endif
  };

#else

  template<typename Adaptor, auto...>
  struct range_adaptor_base_impl {

    template <typename... Args>
    constexpr auto operator()(Args&&... args) const {
      if constexpr (std::default_initializable<Adaptor>) {
        auto closure = [... args(std::forward<Args>(args))]<typename R>(R &&r) {
          return Adaptor{}(std::forward<R>(r), args...);
        };

        return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
      } else {
        static_assert([]{return false;}(), "Adaptor must be default_initializable.");
      }
    }
  };

  template <typename Adaptor>
  struct range_adaptor_closure_base {

    // 1. range | RACO -> view
    template <std::ranges::viewable_range R>
    [[nodiscard]]
    friend constexpr auto operator|(R &&r, const Adaptor& self) {
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


  template<typename Adaptor, auto...>
  struct dispatcher {
#ifdef RIVET_GCC11
    using type = std::views::__adaptor::_RangeAdaptorClosure;
#elif defined(RIVET_GCC10)
    using type = range_adaptor_closure_base<Adaptor>;
#elif defined(RIVET_CLANG)
    using type = std::__range_adaptor_closure<Adaptor>;
#elif defined(RIVET_MSVC)
    using type = std::ranges::_Pipe::_Base<Adaptor>;
#endif
  };

  template<typename Adaptor, int Arity>
  struct dispatcher<Adaptor, false, Arity> {
    using type = range_adaptor_base_impl<Adaptor, Arity>;
  };
}

namespace rivet {

  template <typename Adaptor, bool IsClosure = false, int Arity = 2>
  using range_adaptor_base = typename detail::dispatcher<std::decay_t<Adaptor>, IsClosure, Arity>::type;
}

#define RIVET_ENABLE_ADAPTOR(this_type) using rivet::range_adaptor_base<this_type>::operator()

#undef RIVET_GCC11
#undef RIVET_GCC10
#undef RIVET_CLANG
#undef RIVET_MSVC
#undef RIVET_P2387