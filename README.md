[![Test by MSVC](https://github.com/onihusube/rivet/actions/workflows/msvc.yml/badge.svg)](https://github.com/onihusube/rivet/actions/workflows/msvc.yml)
[![Test by GCC](https://github.com/onihusube/rivet/actions/workflows/gcc.yml/badge.svg)](https://github.com/onihusube/rivet/actions/workflows/gcc.yml)

# rivet

`rivet` is a library to create range adaptor(closure) for the original `view` type.

- header only
- single file
- cross platform
    - GCC 10 or later
    - MSVC 2019 or later
    - clang 15(14?) or later

This library is only for C++20 or later.

## Usage

Copy `include/rivet.hpp` somewhere, reference it from your project, and include `rivet.hpp`.

## Example

After inclusion of the header(`rivet.hpp`), `rivet::range_adaptor_base<Derived>`(for range adaptor) or `rivet::range_adaptor_closure_base<Derived>`(for range adaptor closure) is used in the public inheritance by CRTP.

### Range Adoptor

Example of a unique implementation of `views::filter`, the range adaptor.

```cpp
#include "rivet.hpp"

namespace myranges::views {
  namespace detail {
    
    // Define adapter type and CRTP
    struct filter_adaptor: public rivet::range_adaptor_base<filter_adaptor> {

      // Range Adoptor main process. Generates the original view.
      // Always defined in `operator() const`.
      template<std::ranges::viewable_range R, typename Pred>
      constexpr auto operator()(R&& r, Pred&& p) const {
        return std::ranges::filter_view(std::forward<R>(r), std::forward<Pred>(p));
      }

      // It is a hassle, but essential.
      RIVET_ENABLE_ADAPTOR(filter_adaptor);
    };
  }

  // My views::filter(Range adaptor object) difinition.
  inline constexpr detail::filter_adaptor filter;
}
```

### Range Adoptor Closure

Example of a unique implementation of `views::common`, the range adaptor closure.

```cpp
#include "rivet.hpp"

namespace myranges::views {
  namespace detail {

    // Define adapter closure type and CRTP
    // In this case, specify true after the type name(common_adaptor_closure) to make it clear that it is range adaptor closure.
    struct common_adaptor_closure : public rivet::range_adaptor_closure_base<common_adaptor_closure> {

      // Range Adoptor Closure main process. Generates the original view.
      // Always defined in `operator() const`.
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

  // My views::common(Range adaptor closure object) difinition.
  inline constexpr detail::common_adaptor_closure common;
}
```

### Alternate syntax with lambdas

```cpp
#include "rivet.hpp"

namespace myranges::views {

  // My views::filter(Range adaptor object) difinition.
  inline constexpr rivet::adaptor filter = []<std::ranges::viewable_range R, typename Pred>(R &&r, Pred &&p) {
    // Range Adoptor main process. Generates the original view.
    return std::ranges::filter_view(std::forward<R>(r), std::forward<Pred>(p));
  };

  // My views::common(Range adaptor closure object) difinition.
  inline constexpr rivet::closure common = []<std::ranges::viewable_range R>(R &&r) {
    // Range Adoptor Closure main process. Generates the original view.
    if constexpr (std::ranges::common_range<R>)  {
      return std::views::all(std::forward<R>(r));
    } else {
      return std::ranges::common_view(std::forward<R>(r));
    }
  };
}
```

Range adaptor (closure) object can be defined by simply passing a lambda expression describing the process of generating your own `view` to `rivet::adaptor`(for adaptor) or `rivet::closure`(for adaptor closure).

## Reference

- [P2387R3 Pipe support for user-defined range adaptors](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2387r3.html)
- [［C++］ rangesのパイプにアダプトするには - 地面を見下ろす少年の足蹴にされる私](https://onihusube.hatenablog.com/entry/2022/04/24/010041)
