#include "test_api.h"

#include <coreutils/file.h>
#include <coreutils/utils.h>

#include <fmt/format.h>
#include <sol2/sol.hpp>

#include <cppast/code_generator.hpp>  // for generate_code()
#include <cppast/cpp_entity_kind.hpp> // for the cpp_entity_kind definition
#include <cppast/cpp_forward_declarable.hpp> // for is_definition()
#include <cppast/cpp_namespace.hpp>          // for cpp_namespace
#include <cppast/libclang_parser.hpp> // for libclang_parser, libclang_compile_config, cpp_entity,...
#include <cppast/visitor.hpp> // for visit()

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::string_literals;

struct parser_exception : public std::exception
{
    parser_exception(std::string const& msg) : message(msg) {}
    std::string message;
};

#ifdef __unix__
#    include <cppast/cpp_function_template.hpp>
#    include <cppast/cpp_member_function.hpp>
#    include <limits.h>
#    include <stdlib.h>
#endif

std::string resolvePath(const char* path)
{
    std::string resolvedPath;

#ifdef __unix__
    char* resolvedPathRaw = new char[PATH_MAX];
    char* result = realpath(path, resolvedPathRaw);

    if (result)
        resolvedPath = resolvedPathRaw;

    delete[] resolvedPathRaw;
#else
    resolvedPath = path;
#endif

    return resolvedPath;
}

namespace hey {

struct SomeType
{};

class MyClass
{
    std::string val;
    void fn() { val = ""; }

public:
    double doSomething(SomeType const& arg)
    {
        // val = arg;
        return 2;
    }
};
} // namespace hey

std::vector<std::string> parse(std::string const& doc)
{
    std::string output;
    std::vector<std::string> segments;
    int inQuotes = 0;
    int curly = 0;
    int lastChar = 0;
    for (auto c : doc) {
        if (inQuotes) {
            if (c == inQuotes && lastChar != '\\')
                inQuotes = 0;
        } else if (curly > 0) {
            if (c == '}') {
                if (lastChar == '@')
                    curly = 1;
                curly--;
                if (curly == 0) {
                    segments.push_back(output.substr(0,output.length()-1));
                    output = "";
                    continue;
                }
            } else if (c == '{')
                curly++;
            else if (c == '\'' || c == '\"') {
                inQuotes = c;
            }
        } else {
            if (c == '{' && lastChar == '@') {
                curly = 1;
                segments.push_back(output.substr(0, output.length() - 1));
                output = "";
                continue;
            }
        }
        output += c;
        lastChar = c;
    }
    if(!output.empty()) {
        segments.push_back(output);
    }
    return segments;
}

struct Var
{
    std::string name;
    std::string type;
};

struct Method
{
    std::string name;
    std::vector<Var> params;
};

struct Class
{
    Class() = default;
    Class(std::string const& ns, std::string const& name) : name(name), ns(ns)
    {}
    std::string name;
    std::string ns;
    std::vector<Method> methods;
    std::vector<Var> fields;
};

// prints the AST entry of a cpp_entity (base class for all entities),
// will only print a single line
std::string print_entity(cppast::cpp_entity const& e)
{
    // print the declaration of the entity
    // it will only use a single line
    // derive from code_generator and implement various callbacks for
    // printing it will print into a std::string
    class code_generator : public cppast::code_generator
    {
        std::string str_;
        bool was_newline_ = false;

    public:
        explicit code_generator(const cppast::cpp_entity& e)
        {
            cppast::generate_code(*this, e);
        }

        // return the result
        [[nodiscard]] const std::string& str() const noexcept { return str_; }

    private:
        // called to retrieve the generation options of an entity
        generation_options
        do_get_options(const cppast::cpp_entity&,
                       cppast::cpp_access_specifier_kind) override
        {
            // generate declaration only
            return code_generator::declaration;
        }

        // no need to handle indentation, as only a single line is used
        void do_indent() override {}
        void do_unindent() override {}

        // called when a generic token sequence should be generated
        // there are specialized callbacks for various token kinds,
        // to e.g. implement syntax highlighting
        void do_write_token_seq(cppast::string_view tokens) override
        {
            if (was_newline_) {
                // lazily append newline as space
                str_ += ' ';
                was_newline_ = false;
            }
            // append tokens
            if (";"s != tokens.c_str()) {
                str_ += tokens.c_str();
            }
        }

        // called when a newline should be generated
        // we're lazy as it will always generate a trailing newline,
        // we don't want
        void do_write_newline() override { was_newline_ = true; }

    } generator(e);
    // print generated code
    return generator.str();
}

