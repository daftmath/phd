/*
 * Write a function to swap a number in place (that is, without
 * temporary variables).
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wundef"
#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

template<typename T>
void numSwapBaseline(T *const x, T *const y) {
  /*
   * Requires a temporary, not a solution.
   */
  T tmp = *x;

  *x = *y;
  *y = tmp;
}

template<typename T>
void numSwap1(T *const x, T *const y) {
  /*
   * Solution: XOR trick.
   */
  *x ^= *y;
  *y ^= *x;
  *x ^= *y;
}

// Unit tests

TEST(NumSwap, numSwap1int) {
  int x = 5, y = 10;

  numSwap1(&x, &y);

  ASSERT_EQ(10, x);
  ASSERT_EQ(5, y);

  numSwap1(&x, &y);

  ASSERT_EQ(5, x);
  ASSERT_EQ(10, y);
}

TEST(NumSwap, numSwap1float) {
  std::uint64_t x = 5, y = 10;

  numSwap1(&x, &y);

  ASSERT_EQ(10u, x);
  ASSERT_EQ(5u, y);

  numSwap1(&x, &y);

  ASSERT_EQ(5u, x);
  ASSERT_EQ(10u, y);
}

// Benchmarks

void BM_numSwapBaseline(benchmark::State& state) {
  int x = 5, y = 10;

  while (state.KeepRunning()) {
    numSwapBaseline(&x, &y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_numSwapBaseline);

void BM_numSwap1(benchmark::State& state) {
  int x = 5, y = 10;

  while (state.KeepRunning()) {
    numSwapBaseline(&x, &y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(y);
  }
}
BENCHMARK(BM_numSwap1);


int main(int argc, char **argv) {
  // Run unit tests:
  testing::InitGoogleTest(&argc, argv);
  const auto ret = RUN_ALL_TESTS();

  // Run benchmarks:
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return ret;
}