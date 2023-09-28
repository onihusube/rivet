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
#include <functional>

namespace rivet::detail {

#ifdef RIVET_GCC10

  // range_adaptor_closure_baseを継承している型のオブジェクトが`|`の両辺にくるのを検出する
  struct detect_special_ambiguous_case {};

  template <typename Adaptor>
  struct range_adaptor_closure_base : public detect_special_ambiguous_case {

    // 1. range | RACO -> view
    template <std::ranges::viewable_range R>
      requires requires(R&& in, const Adaptor& raco) {
        { raco(std::forward<R>(in)) } -> std::ranges::view;
      }
    [[nodiscard]]
    friend constexpr auto operator|(R &&r, const Adaptor& self) noexcept(std::is_nothrow_invocable_v<const Adaptor&, R>) {
      return self(std::forward<R>(r));
    }

    // 2. RACO | RACO (self) -> RACO
    // RACO | self みたいな場合に必要
    template<typename LHS>
      requires (not std::ranges::range<LHS>) and  // 1と曖昧になるのを回避
               (not std::derived_from<LHS, detect_special_ambiguous_case>)  // 3と曖昧になるのを回避（この型を派生した型が両辺に来た時）
    [[nodiscard]]
    friend constexpr auto operator|(LHS lhs, Adaptor self) noexcept(std::is_nothrow_move_constructible_v<LHS> and std::is_nothrow_move_constructible_v<Adaptor>) {
      auto closure = [l = std::move(lhs), r = std::move(self)]<typename R>(R &&range) {
        return std::forward<R>(range) | l | r;
      };

      return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
    }

    // 3. RACO (self) | RACO -> RACO
    // self | RACO みたいな場合に必要
    // 両辺をテンプレートにすると、このクラスを継承したクラスが2つ以上あるときに多重定義エラーになる
    template<typename RHS>
    [[nodiscard]]
    friend constexpr auto operator|(Adaptor self, RHS rhs) noexcept(std::is_nothrow_move_constructible_v<Adaptor> and std::is_nothrow_move_constructible_v<RHS>) {
      auto closure = [l = std::move(self), r = std::move(rhs)]<typename R>(R &&range) {
        return std::forward<R>(range) | l | r;
      };

      return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
    }
  };
#endif

  // レンジアダプタクロージャのディスパッチ
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

#ifdef __cpp_lib_bind_back
  using std::bind_back;
#else

  template<typename F, typename... BackArgs>
  class bind_partial {
    std::decay_t<F> m_f;
    std::tuple<std::decay_t<BackArgs>...> m_back_args;

    template<typename Fn, typename T, std::size_t... Idx, typename... FrontArgs>
    static constexpr auto call(Fn&& f, T&& tuple, std::index_sequence<Idx...>, FrontArgs&&... args) {
      return std::invoke(std::forward<Fn>(f), std::forward<FrontArgs>(args)..., std::get<Idx>(std::forward<T>(tuple))...);
    }

    using idx_seq = std::index_sequence_for<BackArgs...>;

  public:

    template<typename Fn = F, typename... Args>
    constexpr bind_partial(Fn&& f, Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, Fn> and std::is_nothrow_constructible_v<std::tuple<std::decay_t<BackArgs>...>, Args...>)
      : m_f(std::forward<Fn>(f))
      , m_back_args(std::forward<Args>(args)...)
    {}

    template<typename... FrontArgs>
    constexpr auto operator()(FrontArgs&&... front_args) & noexcept(std::is_nothrow_invocable_v<F&, FrontArgs..., BackArgs&...>) {
      return call(m_f, m_back_args, idx_seq{}, std::forward<FrontArgs>(front_args)...);
    }

    template<typename... FrontArgs>
    constexpr auto operator()(FrontArgs&&... front_args) const & noexcept(std::is_nothrow_invocable_v<const F&, FrontArgs..., const BackArgs&...>) {
      return call(m_f, m_back_args, idx_seq{}, std::forward<FrontArgs>(front_args)...);
    }

