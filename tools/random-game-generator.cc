#include <iostream>

#include <boost/algorithm/string/replace.hh>
#include <random>

//////////////////// Parse options as math expressions, using exprTk and cxxopts

template <typename T>
class math_expr;

namespace cxxopts::values {
  template <typename T>
  void parse_value (const std::string& text, math_expr<T>& value) {
    value.set_value_str (text);
  }
}

#include "cxxopts.hh"
#include "exprtk.hh"

template <typename T>
class math_expr {
    static exprtk::symbol_table<double> symbol_table;
  public:
    static std::map<std::string, std::reference_wrapper<math_expr>> all_exprs;

    math_expr (const std::string& name = "",
               const std::string& def = "") : name (name), value_str (def) {
      if (not name.empty ()) {
        symbol_table.add_variable (name, value);
        all_exprs.emplace (name, *this);
      }
    };

    void set_value_str (const std::string& expr) {
      value_str = expr;
    }
    const std::string& get_value_str () const {
      return value_str;
    }

    T get_value () {
      if (set)
        return static_cast<T> (value);

      if (get_called)
        cxxopts::throw_or_mimic<cxxopts::exceptions::incorrect_argument_type> (value_str);

      get_called = true;
      assert (not value_str.empty ());
      std::vector<std::string> dependencies;
      if (value_str.empty ())
        cxxopts::throw_or_mimic<cxxopts::exceptions::option_has_no_value> (name);

      if (not exprtk::collect_variables (value_str, dependencies))
        cxxopts::throw_or_mimic<cxxopts::exceptions::incorrect_argument_type> (value_str);
      for (const auto& var : dependencies)
        all_exprs.at (var).get ().get_value ();

      exprtk::expression<double>   expression;
      exprtk::parser<double>       parser;
      expression.register_symbol_table (symbol_table);
      if (not parser.compile (value_str, expression))
        cxxopts::throw_or_mimic<cxxopts::exceptions::incorrect_argument_type> (value_str);
      value = expression.value ();
      set = true;

      return static_cast<T> (value);
    }

    operator T () {
      return get_value ();
    }

  private:
    std::string name;
    std::string value_str;
    bool        set = false, get_called = false;
    double      value;
};

#define instantiate(V) template <> decltype (V) V {}
instantiate (math_expr<size_t>::symbol_table);
instantiate (math_expr<size_t>::all_exprs);

////////////////////////////////////////////////////////////////////////////////

#define die(S)                                  \
  do {                                          \
  std::cerr << S << "\n";                       \
  exit (2);                                     \
  } while (0)

void usage (const cxxopts::Options& opts, const std::string& err = "") {
  if (not err.empty ())
    std::cout << err << "\n\n";
  std::cout << opts.help () << "\n";
  exit (err.empty () ? 1 : 0);
}

std::string make_filename (const std::string& filename, const cxxopts::ParseResult& options, size_t i) {
  auto res { filename };

  boost::algorithm::replace_all (res, "{i}", std::to_string (i));

  for (const auto& opt : options)
    if (not math_expr<size_t>::all_exprs.contains (opt.key ()))
      boost::algorithm::replace_all (res, "{" + opt.key () + "}", opt.value ());
  for (const auto& [name, val] : math_expr<size_t>::all_exprs)
    boost::algorithm::replace_all (res, "{" + name + "}",
                                   val.get ().get_value_str ());

  return res;
}

