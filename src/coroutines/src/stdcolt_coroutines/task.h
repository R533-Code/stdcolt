/*****************************************************************/ /**
 * @file   task.h
 * @brief  Contains `Task<T>`
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
  template<typename T>
  class Task;

  namespace detail
  {
    class TaskPromiseBase
    {
      std::coroutine_handle<> _continuation;

      friend struct final_awaitable;

      struct final_awaitable
      {
        bool await_ready() const noexcept { return false; }

        template<typename PROMISE>
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<PROMISE> handle) noexcept
        {
          return handle.promise()._continuation;
        }

        void await_resume() noexcept {}
      };

    public:
      TaskPromiseBase() noexcept {}

      auto initial_suspend() noexcept { return std::suspend_always{}; }
      auto final_suspend() noexcept { return final_awaitable{}; }
      void set_continuation(std::coroutine_handle<> continuation) noexcept
      {
        _continuation = continuation;
      }
    };

    template<typename T>
    class TaskPromise final : public TaskPromiseBase
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

      std::exception_ptr& get_exception() noexcept
      {
        STDCOLT_debug_pre(state == StorageState::EXCEPT, "expected exception");
        return *(std::exception_ptr*)storage;
      }
      T& get_value() noexcept
      {
        STDCOLT_debug_pre(state == StorageState::VALUE, "expected value");
        return *(T*)storage;
      }

    public:
      TaskPromise() noexcept = default;

      ~TaskPromise()
      {
        if (state == StorageState::VALUE)
          get_value().~T();
        else if (state == StorageState::EXCEPT)
          get_exception().~exception_ptr();
      }

      Task<T> get_return_object() noexcept;

      void unhandled_exception() noexcept
      {
        STDCOLT_debug_assert(state == StorageState::EMPTY, "Task must be empty");
        // as Task must only co_return, not co_yield ^^^
        new (storage) std::exception_ptr(std::current_exception());
        state = StorageState::EXCEPT;
      }

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

      T& result() &
      {
        if (state == StorageState::EXCEPT)
          std::rethrow_exception(get_exception());
        return get_value();
      }

      T&& result() &&
      {
        if (state == StorageState::EXCEPT)
          std::rethrow_exception(get_exception());
        return std::move(get_value());
      }
    };

    template<>
    class TaskPromise<void> : public TaskPromiseBase
    {
      std::exception_ptr exception;

    public:
      TaskPromise() noexcept = default;

      Task<void> get_return_object() noexcept;

      void return_void() noexcept {}

      void unhandled_exception() noexcept { exception = std::current_exception(); }

      void result()
      {
        if (exception)
          std::rethrow_exception(exception);
      }
    };

    template<typename T>
    class TaskPromise<T&> : public TaskPromiseBase
    {
      T* value = nullptr;
      std::exception_ptr exception;

    public:
      TaskPromise() noexcept = default;

      Task<T&> get_return_object() noexcept;

      void unhandled_exception() noexcept { exception = std::current_exception(); }
      void return_value(T& val) noexcept { value = &val; }

      T& result()
      {
        if (exception)
          std::rethrow_exception(exception);
        return *value;
      }
    };
  } // namespace detail

  template<typename T = void>
  struct Task
  {
    using promise_type = detail::TaskPromise<T>;
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
        handle.promise().set_continuation(coroutine);
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
  };

  namespace detail
  {
    template<typename T>
    Task<T> TaskPromise<T>::get_return_object() noexcept
    {
      return Task<T>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    inline Task<void> TaskPromise<void>::get_return_object() noexcept
    {
      return Task<void>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    template<typename T>
    Task<T&> TaskPromise<T&>::get_return_object() noexcept
    {
      return Task<T&>{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }
  } // namespace detail
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_TASK