    template<typename... FrontArgs>
    constexpr auto operator()(FrontArgs&&... front_args) && noexcept(std::is_nothrow_invocable_v<F&&, FrontArgs..., BackArgs&&...>) {
      return call(std::move(m_f), std::move(m_back_args), idx_seq{}, std::forward<FrontArgs>(front_args)...);
    }

    template<typename... FrontArgs>
    constexpr auto operator()(FrontArgs&&... front_args) const && noexcept(std::is_nothrow_invocable_v<const F&&, FrontArgs..., const BackArgs&&...>) {
      return call(std::move(m_f), std::move(m_back_args), idx_seq{}, std::forward<FrontArgs>(front_args)...);
    }
  };

  template<typename F, typename... Args>
  constexpr auto bind_back(F&& f, Args&&... args) noexcept(std::is_nothrow_constructible_v<bind_partial<F, Args...>, F, Args...>) {
    return bind_partial<F, Args...>{std::forward<F>(f), std::forward<Args>(args)...};
  }
#endif
}

namespace rivet {

  template <typename F>
    requires (not std::is_reference_v<F>) // 常にコピーないしムーブして保持する（確認用）
  struct closure : F, detail::dispatcher<closure<F>>::type {

    // ここは右辺値受けのムーブで十分
    // bind_back()にせよラムダにせよ、右辺値で渡ってくる用法しか考慮しない
    constexpr closure(F&& f) noexcept(std::is_nothrow_move_constructible_v<F>) : F(std::move(f)) {}
  };
}

namespace rivet::detail {

  template<typename Adaptor, int Arity = 2>
  struct range_adaptor_base_impl
#ifdef RIVET_GCC
    : public std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>
#endif
  {
    // 派生クラスでoperator()をusingする際に派生クラス型そのものを取得するのに使用する
    using get_derived_type_of_range_adaptor_base_t = Adaptor;

#ifdef RIVET_GCC
    using std::views::__adaptor::_RangeAdaptor<range_adaptor_base_impl<Adaptor>>::operator();

    static constexpr int _S_arity = Arity;
    static constexpr bool _S_has_simple_call_op = true;

    // 基底クラス_RangeAdaptor<...>には RA(Args...) -> RACO を行うoperator()が定義されている
    // ただし、派生クラスAdaptorとして呼び出しを行わないため、Adaptorに定義されているoperator()（viewを生成する）を呼び出せない
    // このクラスとして呼び出しを行おうとはしているため、ここでAdaptorに定義されているoperator()へのルーティングを行う
    template <std::ranges::viewable_range R, typename... Args>
    constexpr auto operator()(R&& r, Args&&... args) const noexcept(std::is_nothrow_invocable_v<const Adaptor&, R, Args...>) {
      // 入力rangeと追加引数によって対象のviewを生成する、Adaptorに定義されているoperator()を呼び出す
      const auto& self = static_cast<const Adaptor&>(*this);
      return self(std::forward<R>(r), std::forward<Args>(args)...);
    }
#elif defined(RIVET_GCC10)

    template <typename... Args>
    constexpr auto operator()(Args&&... args) const noexcept((std::is_nothrow_constructible_v<Args, Args> && ...) and 
                                                             (std::is_nothrow_move_constructible_v<Args> && ...))
    {
      // Adaptorにデフォルト構築可能性を要求する
      static_assert(std::default_initializable<Adaptor>, "Adaptor must be default_initializable.");

      auto closure = [... args(std::forward<Args>(args))]<typename R>(R &&r) {
        return Adaptor{}(std::forward<R>(r), args...);
      };

      return std::views::__adaptor::_RangeAdaptorClosure<decltype(closure)>(std::move(closure));
    }
#else

    // RA(Args...) -> RACO を行うoperator()
    template<typename... Args>
      requires (std::constructible_from<std::decay_t<Args>, Args> && ...)
    constexpr auto operator()(Args&&... args) const noexcept(noexcept(
      // いずれの場合も、Adopterにダウンキャストすることで
      // 入力rangeと追加引数によって対象のviewを生成する、Adaptorに定義されているoperator()を呼び出す
  #ifdef RIVET_P2387
             ::rivet::closure{std::bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...)} )) {
      return ::rivet::closure{std::bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...)};
  #elif defined(RIVET_CLANG)
             std::__range_adaptor_closure_t(std::__bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...)) )) {
      return std::__range_adaptor_closure_t(std::__bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...));
  #elif defined(RIVET_MSVC)
    #if 1930 <= _MSC_VER
      // この場合、Adaptorにデフォルト構築可能性を要求する
             std::ranges::_Range_closure<Adaptor, std::decay_t<Args>...>{std::forward<Args>(args)...} )) {
      return std::ranges::_Range_closure<Adaptor, std::decay_t<Args>...>{std::forward<Args>(args)...};
    #else
      // MSVC 2019（_MSC_VER == 1929以下）では、_Range_closure型が存在していない。
      // その実装は、個別のRangeアダプタ型内部でそれぞれに行われている
             ::rivet::closure{detail::bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...)} )) {
      return ::rivet::closure{detail::bind_back(static_cast<const Adaptor&>(*this), std::forward<Args>(args)...)};
    #endif
  #endif
    }
