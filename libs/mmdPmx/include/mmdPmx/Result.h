// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "mmdPmx/Diagnostic.h"

#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace mmd {

/// The outcome of one pipeline transition (docs/design/DESIGN_POLICY.md §4):
/// either a value and the recoverable diagnostics raised while producing it,
/// or the fatal diagnostic that prevented one.
///
/// A failure keeps the recoverable diagnostics raised before the fatal one, so
/// a tool can report everything it saw. `diagnostics()` never contains a fatal
/// diagnostic; `fatal()` is the only place one appears.
template <class T>
class Result {
public:
    static Result Success(T value, std::vector<Diagnostic> diagnostics = {})
    {
        Result result;
        result._value.emplace(std::move(value));
        result._diagnostics = std::move(diagnostics);
        assert(result._AllRecoverable());
        return result;
    }

    static Result Failure(Diagnostic fatal, std::vector<Diagnostic> diagnostics = {})
    {
        assert(fatal.severity == Severity::Fatal && !fatal.recoverable);
        Result result;
        result._fatal.emplace(std::move(fatal));
        result._diagnostics = std::move(diagnostics);
        assert(result._AllRecoverable());
        return result;
    }

    bool ok() const noexcept { return _value.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    /// The value. Throws std::bad_optional_access on a failure.
    const T& value() const& { return _value.value(); }
    T& value() & { return _value.value(); }
    T&& value() && { return std::move(_value).value(); }

    /// The fatal diagnostic, or nullptr on success.
    const Diagnostic* fatal() const noexcept { return _fatal ? &*_fatal : nullptr; }

    /// Recoverable diagnostics, in emission order.
    const std::vector<Diagnostic>& diagnostics() const noexcept { return _diagnostics; }

private:
    Result() = default;

    bool _AllRecoverable() const
    {
        for (const Diagnostic& d : _diagnostics) {
            if (!d.recoverable) {
                return false;
            }
        }
        return true;
    }

    std::optional<T> _value;
    std::optional<Diagnostic> _fatal;
    std::vector<Diagnostic> _diagnostics;
};

}  // namespace mmd
