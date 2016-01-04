/*
 * Given a directed graph, design an algorithm to find out whether
 * there is a route between two nodes.
 */
#include <string>
#include <iostream>
#include <set>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wundef"
#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#pragma GCC diagnostic pop


class Node {
 public:
  Node() {}
  explicit Node(Node *const parent) : _parent(parent), _children() {}

  auto parent() { return _parent; }
  auto &children() { return _children; }
  auto ischild(Node *const n) { return children().find(n) != children().end(); }

 private:
  Node *_parent;
  std::set<Node *> _children;
};

bool routeBetweenNodes1(Node *const a, Node *const b,
                        const int maxdepth = 1000,
                        int depth = 0) {
  /*
   * Recursive solution.
   */
  if (a == b)
    return true;

  // If we're stuck in a loop, escape!
  if (depth++ > maxdepth)
    return false;

  // Recurse.
  for (auto n : a->children())
    if (routeBetweenNodes1(n, b, maxdepth, depth))
      return true;

  return false;
}


// Unit tests

TEST(DirectedGraph, routeBetweenNodes1) {
  // Directed graph test.
  Node nodes[] = {
    Node(nullptr),
    Node(),
    Node(),
    Node(),
    Node(),
    Node(),
    Node(),
    Node(),
  };

  // root -> A B C
  nodes[0].children().insert(&nodes[0]);
  nodes[0].children().insert(&nodes[1]);
  nodes[0].children().insert(&nodes[2]);
  nodes[0].children().insert(&nodes[3]);

  // A -> B D
  nodes[1].children().insert(&nodes[2]);
  nodes[1].children().insert(&nodes[4]);

  // D -> E
  nodes[4].children().insert(&nodes[5]);

  // E -> A
  nodes[5].children().insert(&nodes[1]);

  // C -> F
  nodes[3].children().insert(&nodes[6]);

  // G -> A
  nodes[7].children().insert(&nodes[0]);

  // Tests.
  ASSERT_EQ(true,  routeBetweenNodes1(&nodes[0], &nodes[0]));
  ASSERT_EQ(true,  routeBetweenNodes1(&nodes[0], &nodes[1]));
  ASSERT_EQ(true,  routeBetweenNodes1(&nodes[0], &nodes[6]));
  ASSERT_EQ(false, routeBetweenNodes1(&nodes[0], &nodes[7]));
  ASSERT_EQ(true,  routeBetweenNodes1(&nodes[4], &nodes[5]));
  ASSERT_EQ(true,  routeBetweenNodes1(&nodes[5], &nodes[4]));
  ASSERT_EQ(false, routeBetweenNodes1(&nodes[6], &nodes[0]));
}


int main(int argc, char **argv) {
  // Run unit tests:
  testing::InitGoogleTest(&argc, argv);
  const auto ret = RUN_ALL_TESTS();

  // Run benchmarks:
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return ret;
}