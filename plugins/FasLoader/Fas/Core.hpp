#pragma once

#include "Header.hpp"
#include "Symbol.hpp"

#include <string>
#include <fstream>
#include <vector>

namespace Fas 
{
  using Symbols = std::vector<Symbol>;
  
  class Core
  {
  public:
    Core ();
    virtual ~Core () = default;
    void load ( const std::string& fileName );
    void load ();
    void open ();
    void loadHeader ();
    void loadSymbols ();
    Symbol loadSymbol ();
    void deleteUndefinedSymbols ();

  private:
    std::ifstream ifs;
    std::string fileName;
    const uint32_t signature;
    Header header;
    Symbols symbols;
  };
} /* namespace Fas */

