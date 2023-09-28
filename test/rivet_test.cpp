#include "rivet.hpp"

#include <algorithm>
#include <string_view>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

namespace ut = boost::ut;
using namespace boost::ut::literals;
using namespace boost::ut::operators::terse;

namespace myranges::views {
  namespace detail {
    
    struct filter_adoptor : public rivet::range_adaptor_base<filter_adoptor> {

      template<std::ranges::viewable_range R, typename Pred>
      constexpr auto operator()(R&& r, Pred&& p) const {
        return std::ranges::filter_view(std::forward<R>(r), std::forward<Pred>(p));
      }

      //using rivet::range_adaptor_base<filter_adoptor>::operator();
      RIVET_USING_BASEOP;
    };

    struct common_adoptor_closure : public rivet::range_adaptor_closure_base<common_adoptor_closure> {
      template<std::ranges::viewable_range R>
      constexpr auto operator()(R&& r) const {
          if constexpr (std::ranges::common_range<R>) {
              return std::views::all(std::forward<R>(r));
          } else {
              return std::ranges::common_view(std::forward<R>(r));
          }
      }
    };

    // RIVET_ENABLE_ADAPTORのテスト（コンパイルチェックのみ）のための簡易なもの
    struct take_adoptor : public rivet::range_adaptor_base<take_adoptor> {
      template<std::ranges::viewable_range R>
      constexpr auto operator()(R&& r, std::integral auto n) const {
        return std::ranges::take_view(std::forward<R>(r), n);
      }

      RIVET_ENABLE_ADAPTOR(take_adoptor);
    };
  }

  inline constexpr detail::filter_adoptor filter;
  inline constexpr detail::common_adoptor_closure common;
}

namespace alterdef::views {
  inline constexpr rivet::adaptor filter = []<std::ranges::viewable_range R, typename Pred>(R &&r, Pred &&p) {
    return std::ranges::filter_view(std::forward<R>(r), std::forward<Pred>(p));
  };

  inline constexpr rivet::closure common = []<std::ranges::viewable_range R>(R &&r) {
    if constexpr (std::ranges::common_range<R>)  {
      return std::views::all(std::forward<R>(r));
    } else {
      return std::ranges::common_view(std::forward<R>(r));
    }
  };
}

int main() {

  "range adoptor partial"_test = []{
    auto even = [](int n) { return n % 2 == 0;};

    auto fil1 = myranges::views::filter(even);
    auto fil2 = std::views::filter(even);
    auto fil3 = alterdef::views::filter(even);

    auto seq1 = fil1(std::views::iota(1)) | std::views::take(5);
    auto seq2 = fil2(std::views::iota(1)) | std::views::take(5);
    auto seq3 = fil3(std::views::iota(1)) | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
    ut::expect(std::ranges::equal(seq3, seq2));
  };

  "range adoptor invoke"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};

    auto seq1 = myranges::views::filter(std::views::iota(1), even) | std::views::take(5);
    auto seq2 = std::views::filter(std::views::iota(1), even)      | std::views::take(5);
    auto seq3 = alterdef::views::filter(std::views::iota(1), even) | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
    ut::expect(std::ranges::equal(seq3, seq2));
  };

  "range adoptor join"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};
    
    auto raco1 = myranges::views::filter(even) | myranges::views::common;
    auto raco2 = std::views::filter(even) | std::views::common;
    auto raco3 = alterdef::views::filter(even) | alterdef::views::common;

    auto seq1 = std::views::iota(1) | raco1 | std::views::take(5);
    auto seq2 = std::views::iota(1) | raco2 | std::views::take(5);
    auto seq3 = std::views::iota(1) | raco3 | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
    ut::expect(std::ranges::equal(seq3, seq2));
  };

  "range adoptor join invoke"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};
    
    auto raco1 = myranges::views::filter(even) | myranges::views::common;
    auto raco2 = std::views::filter(even) | std::views::common;
    auto raco3 = alterdef::views::filter(even) | alterdef::views::common;

    auto seq1 = raco1(std::views::iota(1)) | std::views::take(5);
    auto seq2 = raco2(std::views::iota(1)) | std::views::take(5);
    auto seq3 = raco3(std::views::iota(1)) | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
    ut::expect(std::ranges::equal(seq3, seq2));
  };

  "pipe"_test = [] {
    auto seq1 = std::views::iota(1) | myranges::views::filter([](int n) { return n % 2 == 0;})
                                    | myranges::views::common
                                    | std::views::take(5);

    auto seq2 = std::views::iota(1) | std::views::filter([](int n) { return n % 2 == 0;})
                                    | std::views::common
                                    | std::views::take(5);

    auto seq3 = std::views::iota(1) | alterdef::views::filter([](int n) { return n % 2 == 0;})
                                    | alterdef::views::common
                                    | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
    ut::expect(std::ranges::equal(seq3, seq2));
  };

#ifndef __cpp_lib_bind_back
  "original bind_back()"_test = [] {
    using namespace std::string_view_literals;
    using rivet::detail::bind_back;
    
    // 基本のチェック
    {
      auto testfunc = [](int, double, std::string_view, char) {
        return 20;
      };


      auto pf = bind_back(testfunc, "test"sv, 'a');

      int r = pf(1, 2.0);

      ut::expect(r == 20);
    }

    struct F {
      int operator()(int n, int m) & {
        return 10 * m + n;
      }

      int operator()(int n, int m) const & {
        return 100 * m + n;
      }

      int operator()(int n, int m) && {
        return 1000 * m + n;
      }

      int operator()(int n, int m) const && {
        return 10000 * m + n;
      }
    };

    // 完全転送コールラッパのチェック
    {
      auto pf = bind_back(F{}, 1);

      int r1 = pf(1);
      int r2 = std::as_const(pf)(2);
      int r3 = bind_back(F{}, 1)(3);
      int r4 = std::move(std::as_const(pf))(4);

      ut::expect(r1 == 11);
      ut::expect(r2 == 102);
      ut::expect(r3 == 1003);
      ut::expect(r4 == 10004);
    }

  };
#endif
}