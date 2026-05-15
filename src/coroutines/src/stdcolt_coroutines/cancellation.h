/*****************************************************************/ /**
 * @file   cancellation.h
 * @brief  Contains `CancellationToken` and `CancellationSource`.
 * 
 * @author Raphael Dib Nehme
 * @date   May 2026
 *********************************************************************/
#pragma once

#include <atomic>
#include <memory>

namespace stdcolt::coroutines
{
  class CancellationSource;

  /// @brief Lightweight non-owning view of a cancellation state.
  /// A null (default-constructed) token means uncancellable: `is_cancelled()`
  /// always returns false and no atomic load is performed.
  /// A non-null token is produced by a `CancellationSource`. All tokens produced
  /// by the same source share the same underlying state.
  /// @warning A token must not outlive the `CancellationSource` that produced it.
  class CancellationToken
  {
    const std::atomic<bool>* _state = nullptr;

    explicit constexpr CancellationToken(const std::atomic<bool>* state) noexcept
        : _state(state)
    {
    }

    friend class CancellationSource;

  public:
    /// @brief Constructs a null (uncancellable) token.
    constexpr CancellationToken() noexcept                         = default;
    constexpr CancellationToken(const CancellationToken&) noexcept = default;
    constexpr CancellationToken& operator=(const CancellationToken&) noexcept =
        default;

    /// @brief Returns true if cancellation has been requested on the source.
    /// A null token always returns false without touching any atomic.
    bool is_cancelled() const noexcept
    {
      if (_state == nullptr)
        return false;
      return _state->load(std::memory_order_acquire);
    }

    /// @brief Returns true if this token can ever observe a cancellation.
    /// A null token is not cancellable and is_cancelled() will always be false.
    constexpr bool is_cancellable() const noexcept { return _state != nullptr; }
  };

  /// @brief Owning source that produces `CancellationToken`s.
  /// @note  Not copyable. Movable. After a move the source is in a valid but
  /// empty state: `cancel()` and `is_cancelled()` are no-ops.
  /// @warning All tokens produced by this source must not outlive it.
  class CancellationSource
  {
    std::unique_ptr<std::atomic<bool>> _state;

  public:
    /// @brief Constructs an active (non-cancelled) source.
    CancellationSource()
        : _state(std::make_unique<std::atomic<bool>>(false))
    {
    }

    CancellationSource(CancellationSource&&) noexcept            = default;
    CancellationSource& operator=(CancellationSource&&) noexcept = default;

    CancellationSource(const CancellationSource&)            = delete;
    CancellationSource& operator=(const CancellationSource&) = delete;

    /// @brief Returns a non-owning token that reflects this source's state.
    /// @warning The returned token must not outlive this source.
    CancellationToken token() const noexcept
    {
      return CancellationToken{_state.get()};
    }
    /// @brief Requests cancellation. Idempotent.
    /// Safe to call from any thread.
    /// No-op if the source has been moved from.
    void cancel() noexcept
    {
      if (_state)
        _state->store(true, std::memory_order_release);
    }

    /// @brief Returns true if cancellation has been requested.
    bool is_cancelled() const noexcept
    {
      if (!_state)
        return false;
      return _state->load(std::memory_order_acquire);
    }
  };

} // namespace stdcolt::coroutines