int main (int argc, char** argv) {
  using namespace std::string_literals;

  cxxopts::Options opts (argv[0], "Generate random games");

  size_t count = 100;
  math_expr<size_t> size ("size", "100"), maxp ("maxp", "size"),
    edges ("edges", "min (4 * size, size * (size - 1))"),
    outdegree ("outdegree", "undefined"),
    seed ("seed", std::to_string (std::random_device () ()));
  bool energy = false, bipartite = true;

  opts.custom_help ("[OPTIONS...] [FILE-PATTERN]\n"
                    "FILE-PATTERN is a filename that may contain placeholders that correspond\n"
                    "to the options below, and {i} for the current occurrence, e.g.,\n"
                    "          myfolder/game-{size}-{edges}-{i}.pg\n");

  opts.add_options()
    ("h,help", "Print help")

    ("count", "Number of random games (default: " + std::to_string (count) + ")",
     cxxopts::value (count))

    ("seed", "Seed for the random seed generator (default: random)",
     cxxopts::value (seed) )

    ("size", "Size (number of vertices) of each random game (default: " + size.get_value_str () + ")",
     cxxopts::value (size))

    ("maxp", "Maximum priority/weight of a vertex of each random game (default: " + maxp.get_value_str () + ")",
     cxxopts::value (maxp))

    ("edges", "Number of edges of each random game (default: " + edges.get_value_str () + ")",
     cxxopts::value (edges))

    ("outdegree", "Number of out edges per vertex (default: " + outdegree.get_value_str () + ")",
     cxxopts::value (outdegree))

    ("energy", "Energy game, weight can be negative (default: " + std::to_string (energy) + ")",
     cxxopts::value (energy))

    ("bipartite", "Force the generate graph to be bipartite (default: " + std::to_string (bipartite) + ")",
     cxxopts::value (bipartite));

  opts.allow_unrecognised_options();
  auto options = opts.parse(argc, argv);

  if (options.count ("help")) usage (opts);

  std::mt19937 generator (seed);
  auto rnd = [&] (ssize_t lb, ssize_t ub) {
    std::uniform_int_distribution<> distrib (lb, ub);
    return distrib (generator);
  };

  auto unmatched = options.unmatched ();
  if (unmatched.size () > 1) usage (opts, "only one filename pattern may be provided");

  if (options.count ("outdegree") and options.count ("edges"))
    usage (opts, "outdegree and edges connot both be specified.");

  std::streambuf* buf = std::cout.rdbuf ();
  size_t retries = 0;
  bool retry = false;

  for (ssize_t i = 0; i < (ssize_t) count; retry or ++i) {
    if (retry) {
      retry = false;
      if (++retries > size * size)
        die ("games with these parameters are too rare");
    }
    else {
      std::cerr << "\rgame " << i << "... ";
      retries = 0;
    }

    // Generate game
    struct {
        int32_t owner;
        ssize_t prio;
        std::set<size_t> succ;
    } game[size.get_value ()];

    uint8_t has_players = 0b00;
    for (size_t j = 0; j < size; ++j) {
      game[j].owner = rnd (0, 1);
      has_players |= 1 << game[j].owner;
      if (j == size - 1 and has_players != 0b11) // force that the two players are there
        game[j].owner = !game[j].owner;
      game[j].prio = rnd ((energy ? -1 : 0) * maxp, maxp);
    }

    size_t attempts = 0;
    size_t actual_edges = options.count ("outdegree") ? outdegree * size : edges;
    for (size_t j = 0; j < actual_edges; ++j) {
      size_t from, to;
      if (options.count ("outdegree"))
        from = j / outdegree;
      attempts = size * size;
      do {
        attempts--;
        if (not options.count ("outdegree"))
          from = rnd (0, size - 1);
        to = rnd (0, size - 1);
      } while (attempts > 0 and
               (game[from].succ.contains (to) or (bipartite and (game[from].owner == game[to].owner))));
      if (attempts == 0)
        break;
      game[from].succ.insert (to);
    }

    if (attempts == 0) {
      retry = true;
      continue;
    }

    // Dump game
    std::ofstream of;
    if (unmatched.size ()) {
      auto fn = make_filename (unmatched[0], options, i);
      of.open (fn);
      if (not of.good ()) die (fn + ": cannot open file for writing, exiting");
      buf = of.rdbuf ();
    }

    std::ostream out (buf);
    out << "parity " << size << ";\n";

    for (size_t j = 0; j < size; ++j) {
      out << j << " " << game[j].prio << " " << game[j].owner << " ";
      bool first = true;
      for (auto&& s : game[j].succ) {
        out << (first ? "" : ",") << s;
        first = false;
      }
      out << ";\n";
    }
  }
  std::cerr << "\n";
}
