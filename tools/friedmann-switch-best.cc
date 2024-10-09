#include <iostream>
#include <map>
#include <string>
#include <set>

using prio_t = ssize_t;

#define die(S)                                  \
  do {                                          \
  std::cerr << S << "\n";                       \
  exit (2);                                     \
  } while (0)

std::map<std::string, std::string> nodes;
std::map<std::string, std::set<std::string>> trans;
ssize_t nnodes = 0, ntrans = 0, highest_prio = 0;

namespace std {
  std::string operator+ (const std::string& s, size_t n) {
    return s + std::to_string (n);
  }
}

void add_node (const std::string& name, int owner, prio_t prio) {
  if (nodes.contains (name)) die ("node already exists " << name);
  nodes[name] += std::to_string (prio) + " " + std::to_string (owner);
  ++nnodes;
  if (prio > highest_prio) highest_prio = prio;
}

void add_trans (const std::string& node, std::initializer_list<std::string> succs) {
  if (not nodes.contains (node)) die ("adding transition from nonexisting node: " << node);
  trans[node].insert (succs.begin (), succs.end ());
  ntrans += succs.size ();
}

void sanity_check (size_t n) {
  ssize_t exp_nodes = 21 * n;
  if (exp_nodes != nnodes )
    die ("wrong number of nodes " << nnodes << " expected " << exp_nodes);

  // Original thesis has 81 * n instead of 75 * n, it seems that it's a typo.
  ssize_t exp_trans = (7 * n * n + 75 * n - 8) / 2;
  if (exp_trans != ntrans)
    die ("wrong number of transitions: " << ntrans << " expected " << exp_trans);

  prio_t exp_high_prio = 24 * n + 6;
  if (exp_high_prio != highest_prio)
    die ("wrong highest priority: " << highest_prio << " expected " << exp_high_prio);
}

void dump_game () {
  std::cout << "parity " << nnodes << ";\n";
  size_t n = 0;
  std::map<std::string, size_t> names;
  for (auto&& [name, _] : nodes)
    names[name] = n++;

  for (auto&& [n, t] : nodes) {
    std::cout << names[n] << " " << t << " ";
    bool first = true;
    for (auto&& succ : trans[n]) {
      if (not names.contains (succ))
        die ("successor of " << n << " undefined: " << succ);
      if (not first) std::cout << ",";
      first = false;
      std::cout << names[succ];
    }
    std::cout << ";\n";
  }
}

int main (int argc, char** argv) {
  using namespace std::string_literals;

  if (argc != 2) die ("usage: " << argv[0] << " N");
  size_t n = std::stoul (argv[1]);
  // ti
  add_node ("t1", 0, 8*n+3);
  add_trans ("t1", { "s", "r", "c" });
  for (size_t i = 2; i <= 6*n-2; ++i) {
    add_node ("t"s + i, 0, 8*n+2*i+1);
    add_trans ("t"s + i, { "s", "r", "t"s + (i-1) });
  }

  //ai
  for (size_t i = 1; i <= 6*n-2; ++i) {
    add_node ("a"s + i, 1, 8*n + 2*i + 2);
    add_trans ("a"s + i, { "t"s + i });
  }

  // c
  add_node ("c", 1, 20*n);
  add_trans ("c", { "r" });

  // d1_i
  for (size_t i = 1; i <= n; ++i) {
    add_node ("d1_"s + i, 0, 8*i+1);
    add_trans ("d1_"s + i, { "s", "c", "d2_"s + i });
    for (size_t j = 1; j <= 2*i-2; ++j) {
      add_trans ("d1_"s + i, { "a"s + (3*j+3) });
    }
  }

  // d2_i
  for (size_t i = 1; i <= n; ++i) {
    add_node ("d2_"s + i, 0, 8*i+3);
    add_trans ("d2_"s + i, { "d3_"s + i });
    for (size_t j = 1; j <= 2*i-2; ++j) {
      add_trans ("d2_"s + i, { "a"s + (3*j+2) });
    }
  }

  // d3_i
  for (size_t i = 1; i <= n; ++i) {
    add_node ("d3_"s + i, 0, 8*i+5);
    add_trans ("d3_"s + i, { "e"s + i });
    for (size_t j = 1; j <= 2*i-1; ++j) {
      add_trans ("d3_"s + i, { "a"s + (3*j+1) });
    }
  }

  // ei
  for (size_t i = 1; i <= n; ++i) {
    add_node ("e"s + i, 1, 8*i+6);
    add_trans ("e"s + i, { "d1_"s + i, "h"s + i });
  }

  // yi
  for (size_t i = 1; i <= n; ++i) {
    add_node ("y"s + i, 0, 8*i+7);
    add_trans ("y"s + i, { "f"s + i, "k"s + i });
  }

  // gi
  for (size_t i = 1; i <= n; ++i) {
    add_node ("g"s + i, 0, 8*i+8);
    add_trans ("g"s + i, { "y"s + i, "k"s + i });
  }

  // ki
  for (size_t i = 1; i <= n; ++i) {
    add_node ("k"s + i, 0, 20*n+4*i+3);
    add_trans ("k"s + i, { "x" });
    for (size_t j = i+1; j <= n; ++j) {
      add_trans ("k"s + i, { "g"s + j });
    }
  }

  // fi
  for (size_t i = 1; i <= n; ++i) {
    add_node ("f"s + i, 1, 20*n+4*i+5);
    add_trans ("f"s + i, { "e"s + i });
  }

  // fi
  for (size_t i = 1; i <= n; ++i) {
    add_node ("h"s + i, 1, 20*n+4*i+6);
    add_trans ("h"s + i, { "k"s + i });
  }

  // s
  add_node ("s", 0, 20*n+2);
  add_trans ("s", { "x" });
  for (size_t j = 1; j <= n; ++j) {
    add_trans ("s", { "f"s + j });
  }

  // r
  add_node ("r", 0, 20*n+4);
  add_trans ("r", { "x" });
  for (size_t j = 1; j <= n; ++j) {
    add_trans ("r", { "g"s + j });
  }

  // x
  add_node ("x", 1, 1);
  add_trans ("x", { "x" });

  sanity_check (n);
  dump_game ();
}
