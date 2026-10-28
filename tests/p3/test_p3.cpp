// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// -------------------------------------------------------------- registry tests

TEST(reg_null_rejected) {
    ToolRegistry r;
    std::unique_ptr<Tool> empty;
    CHECK(!r.add(std::move(empty)));
    CHECK(r.size() == 0);
    CHECK(r.find("anything") == nullptr);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));
    std::vector<std::string> names;
    r.for_each([&](const Tool& t) { names.push_back(t.name()); });
    std::sort(names.begin(), names.end());
    CHECK(names.size() == 3);
    CHECK(names[0] == "alpha"); CHECK(names[1] == "beta"); CHECK(names[2] == "gamma");
}

TEST(reg_load_factor_and_bucket_count) {
    ToolRegistry r;
    CHECK(r.size() == 0); CHECK(r.bucket_count() >= 1); CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    REQUIRE(r.add(std::make_unique<NamedTool>("one")));
    CHECK(r.load_factor() <= 1.0);
}

TEST(reg_hash_uses_order_and_characters) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_123") != ToolRegistry::hash("tool_124"));
    std::vector<std::size_t> hashes;
    hashes.reserve(10000);
    for (int i = 0; i < 10000; ++i) hashes.push_back(ToolRegistry::hash("tool_" + std::to_string(i)));
    std::sort(hashes.begin(), hashes.end());
    CHECK(static_cast<std::size_t>(std::unique(hashes.begin(), hashes.end()) - hashes.begin()) >= 9990);
}

TEST(reg_10000_tools_and_alternative_hash) {
    ToolRegistry r;
    Tool* first = nullptr;
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        if (i == 0) first = tool.get();
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(r.size() == 10000); CHECK(r.find("tool_0") == first);
    for (int i = 0; i < 10000; ++i) CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    CHECK(r.max_probe_length() <= 64);
    std::vector<int> weak_counts(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak_hash = static_cast<unsigned char>(name[0]) + name.size();
        ++weak_counts[weak_hash % r.bucket_count()];
    }
    const int weak_longest = *std::max_element(weak_counts.begin(), weak_counts.end());
    std::cout << "P3 scale: buckets=" << r.bucket_count() << ", load_factor=" << r.load_factor()
              << ", max_chain=" << r.max_probe_length() << ", weak_hash_longest_chain=" << weak_longest << '\n';
}

// --------------------------------------------------------------- workspace tests

TEST(ws_accepts_and_normalizes_relative_paths) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason)); CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason)); CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("a/../b", &out, &reason)); CHECK(out == ws.root() / "b");
}

TEST(ws_dot_resolves_to_root) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve(".", &out, &reason)); CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/..", &out, &reason)); CHECK(out == ws.root());
}

TEST(ws_accepts_nonexistent_and_double_dot_filename) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason)); CHECK(out == ws.root() / "new" / "dir" / "file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason)); CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_rejects_empty_path) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("", &out, &reason)); CHECK(reason == "empty path");
}

TEST(ws_rejects_nul_byte) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    const std::string path = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(path, &out, &reason)); CHECK(!reason.empty());
}

TEST(ws_rejects_absolute_path) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason)); CHECK(!reason.empty());
}

TEST(ws_rejects_parent_escape_variants) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    for (const std::string path : {"..", "../etc/passwd", "a/../../b", "notes/../../ws/notes/todo.txt"}) {
        CHECK(!ws.resolve(path, &out, &reason)); CHECK(!reason.empty());
    }
}

// ---------------------------------------------------------------- calculator tests

TEST(calc_left_to_right_subtraction_and_division) {
    double v = 0; std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err)); CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err)); CHECK(v == 2);
}

TEST(calc_parentheses_and_mixed_precedence) {
    double v = 0; std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err)); CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err)); CHECK(v == 9);
}

TEST(calc_negative_numbers_and_decimals) {
    double v = 0; std::string err;
    REQUIRE(eval("-3 * -3", &v, &err)); CHECK(v == 9);
    REQUIRE(eval("7 / 2", &v, &err)); CHECK(v == 3.5);
    REQUIRE(eval("1.25 + 2.5", &v, &err)); CHECK(v == 3.75);
}

TEST(calc_parenthesized_division_by_zero) {
    double v = 0; std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err)); CHECK(err == "division by zero");
}

TEST(calc_long_expression_is_iterative) {
    std::string expr = "1";
    for (int i = 0; i < 20000; ++i) expr += "+1";
    double v = 0; std::string err;
    REQUIRE(eval(expr, &v, &err)); CHECK(v == 20001);
}

TEST(calc_deep_parentheses_are_iterative) {
    std::string expr(5000, '('); expr += "2"; expr += std::string(5000, ')');
    double v = 0; std::string err;
    REQUIRE(eval(expr, &v, &err)); CHECK(v == 2);
}


int main(int argc, char** argv) { return mt::run_all(argc, argv); }
