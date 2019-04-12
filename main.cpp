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
                    segments.push_back(output);
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
void print_entity(std::ostream& out, const cppast::cpp_entity& e)
{
    // print name and the kind of the entity
    if (!e.name().empty())
        out << e.name();
    else
        out << "<anonymous>";
    out << " (" << cppast::to_string(e.kind()) << ")";

    // print whether or not it is a definition
    if (cppast::is_definition(e))
        out << " [definition]";

    // print number of attributes
    if (!e.attributes().empty())
        out << " [" << e.attributes().size() << " attribute(s)]";

    if (e.kind() == cppast::cpp_entity_kind::language_linkage_t)
        // no need to print additional information for language linkages
        out << '\n';
    else if (e.kind() == cppast::cpp_entity_kind::namespace_t) {
        // cast to cpp_namespace
        auto& ns = static_cast<const cppast::cpp_namespace&>(e);
        // print whether or not it is inline
        if (ns.is_inline())
            out << " [inline]";
        out << '\n';
    } else {
        // print the declaration of the entity
        // it will only use a single line
        // derive from code_generator and implement various callbacks for
        // printing it will print into a std::string
        class code_generator : public cppast::code_generator
        {
            std::string str_; // the result
            bool was_newline_ =
                false; // whether or not the last token was a newline
            // needed for lazily printing them

        public:
            code_generator(const cppast::cpp_entity& e)
            {
                // kickoff code generation here
                cppast::generate_code(*this, e);
            }

            // return the result
            const std::string& str() const noexcept { return str_; }

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
                str_ += tokens.c_str();
            }

            // called when a newline should be generated
            // we're lazy as it will always generate a trailing newline,
            // we don't want
            void do_write_newline() override { was_newline_ = true; }

        } generator(e);
        // print generated code
        out << ": `" << generator.str() << '`' << '\n';
    }
}
class CppParser
{
    std::string project_dir_;
    std::unordered_map<std::string, Class> classes;
    std::vector<std::string> ns;
    std::string className;
    cppast::libclang_compilation_database database_;

public:
    CppParser(std::string const& project_dir)
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
        cppast::visit(
            *file, [&](const cppast::cpp_entity& e, cppast::visitor_info info) {
                if (e.kind() == cppast::cpp_entity_kind::class_t) {
                    fmt::print("{} : {}\n", e.name(), info.event);
                }
                print_entity(std::cout, e);
                return true;
            });
    }

    void test1(size_t abc) {}

    void test2(std::thread const& cde) {}
    void test3(std::vector<int> const& fgh) {}
};

int main(int argc, char** argv)
{

    CppParser parser{"."};
    parser.load(argv[1]);

    sol::state lua;

    lua["print"] = [](std::string const& text) { std::cout << text; };

    auto res = parse(utils::File{argv[1]}.readAll());
    bool isLua = false;
    for (auto segment : res) {
        if (isLua) {
            lua.script(segment);
        } else
            fmt::print("{}", segment);
        isLua = !isLua;
    }

#if 0
    CXIndex index = clang_createIndex(0, 0);

    auto resolvedPath = resolvePath(argv[1]);

    CXCompilationDatabase_Error compilationDatabaseError;
    CXCompilationDatabase compilationDatabase =
        clang_CompilationDatabase_fromDirectory(".", &compilationDatabaseError);
    CXCompileCommands compileCommands =
        clang_CompilationDatabase_getCompileCommands(compilationDatabase,
                                                     resolvedPath.c_str());
    unsigned int numCompileCommands =
        clang_CompileCommands_getSize(compileCommands);

    CXCompileCommand compileCommand =
        clang_CompileCommands_getCommand(compileCommands, 0);
    unsigned int numArguments = clang_CompileCommand_getNumArgs(compileCommand);
    char** arguments = new char*[numArguments];

    for (unsigned int i = 0; i < numArguments; i++) {
        CXString argument = clang_CompileCommand_getArg(compileCommand, i);
        std::string strArgument = clang_getCString(argument);
        arguments[i] = new char[strArgument.size() + 1];

        std::fill(arguments[i], arguments[i] + strArgument.size() + 1, 0);

        std::copy(strArgument.begin(), strArgument.end(), arguments[i]);

        clang_disposeString(argument);
    }

    auto unit = clang_parseTranslationUnit(index, 0, arguments, numArguments, 0,
                                           0, CXTranslationUnit_None);

    /* const char* args[] = { */
    /*     "-std=gnu++14", */
    /*     "-I/opt/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04/include"};
     */
    /* CXTranslationUnit unit = clang_parseTranslationUnit( */
    /*     index, argv[1], args, 2, nullptr, 0, CXTranslationUnit_None); */
    /* if (unit == nullptr) { */
    /*     std::cerr << "Unable to parse translation unit. Quitting." <<
     * std::endl; */
    /*     exit(-1); */
    /* } */

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    static std::string ns = "";
    static CX_CXXAccessSpecifier access = CX_CXXAccessSpecifier::CX_CXXPrivate;
    clang_visitChildren(
        cursor,
        [](CXCursor c, CXCursor parent, CXClientData client_data) {
            auto kind = clang_getCursorKind(c);
            const char* cs = clang_getCString(clang_getCursorSpelling(c));
            std::string name = cs;
            if (kind == CXCursorKind::CXCursor_CXXAccessSpecifier) {
                access = clang_getCXXAccessSpecifier(c);
                if (access == CX_CXXAccessSpecifier::CX_CXXPublic) {
                    std::cout << "public\n";
                } else {
                    std::cout << "private/protected\n";
                }
            }
            if (kind == CXCursorKind::CXCursor_Namespace) {
                std::cout << "namespace " << name << "\n";
            } else if (kind == CXCursorKind::CXCursor_ClassDecl) {
                access = CX_CXXAccessSpecifier::CX_CXXPrivate;
                classes[ns + name] = Class{ns, name};
                std::cout << "class " << name << "\n";
            } else if (kind == CXCursorKind::CXCursor_StructDecl) {
                access = CX_CXXAccessSpecifier::CX_CXXPublic;
                classes[ns + name] = Class{ns, name};
                std::cout << "struct " << name << "\n";
            }

            /* std::cout << "Cursor '" << clang_getCursorSpelling(c) */
            /*           << "' of kind '" */
            /*           <<
             * clang_getCursorKindSpelling(clang_getCursorKind(c))
             */
            /*           << "' inside " << clang_getCursorSpelling(parent)
             * <<
             * '\n'; */
            return CXChildVisit_Recurse;
        },
        nullptr);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
#endif
    return 0;
}