#endif
  };

  // Rangeアダプタのディスパッチ
  template<typename Adaptor, int Arity>
  struct dispatcher<Adaptor, false, Arity> {
    using type = range_adaptor_base_impl<Adaptor, Arity>;
  };
}

namespace rivet {

  template <typename Adaptor, bool IsClosure = false, int Arity = 2>
  using range_adaptor_base = typename detail::dispatcher<std::decay_t<Adaptor>, IsClosure, Arity>::type;

  template <typename Adaptor>
  using range_adaptor_closure_base = range_adaptor_base<Adaptor, true>;
}

// range_adaptor_base継承時、range_adaptor_baseのoperator()をusingする
#define RIVET_ENABLE_ADAPTOR(this_type) using rivet::range_adaptor_base<this_type>::operator()
#define RIVET_USING_BASEOP using rivet::range_adaptor_base<get_derived_type_of_range_adaptor_base_t>::operator()

namespace rivet {

  template<typename F>
    requires (not std::is_reference_v<F>) // 常にコピーないしムーブして保持する（確認用）
  class adaptor {
    F m_f;

  public:

    // ここは右辺値受けのムーブで十分、くるのはラムダを直接のはず
    constexpr adaptor(F&& f) noexcept(std::is_nothrow_move_constructible_v<F>) : m_f(std::move(f)) {}

    // 呼び出しも完全転送を考慮しない
    // この型のオブジェクトはレンジアダプタオブジェクトとして扱われるが、ほぼRACOを生成するためにしか使われないはず

    // RA(r, args...) -> view
    template<std::ranges::viewable_range R, typename... Args>
      requires std::invocable<const F&, R, Args...>
    constexpr auto operator()(R&& r, Args&&... args) const noexcept(std::is_nothrow_invocable_v<const F&, R, Args...>) {
      return m_f(std::forward<R>(r), std::forward<Args>(args)...);
    }

    // RA(args...) -> RACO
    template<typename... Args>
    constexpr auto operator()(Args&&... args) const noexcept(noexcept(
             ::rivet::closure{detail::bind_back(m_f, std::forward<Args>(args)...)} )) {
      return ::rivet::closure{detail::bind_back(m_f, std::forward<Args>(args)...)};
    }
  };
}

#undef RIVET_GCC
#undef RIVET_GCC10
#undef RIVET_CLANG
#undef RIVET_MSVC
#undef RIVET_P2387