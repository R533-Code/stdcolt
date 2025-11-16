/*****************************************************************/ /**
 * @file   generator.h
 * @brief  Contains `Generator<T>` and useful basic generators.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINES_GENERATOR
#define __HG_STDCOLT_COROUTINES_GENERATOR

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>
#include <concepts>
#include <type_traits>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <stdcolt_contracts/contracts.h>

/// @brief Contains utilities related to coroutines
namespace stdcolt::coroutines
{
  template<typename T>
  struct Generator
  {
    static_assert(!std::is_reference_v<T>, "Generator<T&> is not supported");

    struct promise_type;
    /// @brief Short-hand for the coroutine handle type
    using handle_type = std::coroutine_handle<promise_type>;

    /// @brief Promise type, needed for coroutines
    struct promise_type
    {
      /// @brief The state of the storage
      enum class StorageState : uint8_t
      {
        /// @brief Nothing
        EMPTY,
        /// @brief Value
        VALUE,
        /// @brief Exception pointer
        EXCEPT,
      };

      /// @brief Storage suitable for storing the produced value or the exception
      alignas(std::max(alignof(T), alignof(std::exception_ptr))) char storage
          [std::max(sizeof(T), sizeof(std::exception_ptr))];
      /// @brief The state of the storage
      StorageState state = StorageState::EMPTY;

      /// @brief Creates the generator
      /// @return Generator
      Generator get_return_object()
      {
        return Generator(handle_type::from_promise(*this));
      }

      /// @brief Start the coroutine in a suspended state
      std::suspend_always initial_suspend() const noexcept { return {}; }
      /// @brief End the coroutine in a suspended state
      std::suspend_always final_suspend() const noexcept { return {}; }

      /// @brief If an exception leaks out of the coroutine, save it.
      void unhandled_exception() noexcept
      {
        auto exception = std::current_exception();
        if (state == StorageState::VALUE)
        {
          try
          {
            ((T*)storage)->~T();
          }
          catch (...)
          {
            // if the destructor of the last produced value throws,
            // then terminate (two exceptions at the same time...)
            std::terminate();
          }
        }
        new (storage) std::exception_ptr(exception);
        state = StorageState::EXCEPT;
      }

      /// @brief Yields a value, and suspend the coroutine.
      /// @tparam From The yielded value's type
      /// @param from The yielded value
      template<std::convertible_to<T> From>
      std::suspend_always yield_value(From&& from)
      {
        if (state == StorageState::EXCEPT)
        {
          state = StorageState::EMPTY;
          ((std::exception_ptr*)storage)->~exception_ptr();
        }
        else if (state == StorageState::VALUE)
        {
          state = StorageState::EMPTY;
          ((T*)storage)->~T();
        }

        try
        {
          new (storage) T(std::forward<From>(from));
          state = StorageState::VALUE;
        }
        catch (...)
        {
          new (storage) std::exception_ptr(std::current_exception());
          state = StorageState::EXCEPT;
        }
        return {};
      }

      /// @brief Does nothing
      void return_void() const noexcept {}

      /// @brief Destroys the promise, calling the destructor of the
      /// stored value.
      ~promise_type() noexcept
      {
        if (state == StorageState::EXCEPT)
        {
          state = StorageState::EMPTY;
          ((std::exception_ptr*)storage)->~exception_ptr();
        }
        else if (state == StorageState::VALUE)
        {
          state = StorageState::EMPTY;
          ((T*)storage)->~T();
        }
      }
    };

    /// @brief Creates a generator from a coroutine handle
    /// @param h The handle
    Generator(handle_type h) noexcept
        : handle(h)
    {
    }
    /// @brief Destructor, destroys the coroutine
    ~Generator() noexcept
    {
      if (handle)
        handle.destroy();
    }
    /// @brief Move constructor
    /// @param other The generator whose handle to steal
    Generator(Generator&& other) noexcept
        : handle(std::exchange(other.handle, handle_type{}))
        , full(std::exchange(other.full, false))
    {
    }
    /// @brief Move assignment operator
    /// @param other The generator whose handle to steal
    /// @return This
    Generator& operator=(Generator&& other) noexcept
    {
      if (this == &other)
        return *this;
      if (handle)
        handle.destroy();
      handle = std::exchange(other.handle, handle_type{});
      full   = std::exchange(other.full, false);
      return *this;
    }

    Generator(const Generator&)            = delete;
    Generator& operator=(const Generator&) = delete;

    /// @brief Iterator adaptor for the generator
    class iterator
    {
    public:
      /// @brief Value produced by the iterator
      using value_type = std::remove_reference_t<T>;
      /// @brief Difference type
      using difference_type = std::ptrdiff_t;
      /// @brief Input iterator
      using iterator_category = std::input_iterator_tag;
      /// @brief Reference type
      using reference = T;
      /// @brief Pointer type
      using pointer = value_type*;

      /// @brief Default constructor
      iterator() = default;
      /// @brief Constructs an iterator from a generator
      /// @param g The generator
      explicit iterator(Generator* g)
          : gen(g)
      {
        if (gen)
        {
          gen->fill();
          if (!gen->full)
            gen = nullptr;
        }
      }

      /// @brief Returns the produced value
      /// @return Produced value
      value_type operator*() const
      {
        return std::move(*(T*)gen->handle.promise().storage);
      }

      /// @brief Increments the iterator
      /// @return This
      iterator& operator++()
      {
        gen->full = false;
        gen->fill();
        if (!gen->full)
          gen = nullptr;
        return *this;
      }

      /// @brief Comparison operator to check for `end` condition.
      /// @return True if the iterator is at the end of the generator
      bool operator==(std::default_sentinel_t) const { return gen == nullptr; }

    private:
      /// @brief The generator
      Generator* gen = nullptr;
    };

    /// @brief Returns the next value or nullopt if there are no more.
    /// This method may throw an unhandled exception from the coroutine.
    /// @return
    std::optional<T> next()
    {
      fill();
      if (!full)
        return std::nullopt;

      full          = false;
      auto& promise = handle.promise();
      return std::move(*(T*)promise.storage);
    }

    /// @brief Check if the generator has a value to produce
    /// This method may throw an unhandled exception from the coroutine.
    explicit operator bool()
    {
      fill();
      return full;
    }

    /// @brief Returns the value produced by the generator
    /// This method may throw an unhandled exception from the coroutine.
    /// @return The value produced by the generator
    /// @pre operator bool must return true!
    T operator()()
    {
      fill();
      STDCOLT_assert(full, "Generator exhausted");
      full          = false;
      auto& promise = handle.promise();
      return std::move(*(T*)promise.storage);
    }

    /// @brief Returns an iterator over the generator
    /// @return Iterator over the generator
    iterator begin() { return iterator(this); }
    /// @brief end iterator
    std::default_sentinel_t end() const { return {}; }

  private:
    /// @brief The coroutine handle
    handle_type handle{};
    /// @brief If true then a value was already produced.
    bool full = false;

    /// @brief Generates the next value if not full.
    void fill()
    {
      if (!handle || full)
        return;
      if (handle.done())
        return;

      handle.resume();
      if (auto& promise = handle.promise();
          promise.state == promise_type::StorageState::EXCEPT)
      {
        auto ex = *(std::exception_ptr*)promise.storage;
        std::rethrow_exception(ex);
      }
      full = !handle.done();
    }
  };

  /// @brief Produces a sequence of integers from start (inclusive) to stop (exclusive) by step.
  /// @tparam T The type of the integer
  /// @param start The start (inclusive)
  /// @param stop The end (exclusive)
  /// @param step The step (if 0 returns nothing!)
  /// @return Generator
  template<std::integral T>
  Generator<T> range(T start, T stop, T step = T(1)) noexcept
  {
    if (step == 0)
      co_return;

    if (step > 0)
    {
      while (start < stop)
      {
        co_yield start;
        start += step;
      }
    }
    else
    {
      while (start > stop)
      {
        co_yield start;
        start += step;
      }
    }
  }

  /// @brief Produces an infinite sequence of integers starting from `initial` by incrementing.
  /// @tparam T The type of the integer
  /// @param initial The initial value (inclusive)
  /// @return Generator
  template<std::integral T>
  Generator<T> iota(T initial) noexcept
  {
    while (true)
    {
      co_yield initial;
      ++initial;
    }
  }

  /// @brief Drops the `n` next elements from a generator.
  /// If the generator has less than `n` elements to produce,
  /// then they are all dropped.
  /// @tparam T The generated type
  /// @param gen The generator
  /// @param n The number of elements to drop
  /// @return Generator
  template<typename T>
  Generator<T> drop(Generator<T> gen, size_t n)
  {
    while (n != 0 && gen)
    {
      (void)gen();
      --n;
    }

    while (gen)
      co_yield gen();
  }

  /// @brief Produces the next `n` element from a generator.
  /// If the generator has less than `n` elements to produce,
  /// then only as much elements are produced.
  /// @tparam T The generated type
  /// @param gen The generator
  /// @param n The number of elements to keep
  /// @return Generator
  template<typename T>
  Generator<T> take(Generator<T> gen, size_t n)
  {
    while (n != 0 && gen)
    {
      co_yield gen();
      --n;
    }
  }

  /// @brief Produces elements of a generator for which a function produced true.
  /// @tparam T The generated type
  /// @tparam F The filter function type
  /// @param gen The generator
  /// @param fn The filter function, returns true to keep a value
  /// @return Generator
  template<typename T, typename F>
  Generator<T> filter(Generator<T> gen, F fn)
  {
    using value_type = std::remove_reference_t<T>;
    using arg_type   = const value_type&;

    static_assert(
        std::invocable<F&, arg_type>, "Predicate must be callable with const T&");
    static_assert(
        std::convertible_to<std::invoke_result_t<F, arg_type>, bool>,
        "Predicate must return type convertible to bool");

    while (gen)
    {
      auto v = gen();
      if (std::invoke(fn, static_cast<arg_type>(v)))
        co_yield std::move(v);
    }
  }

  /// @brief Maps elements produced from a generator using a function
  /// @tparam T The generated type
  /// @tparam F The mapping function type
  /// @param gen The generator
  /// @param fn The mapping function
  /// @return Generator
  template<typename T, typename F>
    requires std::invocable<F, T&&>
  Generator<std::invoke_result_t<F, T&&>> map(Generator<T> gen, F fn)
  {
    while (gen)
      co_yield fn(gen());
  }

  /// @brief Produces pair of index and element.
  /// @tparam T The generated type
  /// @param gen The generator
  /// @param start The starting index
  /// @return
  template<typename T>
  Generator<std::pair<size_t, T>> enumerate(Generator<T> gen, size_t start = 0)
  {
    while (gen)
    {
      co_yield std::pair<size_t, T>{start, gen()};
      ++start;
    }
  }

  /// @brief Create a generator that zips multiple generators into tuples,
  /// stopping when the shortest generator is exhausted.
  /// @tparam T Type of values produced by the first generator.
  /// @tparam ...Ts Types of values produced by the remaining generators.
  /// @param gen First input generator.
  /// @param ...gens Additional generators to zip with the first.
  /// @return Generator
  template<typename T, typename... Ts>
  Generator<std::tuple<T, Ts...>> zip(Generator<T> gen, Generator<Ts>... gens)
  {
    while (true)
    {
      auto first = gen.next();
      if (!first)
        co_return;

      std::tuple<std::optional<Ts>...> others{gens.next()...};

      bool all_present = true;
      std::apply(
          [&](auto&... opt)
          { ((all_present = all_present && static_cast<bool>(opt)), ...); }, others);

      if (!all_present)
        co_return;

      co_yield std::apply(
          [&](auto&... opt)
          { return std::tuple<T, Ts...>{std::move(*first), std::move(*opt)...}; },
          others);
    }
  }
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_GENERATOR
