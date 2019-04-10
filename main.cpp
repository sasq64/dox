#include <coreutils/file.h>
#include <coreutils/utils.h>

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

using namespace std::string_literals;

struct parser_exception : public std::exception
{
    parser_exception(std::string const& msg) : message(msg) {}
    std::string message;
};

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

class CppParser
{
    std::string project_dir_;
    CXCompilationDatabase compilation_database_;
    CXIndex index_;
    CXTranslationUnit translation_unit_;

    std::vector<std::string> args_;
    std::unordered_map<std::string, Class> classes;
    std::vector<std::string> ns;
    std::string className;
    CX_CXXAccessSpecifier access = CX_CXXAccessSpecifier::CX_CXXPrivate;

public:
    CppParser(std::string const& project_dir) : project_dir_(project_dir)
    {

        index_ = clang_createIndex(0, 0);
        CXCompilationDatabase_Error cderror;
        compilation_database_ = clang_CompilationDatabase_fromDirectory(
            project_dir.c_str(), &cderror);
        if (cderror != 0)
            throw parser_exception("Could not load compilation database");
    }

    void load(std::string const& source_file)
    {
        std::cout << source_file << "\n";
        auto resolvedPath = resolvePath(source_file.c_str());

        auto compile_commands = clang_CompilationDatabase_getCompileCommands(
            compilation_database_, resolvedPath.c_str());
        auto num_commands = clang_CompileCommands_getSize(compile_commands);
        if (num_commands != 1)
            throw parser_exception("Expected exactly one compilation command");

        auto command = clang_CompileCommands_getCommand(compile_commands, 0);
        auto numArgs = clang_CompileCommand_getNumArgs(command);
        fmt::print("{} args\n", numArgs);
        for (auto i = 0; i < numArgs; i++) {
            CXString argument = clang_CompileCommand_getArg(command, i);
            args_.push_back(clang_getCString(argument));
            clang_disposeString(argument);
        }
        fmt::print("Transforming\n");
        std::vector<const char*> argArray;
        std::transform(args_.begin(), args_.end(), std::back_inserter(argArray),
                       [](auto const& c) { return c.c_str(); });

        translation_unit_ = clang_parseTranslationUnit(
            index_, 0, argArray.data(), numArgs, 0, 0, CXTranslationUnit_None);
    }

    CXChildVisitResult method_visitor(CXCursor c, CXCursor parent)
    {
        int nsLevel = ns.size() * 4;
        auto kind = clang_getCursorKind(c);
        const char* cs = clang_getCString(clang_getCursorSpelling(c));
        std::string name = cs;
        const char* ks = clang_getCString(clang_getCursorKindSpelling(kind));
        int argCount = clang_Cursor_getNumArguments(c);
        auto t = clang_getCursorType(c);
        int fArgs = clang_getNumArgTypes(t);
        int nArgs = clang_Cursor_getNumArguments(c);
        std::string typeName;
        std::string extra;
        if (nArgs >= 1) {
            // auto aType = clang_getArgType(t, 0);
            // CXString typeSpelling = clang_getTypeSpelling(aType);
            // const char* ts = clang_getCString(typeSpelling);
            // typeName = ts;

            auto argCursor = clang_Cursor_getArgument(c, 0);
            if (clang_Cursor_isNull(argCursor)) {
                extra = "null";
            } else if (clang_isInvalidDeclaration(c)) {
                extra = "invalid";
            } else {
                const char* as =
                    clang_getCString(clang_getCursorSpelling(argCursor));
                extra = as;
            }
        }
        fmt::print("{}{} ({}) {} {} {} {}\n", utils::indent("", nsLevel), name,
                   ks, argCount, fArgs, typeName, extra);
        if (kind == CXCursorKind::CXCursor_CXXMethod) {
            fmt::print("{}()\n", utils::indent(name, nsLevel));
            int argCount = clang_Cursor_getNumArguments(c);
            CXType type = clang_getCursorType(c);
            for (int i = 0; i < argCount; i++) {

                auto aType = clang_getArgType(type, i);
                auto argCursor = clang_Cursor_getArgument(c, i);
                const char* cs =
                    clang_getCString(clang_getCursorSpelling(argCursor));
                std::string name = cs;
                CXString typeSpelling = clang_getTypeSpelling(aType);
                const char* ts = clang_getCString(typeSpelling);
                fmt::print("{}..\n", utils::indent(name + " : " + ts, nsLevel));
            }
        } else if (kind == CXCursorKind::CXCursor_ParmDecl) {

        } else {
        }
        return CXChildVisit_Recurse;
    }

    static CXChildVisitResult static_method_visitor(CXCursor c, CXCursor parent,
                                                    CXClientData client_data)
    {
        auto* thiz = static_cast<CppParser*>(client_data);
        return thiz->method_visitor(c, parent);
    }

    CXChildVisitResult visitor(CXCursor c, CXCursor parent)
    {
        auto kind = clang_getCursorKind(c);
        const char* cs = clang_getCString(clang_getCursorSpelling(c));
        std::string name = cs;
        int nsLevel = ns.size() * 4;
        if (kind == CXCursorKind::CXCursor_Namespace) {
            std::cout << utils::indent(name, nsLevel * 4) << "\n";
            ns.push_back(name);
            nsLevel++;
            clang_visitChildren(c, static_visitor, this);
            ns.pop_back();
            nsLevel--;
        } else if (kind == CXCursorKind::CXCursor_ClassDecl) {
            access = CX_CXXAccessSpecifier::CX_CXXPrivate;
            auto nspace = utils::join(ns.begin(), ns.end(), "::"s);
            className = nspace + "::" + name;
            classes[className] = Class{nspace, name};
            std::cout << utils::indent(className, nsLevel * 4) << "\n";
            clang_visitChildren(c, static_method_visitor, this);
        } else if (kind == CXCursorKind::CXCursor_StructDecl) {
            access = CX_CXXAccessSpecifier::CX_CXXPublic;
        }
        return CXChildVisit_Continue;
    }

    static CXChildVisitResult static_visitor(CXCursor c, CXCursor parent,
                                             CXClientData client_data)
    {
        auto* thiz = static_cast<CppParser*>(client_data);
        return thiz->visitor(c, parent);
    }

    void traverse()
    {
        CXCursor cursor = clang_getTranslationUnitCursor(translation_unit_);
        static std::string ns = "";
        static CX_CXXAccessSpecifier access =
            CX_CXXAccessSpecifier::CX_CXXPrivate;
        ns.clear();
        clang_visitChildren(cursor, static_visitor, this);
    }
};

int main(int argc, char** argv)
{

    CppParser parser{"."};
    parser.load(argv[1]);
    parser.traverse();

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

