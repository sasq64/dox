#include <coreutils/file.h>

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/Index.h>

#include <fmt/format.h>
#include <sol2/sol.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

std::ostream& operator<<(std::ostream& stream, const CXString& str)
{
    stream << clang_getCString(str);
    clang_disposeString(str);
    return stream;
}

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

void doSomething() {}

namespace hey {

class MyClass
{
    std::string val;
    void fn() { val = ""; }

public:
    int doSomething(std::string const& arg)
    {
        val = arg;
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

std::unordered_map<std::string, Class> classes;

int main(int argc, char** argv)
{
#if 0
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
#endif
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
    /*     "-I/opt/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04/include"}; */
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
            /*           << clang_getCursorKindSpelling(clang_getCursorKind(c))
             */
            /*           << "' inside " << clang_getCursorSpelling(parent) <<
             * '\n'; */
            return CXChildVisit_Recurse;
        },
        nullptr);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return 0;
}

