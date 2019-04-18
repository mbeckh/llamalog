/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file

#pragma once

#include <fmt/core.h>

#include <utility>

namespace llamalog::internal {

/// @brief Helper class for running code during stack unwinding (similar to finally from the gsl, but without copy or move).
/// @tparam F The type of the lambda called during stack unwinding.
template <class F>
class FinalAction final {
public:
	/// @brief Create a final action.
	/// @param f The lambda to be called during stack unwinding.
	explicit FinalAction(F f) noexcept
		: m_f{std::move(f)} {
		static_assert(noexcept(m_f()), "lambda must be noexcept");
	}

	FinalAction(const FinalAction&) = delete;  ///< @nocopyconstructor
	FinalAction(FinalAction&&) = delete;       ///< @nomoveconstructor

	/// @brief Calls the lambda.
	~FinalAction() noexcept {
		m_f();
	}

public:
	FinalAction& operator=(const FinalAction&) = delete;  ///< @noassignmentoperator
	FinalAction& operator=(FinalAction&&) = delete;       ///< @nomoveoperator

private:
	F m_f;  ///< @brief Our lambda.
};

/// @brief Run code during stack unwinding.
/// @tparam F The (auto-deducted) type of the lambda expression.
/// @param f The lambda expression. The expression MUST NOT throw any exceptions.
/// @return The guard object.
template <class F>
inline FinalAction<F> finally(F&& f) noexcept {  // NOLINT(readability-identifier-naming): cf. GSL
	return FinalAction<F>(std::forward<F>(f));
}

}  // namespace llamalog::internal
