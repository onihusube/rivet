#include "rivet.hpp"

#include <algorithm>

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
      RIVET_ENABLE_ADAPTOR(filter_adoptor);
    };

    struct common_adoptor_closure : public rivet::range_adaptor_base<common_adoptor_closure, true>
    {
      template<std::ranges::viewable_range R>
      constexpr auto operator()(R&& r) const {
          if constexpr (std::ranges::common_range<R>) {
              return std::views::all(std::forward<R>(r));
          } else {
              return std::ranges::common_view(std::forward<R>(r));
          }
      }
    };
  }

  inline constexpr detail::filter_adoptor filter;
  inline constexpr detail::common_adoptor_closure common;
}

int main() {

  "range adoptor partial"_test = []{
    auto even = [](int n) { return n % 2 == 0;};

    auto fil1 = myranges::views::filter(even);
    auto fil2 = std::views::filter(even);

    auto seq1 = fil1(std::views::iota(1));
    auto seq2 = fil2(std::views::iota(1));

    auto it1 = seq1.begin();
    auto it2 = seq2.begin();
    for ([[maybe_unused]] int i : {1, 2, 3}) {
      ut::expect(*it1 == *it2);

      ++it1;
      ++it2;
    }
  };

  "range adoptor invoke"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};
    
    auto seq1 = myranges::views::filter(std::views::iota(1), even);
    auto seq2 = std::views::filter(std::views::iota(1), even);

    auto it1 = seq1.begin();
    auto it2 = seq2.begin();
    for ([[maybe_unused]] int i : {1, 2, 3}) {
      ut::expect(*it1 == *it2);

      ++it1;
      ++it2;
    }
  };

  "range adoptor join"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};
    
    auto raco1 = myranges::views::filter(even) | myranges::views::common;
    auto raco2 = std::views::filter(even) | std::views::common;

    auto seq1 = std::views::iota(1) | raco1;
    auto seq2 = std::views::iota(1) | raco2;

    auto it1 = seq1.begin();
    auto it2 = seq2.begin();
    for ([[maybe_unused]] int i : {1, 2, 3}) {
      ut::expect(*it1 == *it2);

      ++it1;
      ++it2;
    }
  };

  "range adoptor join invoke"_test = [] {
    auto even = [](int n) { return n % 2 == 0;};
    
    auto raco1 = myranges::views::filter(even) | myranges::views::common;
    auto raco2 = std::views::filter(even) | std::views::common;

    auto seq1 = raco1(std::views::iota(1));
    auto seq2 = raco2(std::views::iota(1));

    auto it1 = seq1.begin();
    auto it2 = seq2.begin();
    for ([[maybe_unused]] int i : {1, 2, 3}) {
      ut::expect(*it1 == *it2);

      ++it1;
      ++it2;
    }
  };

  "pipe"_test = [] {
    auto seq1 = std::views::iota(1) | myranges::views::filter([](int n) { return n % 2 == 0;})
                                    | myranges::views::common
                                    | std::views::take(5);
    auto seq2 = std::views::iota(1) | std::views::filter([](int n) { return n % 2 == 0;})
                                    | std::views::common
                                    | std::views::take(5);

    ut::expect(std::ranges::equal(seq1, seq2));
  };
}