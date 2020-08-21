#pragma once

// More comments
#include <string>
#include <vector>

// Comment
class MyApi
{
    int priv;
public:
    float open;
    MyApi() = default;

    template <typename t, int n> t otherfn(t const& x) {
        return t(n);
    }

    //! Do stuff to the API
    //!
    //! \param what
    //! \param v
    //!
    void doStuff(int what, std::vector<std::string> const& v);
    void doStuff(std::vector<std::string> const& v);

};

//! Convert to upper case
std::string toUpper(std::string const& txt);