#pragma once

#include "Header.hpp"
#include "Symbol.hpp"
#include "PluginSymbol.hpp"

#include <string>
#include <fstream>
#include <vector>

namespace Fas 
{
  using FasSymbols = std::vector<Fas::Symbol>;
  using PluginSymbols = std::vector<PluginSymbol>;
  
  class Core
  {
  public:
    Core ();
    virtual ~Core () = default;
    void load ( const std::string& fileName );
    void load ();
    void open ();
    void loadHeader ();
    void loadFasSymbols ();
    Symbol loadFasSymbol ();
    void deleteUndefinedSymbols ();
    void deleteCannotBeForwardReferenced ();
    void deleteAssemblyTimeVariable ();
    void deleteSpecialMarkers ();
    void deleteNegativeSymbols ();
    void deleteAnonymousSymbols ();
    void loadSymbols ();
    void checkAbsoluteValue ( Fas::Symbol& fasSymbol );
    void loadSymbolFromFasSymbol ( Fas::Symbol& fasSymbol );
    std::string pascal2string ( Fas::Symbol& fasSymbol );
    std::string cstr2string ( Fas::Symbol& fasSymbol );

  private:
    std::ifstream ifs;
    std::string fileName;
    const uint32_t signature;
    Header header;
    FasSymbols fasSymbols;
    PluginSymbols symbols;
  };
} /* namespace Fas */

