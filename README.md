# rivet

`rivet` is a library to create range adoptor(closure) for the original `view` type.

- header only
- single file
- cross platform
    - GCC10/11
    - MSVC 2019/2022
    - clang 14?

This library is only for C++20 or later.

## Example

After inclusion of the header(`rivet.hpp`), `rivet::range_adaptor_base<Derived>` is used in the public inheritance by CRTP.

### Range Adoptor

Example of a unique implementation of `views::filter`, the range adoptor.

```cpp
namespace myranges::views {
  namespace detail {
    
    // Define adapter type and CRTP
    struct filter_adoptor: public rivet::range_adaptor_base<filter_adoptor> {

      // Range Adoptor main process. Generates the original view.
      // Always defined in `operator() const`.
      template<std::ranges::viewable_range R, typename Pred>
      constexpr auto operator()(R&& r, Pred&& p) const {
        return std::ranges::filter_view(std::forward<R>(r), std::forward<Pred>(p));
      }

      // It is a hassle, but essential.
      RIVET_ENABLE_ADOPTOR(filter_adoptor);
    };
  }

  // My views::filter(Range adoptor object) difinition.
  inline constexpr detail::filter_adoptor filter;
}
```

### Range Adoptor Closure

Example of a unique implementation of `views::common`, the range adoptor closure.

```cpp
namespace myranges::views {
  namespace detail {

    // Define adapter closure type and CRTP
    // In this case, specify true after the type name(common_adoptor_closure) to make it clear that it is range adoptor closure.
    struct common_adoptor_closure : public rivet::range_adaptor_base<common_adoptor_closure, true> {

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

  // My views::common(Range adoptor closure object) difinition.
  inline constexpr detail::common_adoptor_closure common;
}
```

