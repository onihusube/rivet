#pragma once

#include <version>

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges < 202202L
  #if defined(__GNUC__) && 11 <= __GNUC__
    // GCC11以降のC++20実装
    #define RIVET_GCC
  #elif defined(__GNUC__) && __GNUC__ == 10
    #define RIVET_GCC10
  #elif defined(_LIBCPP_VERSION) && !defined(_LIBCPP_HAS_NO_INCOMPLETE_RANGES)
    // clang use lic++
    #define RIVET_CLANG
  #elif defined(_MSC_VER)
    #define RIVET_MSVC
  #elif defined(__GLIBCXX__)
    // clang use libstdc++
    #if 20210408 < __GLIBCXX__
      #define RIVET_GCC
    #else
      #define RIVET_GCC10
    #endif
  #else
    #error No support compiler
  #endif
#elif defined(__clang__) && __has_include(<ranges>) && !defined(_LIBCPP_HAS_NO_INCOMPLETE_RANGES)
  // clang14以下でrange実装中（機能テストマクロがない）
  #define RIVET_CLANG
#elif defined(__cpp_lib_ranges)
  // P2387 implemented (C++23)
  #define RIVET_P2387
#else
  #error This environment does not implement <ranges>
#endif

#include <ranges>
#include <concepts>

#ifdef RIVET_P2387
  // for std::bind_back()
  #include <functional>
#endif

namespace rivet::detail {

#ifdef RIVET_P2387

  template<typename F>
  class range_closure_t : public std::ranges::range_adaptor_closure<range_closure_t<F>> {
    F m_f;
  public:
    constexpr range_closure_t(F&& f) : m_f(std::move(f)) {}

    template <std::ranges::viewable_range R>
      requires std::invocable<const F&, R>
    constexpr auto operator()(R&& r) const {
      return std::invoke(m_f, std::forward<R>(r));
    }
  };

#elif defined(RIVET_MSVC)
  #if 1930 <= _MSC_VER
    template<typename... Ts>
    using range_closure_t = std::ranges::_Range_closure<Ts...>;
  #else
    // MSVC 2019（_MSC_VER == 1929以下）では、_Range_closure型が存在していない。
    // その実装は、個別のRangeアダプタ型内部でそれぞれに行われている
    template<std::default_initializable Adaptor, typename... Ts>
    class range_closure_t : public std::ranges::_Pipe::_Base<range_closure_t<Adaptor, Ts...>> {
      std::tuple<Ts...> m_args;

      template<typename R, typename T, std::size_t... Idx>
      static constexpr decltype(auto) call(R&& range, T&& tuple, std::index_sequence<Idx...>) {
        return Adaptor{}(std::forward<R>(range), std::get<Idx>(std::forward<T>(tuple))...);
      }

      using idx_seq = std::index_sequence_for<Ts...>;

    public:

      template<typename... Args>
        requires (std::same_as<std::decay_t<Args>, Ts> && ...)
      constexpr explicit range_closure_t(Args&&... args)
        : m_args(std::forward<Args>(args)...)
      {}

        
      template<std::ranges::viewable_range R>
      constexpr decltype(auto) operator()(R&& range) & {
        return call(std::forward<R>(range), m_args, idx_seq{});
      }

      template<std::ranges::viewable_range R>
      constexpr decltype(auto) operator()(R&& range) const & {
        return call(std::forward<R>(range), m_args, idx_seq{});
      }

      template<std::ranges::viewable_range R>
      constexpr decltype(auto) operator()(R&& range) && {
        return call(std::forward<R>(range), std::move(m_args), idx_seq{});
      }

      template<std::ranges::viewable_range R>
      constexpr decltype(auto) operator()(R&& range) const && {
        return call(std::forward<R>(range), std::move(m_args), idx_seq{});
      }

    };
  #endif
#endif

#ifndef RIVET_GCC10

  template<typename Adaptor, int Arity = 2>
  struct range_adaptor_base_impl
#ifdef RIVET_GCC
    : public std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>
#endif
  {
#ifdef RIVET_GCC
    using std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>::operator();

    static constexpr int _S_arity = Arity;
    static constexpr bool _S_has_simple_call_op = true;

    template <std::ranges::viewable_range R, typename... Args>
    constexpr auto operator()(R&& r, Args&&... args) const {
      const auto& self = static_cast<const Adaptor&>(*this);
      return self(std::forward<R>(r), std::forward<Args>(args)...);
    }
#else

    template<typename... Args>
      requires (std::constructible_from<std::decay_t<Args>, Args> && ...)
    constexpr auto operator()(Args&&... args) const noexcept {
  #ifdef RIVET_P2387
      return range_closure_t{std::bind_back(*this, std::forward<Args>(args)...)};
  #elif defined(RIVET_CLANG)
      return std::__range_adaptor_closure_t(std::__bind_back(*this, std::forward<Args>(args)...));
  #elif defined(RIVET_MSVC)
      return range_closure_t<Adaptor, std::decay_t<Args>...>{std::forward<Args>(args)...};
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
#ifdef RIVET_P2387
    using type = std::ranges::range_adaptor_closure<Adaptor>;
#elif defined(RIVET_GCC)
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

#undef RIVET_GCC
#undef RIVET_GCC10
#undef RIVET_CLANG
#undef RIVET_MSVC
#undef RIVET_P2387