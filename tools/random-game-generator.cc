#include <iostream>

#include <boost/algorithm/string/replace.hpp>

#include <mpreal.h>

using mpfr::mpreal;

#include <boost/multiprecision/gmp.hpp>
#include <boost/random.hpp>

#include <random>

//////////////////// Parse options as math expressions, using exprTk and cxxopts

class mpfr_math_expr;
class long_math_expr;

namespace cxxopts::values {
  void parse_value (const std::string& text, mpfr_math_expr& value);
  void parse_value (const std::string& text, long_math_expr& value);
}

#include "cxxopts.hh"
#include "exprtk_mpfr_adaptor.hh"
#include "exprtk.hh"

class mpfr_math_expr {
    static exprtk::symbol_table<mpreal> symbol_table;
  public:
    static std::map<std::string, std::reference_wrapper<mpfr_math_expr>> all_exprs;

    mpfr_math_expr (const std::string& name = "",
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

  protected:
    mpreal get_value () {
      if (set)
        return value;

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

      exprtk::expression<mpreal>   expression;
      exprtk::parser<mpreal>       parser;
      expression.register_symbol_table (symbol_table);
      if (not parser.compile (value_str, expression))
        cxxopts::throw_or_mimic<cxxopts::exceptions::incorrect_argument_type> (value_str);
      value = expression.value ();
      set = true;
      return value;
    }
  public:
    mpreal operator* () {
      return get_value ();
    }

    operator mpreal () {
      return *(*this);
    }
  private:
    std::string name;
    std::string value_str;
    bool        set = false, get_called = false;
    mpreal      value;
};

class long_math_expr : public mpfr_math_expr {
  public:
    long_math_expr (const std::string& name = "",
                    const std::string& def = "") : mpfr_math_expr (name, def) { }

    long operator* () {
      return get_value ().toLong ();
    }

    operator long () {
      return *(*this);
    }
};

void cxxopts::values::parse_value (const std::string& text, mpfr_math_expr& value) {  value.set_value_str (text); }
void cxxopts::values::parse_value (const std::string& text, long_math_expr& value) {  value.set_value_str (text); }

#define instantiate(V) decltype (V) V {}
instantiate (mpfr_math_expr::symbol_table);
instantiate (mpfr_math_expr::all_exprs);


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

std::string make_filename (const std::string& filename, const cxxopts::ParseResult& options, long i) {
  auto res { filename };

  boost::algorithm::replace_all (res, "{i}", std::to_string (i));

  for (const auto& opt : options)
    if (not long_math_expr::all_exprs.contains (opt.key ()))
      boost::algorithm::replace_all (res, "{" + opt.key () + "}", opt.value ());
  for (const auto& [name, val] : mpfr_math_expr::all_exprs)
    boost::algorithm::replace_all (res, "{" + name + "}",
                                   val.get ().get_value_str ());

  return res;
}

int main (int argc, char** argv) {
  using namespace std::string_literals;

  cxxopts::Options opts (argv[0], "Generate random games");

  long count = 100;
  long_math_expr size ("size", "100"),
    edges ("edges", "min (4 * size, size * (size - 1))"),
    outdegree ("outdegree", "undefined"),
    seed ("seed", std::to_string (std::random_device () ()));
  mpfr_math_expr maxp ("maxp", "size");

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
  auto rnd = [&] (long lb, long ub) {
    std::uniform_int_distribution distrib (lb, ub);
    return distrib (generator);
  };

  mpfr::random (seed);
  auto rnd_mpfr = [&] (mpreal lb, mpreal ub) {
    return mpfr::random () * (ub - lb + 1) + lb;
  };

  auto unmatched = options.unmatched ();
  if (unmatched.size () > 1) usage (opts, "only one filename pattern may be provided");

  if (options.count ("outdegree") and options.count ("edges"))
    usage (opts, "outdegree and edges connot both be specified.");

  std::streambuf* buf = std::cout.rdbuf ();
  long retries = 0;
  bool retry = false;

  for (long i = 0; i < (long) count; retry or ++i) {
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
    struct vert {
        int32_t owner;
        mpreal prio;
        std::set<long> succ;
    };
    auto game = new vert[*size];

    uint8_t has_players = 0b00;
    for (long j = 0; j < size; ++j) {
      game[j].owner = rnd (0, 1);
      has_players |= 1 << game[j].owner;
      if (j == size - 1 and has_players != 0b11) // force that the two players are there
        game[j].owner = !game[j].owner;
      game[j].prio = rnd_mpfr ((energy ? -1 : 0) * *maxp, *maxp);
    }

    long attempts = 0;
    long actual_edges = options.count ("outdegree") ? outdegree * size : edges;
    for (long j = 0; j < actual_edges; ++j) {
      long from, to;
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

    for (long j = 0; j < size; ++j) {
      out << j << " " << game[j].prio.toString ("%34.0RNf") << " " << game[j].owner << " ";
      bool first = true;
      for (auto&& s : game[j].succ) {
        out << (first ? "" : ",") << s;
        first = false;
      }
      out << ";\n";
    }
    delete[] game;
  }
  std::cerr << "\n";
}
