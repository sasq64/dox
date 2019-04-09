#include <clang-c/Index.h>

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

void doSomething() {}

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

int main(int argc, char** argv)
{
    doSomething();
    MyClass mc;
    CXIndex index = clang_createIndex(0, 0);
    const char* args[] = {
        "-std=gnu++14",
        "-I/opt/clang+llvm-8.0.0-x86_64-linux-gnu-ubuntu-18.04/include"};
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, argv[1], args, 2, nullptr, 0, CXTranslationUnit_None);
    if (unit == nullptr) {
        std::cerr << "Unable to parse translation unit. Quitting." << std::endl;
        exit(-1);
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(
        cursor,
        [](CXCursor c, CXCursor parent, CXClientData client_data) {
            std::cout << "Cursor '" << clang_getCursorSpelling(c)
                      << "' of kind '"
                      << clang_getCursorSpelling(clang_getCursorReferenced(c))
                      << " " << clang_getCursorDisplayName(c) << " "
                      << clang_getCursorKindSpelling(clang_getCursorKind(c))
                      << "' inside " << clang_getCursorSpelling(parent) << '\n';
            return CXChildVisit_Recurse;
        },
        nullptr);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return 0;
}