class CppParser
{
    std::string project_dir_;
    std::unordered_map<std::string, Class> classes;
    std::vector<std::string> ns;
    std::string className;
    cppast::libclang_compilation_database database_;

public:
    explicit CppParser(std::string const& project_dir)
        : project_dir_(project_dir), database_(project_dir)
    {}

    void load(std::string const& source_file)
    {
        std::cout << source_file << "\n";
        auto resolvedPath = resolvePath(source_file.c_str());
        auto config = cppast::libclang_compile_config(database_, resolvedPath);
        cppast::stderr_diagnostic_logger logger;
        logger.set_verbose(true);

        // Index is used to resolve cross references in the AST
        // we don't need that, so it will not be needed afterwards
        cppast::cpp_entity_index idx;
        // the parser is used to parse the entity
        // there can be multiple parser implementations
        cppast::libclang_parser parser(type_safe::ref(logger));
        // parse the file
        auto file = parser.parse(idx, resolvedPath, config);
        if (parser.error())
            throw parser_exception("Could not parse");
        cppast::visit(*file, [&](const cppast::cpp_entity& e,
                                 cppast::visitor_info info) {
            if (e.kind() == cppast::cpp_entity_kind::class_t) {
                fmt::print("Found class {} : {}\n", e.name(), info.event);
            }
            if (e.kind() == cppast::cpp_entity_kind::function_template_t) {

                fmt::print("Found Template\n");
                 auto const& tf =
                  dynamic_cast<cppast::cpp_function_template const&>(e);
                for (auto const& param : tf.parameters()) {
                    fmt::print("T: {} : ({})\n", param.name(),
                               param.comment() ? param.comment().value().c_str() : "");
                }
                cppast::visit(tf, [&](const cppast::cpp_entity& e,
                                         cppast::visitor_info info) {
                    fmt::print("T: {} -- {}\n", e.name(), print_entity(e));
                });

            }
            if (e.kind() == cppast::cpp_entity_kind::member_function_t) {
                fmt::print("Found member function\n");
                auto const& mf =
                    dynamic_cast<cppast::cpp_member_function const&>(e);

                for (auto const& param : mf.parameters()) {
                    fmt::print("{} : ({})", param.name(),
                               param.comment() ? param.comment().value().c_str()
                                               : "");
                    cppast::visit(param, [&](const cppast::cpp_entity& e,
                                             cppast::visitor_info info) {
                        fmt::print("{} -- {}\n", e.name(), print_entity(e));
                    });
                }
            }
            if (auto cr = e.comment()) {
                fmt::print("\n{}\n", cr.value());
            }
            fmt::print("[{}] {}\n", (int)e.kind(), print_entity(e));
            return true;
        });
    }
};

int main(int argc, char** argv)
{

    std::string currDir = ".";
    sol::state lua;

    lua["print"] = [](std::string const& text) { std::cout << text; };

    lua["set_dir"] = [&](std::string const& fn) {
        currDir = fn;
    };

    lua["load_source"] = [&](std::string const& fn) {
        auto parser = std::make_shared<CppParser>(currDir);
        parser->load(fn);
        return parser;
    };

    auto res = parse(utils::File{argv[1]}.readAll());
    bool isLua = false;
    for (auto const& segment : res) {
        if (isLua) {
            lua.script(segment);
        } else
            std::cout << segment;
        isLua = !isLua;
    }

    return 0;
}
