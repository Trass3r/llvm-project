//===-------------- lib/Support/Hashing.cpp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides implementation bits for the LLVM common hashing
// infrastructure. Documentation and most of the other information is in the
// header file.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Hashing.h"
#include <algorithm>

using namespace llvm;

// Provide a definition and static initializer for the fixed seed. This
// initializer should always be zero to ensure its value can never appear to be
// non-zero, even during dynamic initialization.
uint64_t llvm::hashing::detail::fixed_seed_override = 0;

// Implement the function for forced setting of the fixed seed.
// FIXME: Use atomic operations here so that there is no data race.
void llvm::set_fixed_execution_hash_seed(uint64_t fixed_value) {
  hashing::detail::fixed_seed_override = fixed_value;
}

namespace llvm::hashing::detail {

hash_code hash_combine_recursive_helper::combine(size_t length,
                                                 char *buffer_ptr,
                                                 char *buffer_end) {
  // Check whether the entire set of values fit in the buffer. If so, we'll
  // use the optimized short hashing routine and skip state entirely.
  if (length == 0)
    return hash_short(buffer, buffer_ptr - buffer, seed);

  // Mix the final buffer, rotating it if we did a partial fill in order to
  // simulate doing a mix of the last 64-bytes. That is how the algorithm
  // works when we have a contiguous byte sequence, and we want to emulate
  // that here.
  std::rotate(buffer, buffer_ptr, buffer_end);

  // Mix this chunk into the current state.
  state.mix(buffer);
  length += buffer_ptr - buffer;

  return state.finalize(length);
}

hash_state hash_state::create(const char *s, uint64_t seed) {
  hash_state state = {0,
                      seed,
                      hash_16_bytes(seed, k1),
                      rotate(seed ^ k1, 49),
                      seed * k1,
                      shift_mix(seed),
                      0};
  state.h6 = hash_16_bytes(state.h4, state.h5);
  state.mix(s);
  return state;
}

void hash_state::mix_32_bytes(const char *s, uint64_t &a, uint64_t &b) {
  a += fetch64(s);
  uint64_t c = fetch64(s + 24);
  b = rotate(b + a + c, 21);
  uint64_t d = a;
  a += fetch64(s + 8) + fetch64(s + 16);
  b += rotate(a, 44) + d;
  a += c;
}

void hash_state::mix(const char *s) {
  h0 = rotate(h0 + h1 + h3 + fetch64(s + 8), 37) * k1;
  h1 = rotate(h1 + h4 + fetch64(s + 48), 42) * k1;
  h0 ^= h6;
  h1 += h3 + fetch64(s + 40);
  h2 = rotate(h2 + h5, 33) * k1;
  h3 = h4 * k1;
  h4 = h0 + h5;
  mix_32_bytes(s, h3, h4);
  h5 = h2 + h6;
  h6 = h1 + fetch64(s + 16);
  mix_32_bytes(s + 32, h5, h6);
  std::swap(h2, h0);
}

uint64_t hash_state::finalize(size_t length) {
  return hash_16_bytes(hash_16_bytes(h3, h5) + shift_mix(h1) * k1 + h2,
                       hash_16_bytes(h4, h6) + shift_mix(length) * k1 + h0);
}

} // namespace llvm::hashing::detail
