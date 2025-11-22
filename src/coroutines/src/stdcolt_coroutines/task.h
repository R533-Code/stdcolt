/*****************************************************************/ /**
 * @file   task.h
 * @brief  Contains `Task<T>`, and other task-related utilities.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINES_TASK
#define __HG_STDCOLT_COROUTINES_TASK

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <stdcolt_contracts/contracts.h>

namespace stdcolt::coroutines
{
  //---------------------------------
  // COMMON UTILITIES FOR ALL TASKS
  //---------------------------------

  /// @brief Trait to specify the return object type for `get_return_object`.
  /// @tparam PromiseBase The promise base for which to use
  template<typename PromiseBase>
  struct return_object_for_promise_base;

  /// @brief Generic promise for task storage.
  /// @tparam T The result type to store
  /// @tparam PROMISE_BASE The promise base class to inherit for extra-data
  template<typename T, typename PROMISE_BASE>
  class generic_task_promise final : public PROMISE_BASE
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
    alignas(std::max(alignof(T), alignof(std::exception_ptr))) char storage[std::max(
        sizeof(T), sizeof(std::exception_ptr))];
    /// @brief The state of the storage
    StorageState state = StorageState::EMPTY;

    /// @brief Returns the exception pointer
    /// @pre `state == StorageState::EXCEPT`
    /// @return The exception pointer
    std::exception_ptr& get_exception() noexcept
    {
      STDCOLT_debug_pre(state == StorageState::EXCEPT, "expected exception");
      return *(std::exception_ptr*)storage;
    }
    /// @brief Returns the value
    /// @pre `state == StorageState::VALUE`
    /// @return The value
    T& get_value() noexcept
    {
      STDCOLT_debug_pre(state == StorageState::VALUE, "expected value");
      return *(T*)storage;
    }

  public:
    /// @brief Constructor, initializes to an empty state
    generic_task_promise() noexcept = default;
    /// @brief Constructor, forwards the arguments to the PROMISE_BASE
    /// @tparam ...Args Parameter pack
    /// @param ...args Argument pack
    template<typename... Args>
      requires std::constructible_from<PROMISE_BASE, Args&&...>
    generic_task_promise(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<PROMISE_BASE, Args&&...>)
        : PROMISE_BASE(std::forward<Args>(args)...)
    {
    }
    /// @brief Destructor, destroys the value if there is one
    ~generic_task_promise()
    {
      if (state == StorageState::VALUE)
        get_value().~T();
      else if (state == StorageState::EXCEPT)
        get_exception().~exception_ptr();
    }

    /// @brief The type to construct in `get_return_object`
    using return_type =
        typename return_object_for_promise_base<PROMISE_BASE>::template type<T>;

    /// @brief Creates the return object using `return_object_for_promise_base`
    /// @return The return object, constructed from the handle
    return_type get_return_object() noexcept
    {
      using promise_t = generic_task_promise<T, PROMISE_BASE>;
      auto h          = std::coroutine_handle<promise_t>::from_promise(*this);
      return return_type{h};
    }
    /// @brief On unhandled exceptions, capture to exception in the storage
    void unhandled_exception() noexcept
    {
      STDCOLT_debug_assert(state == StorageState::EMPTY, "Task must be empty");
      // as Task must only co_return, not co_yield ^^^
      new (storage) std::exception_ptr(std::current_exception());
      state = StorageState::EXCEPT;
    }

    /// @brief Returns a value from the coroutine
    /// @tparam U The type from which to convert
    /// @param value The value to return
    template<std::convertible_to<T> U>
    void return_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
    {
      STDCOLT_debug_assert(state == StorageState::EMPTY, "Task must be empty");
      // as Task must only co_return, not co_yield ^^^
      try
      {
        new (storage) T(std::forward<U>(value));
        state = StorageState::VALUE;
      }
      catch (...)
      {
        new (storage) std::exception_ptr(std::current_exception());
        state = StorageState::EXCEPT;
      }
    }

    /// @brief Returns the result, or throws the unhandled exception
    /// @return The returned object
    T& result() &
    {
      if (state == StorageState::EXCEPT)
        std::rethrow_exception(get_exception());
      else
        return get_value();
    }

    /// @brief Returns the result, or throws the unhandled exception
    /// @return The returned object
    T&& result() &&
    {
      if (state == StorageState::EXCEPT)
        std::rethrow_exception(get_exception());
      else
        return std::move(get_value());
    }
  };

  template<typename PROMISE_BASE>
  class generic_task_promise<void, PROMISE_BASE> : public PROMISE_BASE
  {
    std::exception_ptr exception;

  public:
    generic_task_promise() noexcept = default;
    /// @brief Constructor, forwards the arguments to the PROMISE_BASE
    /// @tparam ...Args Parameter pack
    /// @param ...args Argument pack
    template<typename... Args>
      requires std::constructible_from<PROMISE_BASE, Args&&...>
    generic_task_promise(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<PROMISE_BASE, Args&&...>)
        : PROMISE_BASE(std::forward<Args>(args)...)
    {
    }

    using return_type =
        typename return_object_for_promise_base<PROMISE_BASE>::template type<void>;
    return_type get_return_object() noexcept
    {
      using promise_t = generic_task_promise<void, PROMISE_BASE>;
      auto h          = std::coroutine_handle<promise_t>::from_promise(*this);
      return return_type{h};
    }

    void return_void() noexcept {}

    void unhandled_exception() noexcept { exception = std::current_exception(); }

    void result()
    {
      if (exception)
        std::rethrow_exception(exception);
    }
  };

  template<typename T, typename PROMISE_BASE>
  class generic_task_promise<T&, PROMISE_BASE> : public PROMISE_BASE
  {
    T* value = nullptr;
    std::exception_ptr exception;

  public:
    generic_task_promise() noexcept = default;
    /// @brief Constructor, forwards the arguments to the PROMISE_BASE
    /// @tparam ...Args Parameter pack
    /// @param ...args Argument pack
    template<typename... Args>
      requires std::constructible_from<PROMISE_BASE, Args&&...>
    generic_task_promise(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<PROMISE_BASE, Args&&...>)
        : PROMISE_BASE(std::forward<Args>(args)...)
    {
    }

    using return_type =
        typename return_object_for_promise_base<PROMISE_BASE>::template type<T&>;
    return_type get_return_object() noexcept
    {
      using promise_t = generic_task_promise<T&, PROMISE_BASE>;
      auto h          = std::coroutine_handle<promise_t>::from_promise(*this);
      return return_type{h};
    }

    void unhandled_exception() noexcept { exception = std::current_exception(); }
    void return_value(T& val) noexcept { value = &val; }

    T& result()
    {
      if (exception)
        std::rethrow_exception(exception);
      return *value;
    }
  };

  //--------------------------------------
  // SINGLE OWNING, SYNCHRONOUS `Task<T>`
  //--------------------------------------

  /// @brief Promise base for `Task<T>`
  struct TaskPromiseBase
  {
    /// @brief The coroutine to continue after the final suspension point
    std::coroutine_handle<> continuation{};

    /// @brief Awaitable for `final_suspend`
    struct final_awaitable
    {
      /// @brief Never ready
      bool await_ready() const noexcept { return false; }

      /// @brief Returns the continuation or `std::noop_coroutine` if there isn't any
      /// @tparam PROMISE The promise type
      /// @param handle The coroutine handle
      /// @return The continuation
      template<typename PROMISE>
      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<PROMISE> handle) const noexcept
      {
        auto ret = handle.promise().continuation;
        return ret ? ret : std::noop_coroutine();
      }

      /// @brief Does nothing
      void await_resume() const noexcept {}
    };

    /// @brief Initially suspended
    auto initial_suspend() const noexcept { return std::suspend_always{}; }
    /// @brief Starts the continuation
    auto final_suspend() const noexcept { return final_awaitable{}; }
  };

  template<typename T = void>
  struct Task
  {
    using promise_type = generic_task_promise<T, TaskPromiseBase>;
    using value_type   = T;

  private:
    std::coroutine_handle<promise_type> handle;

    struct awaitable_base
    {
      std::coroutine_handle<promise_type> handle;

      awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept
          : handle(coroutine)
      {
      }

      bool await_ready() const noexcept { return !handle || handle.done(); }

      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<> coroutine) noexcept
      {
        handle.promise().continuation = coroutine;
        return handle;
      }
    };

  public:
    Task() noexcept
        : handle(nullptr)
    {
    }
    explicit Task(std::coroutine_handle<promise_type> coroutine)
        : handle(coroutine)
    {
    }
    ~Task()
    {
      if (handle)
        handle.destroy();
    }
    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& t) noexcept
        : handle(std::exchange(t.handle, nullptr))
    {
    }
    Task& operator=(Task&& other) noexcept
    {
      if (this == &other)
        return *this;
      if (handle)
        handle.destroy();
      handle = std::exchange(other.handle, nullptr);
      return *this;
    }

    bool is_ready() const noexcept { return !handle || handle.done(); }

    auto operator co_await() const& noexcept
    {
      struct awaitable : awaitable_base
      {
        using awaitable_base::awaitable_base;

        decltype(auto) await_resume()
        {
          STDCOLT_pre(this->handle, "task was empty");
          return this->handle.promise().result();
        }
      };
      return awaitable{handle};
    }

    auto operator co_await() const&& noexcept
    {
      struct awaitable : awaitable_base
      {
        using awaitable_base::awaitable_base;

        decltype(auto) await_resume()
        {
          STDCOLT_pre(this->handle, "task was empty");
          return std::move(this->handle.promise()).result();
        }
      };
      return awaitable{handle};
    }

    auto when_ready() const noexcept
    {
      struct awaitable : awaitable_base
      {
        using awaitable_base::awaitable_base;
        void await_resume() const noexcept {}
      };
      return awaitable{handle};
    }

    auto get_handle() const noexcept { return handle; }
    auto steal_handle() noexcept { return std::exchange(handle, nullptr); }

    decltype(auto) result() &
    {
      STDCOLT_pre(is_ready(), "Task not ready yet");
      if constexpr (std::is_void_v<T>)
        handle.promise().result();
      else
        return (handle.promise().result());
    }

    decltype(auto) result() &&
    {
      STDCOLT_pre(is_ready(), "Task not ready yet");
      if constexpr (std::is_void_v<T>)
        (std::move(handle.promise()).result());
      else
        return (std::move(handle.promise()).result());
    }
  };

  /// @brief For TaskPromiseBase, a Task<T> is returned
  template<>
  struct return_object_for_promise_base<TaskPromiseBase>
  {
    template<typename T>
    using type = Task<T>;
  };
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_TASK
