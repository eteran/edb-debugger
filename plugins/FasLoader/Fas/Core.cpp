#include "Core.hpp"
#include "Exception.hpp"

#include <iostream>

using namespace Fas;

Core::Core () 
  : signature ( 0x1A736166 )
{
  
}


void 
Core::load ( const std::string& fileName ) 
{
  this->fileName = fileName;
  load ();
}

void 
Core::load () 
{
  try {
    open ();
    loadHeader ();
    loadSymbols ();
  }
  catch ( std::exception& e ) {
    std::cerr << e.what () << std::endl;  
  }
}

void 
Core::open () 
{
  ifs.open ( fileName, std::ios::binary );
  if ( !ifs.is_open () ) {
    throw Exception ( "*.fas file not loaded." );
  }
}

void 
Core::loadHeader () 
{
  ifs.seekg (0);
  ifs.read ( (char*)&header, sizeof ( Header ) );
  if ( !ifs.good () ) {
    throw Exception ( "*.fas Header not loaded." );
  }
  if ( header.signature != signature ) {
    throw Exception ( "*.fas signature fail" );
  }
  if ( header.lengthOfHeader != 64 ) {
    throw Exception ( "*.fas header size not supported" );
  }
}

void 
Core::loadSymbols () 
{
  ifs.seekg ( header.offsetOfSymbolsTable );

  auto size = header.lengthOfSymbolsTable;
  auto count = size/sizeof ( Symbol );
  symbols.resize ( count );
  for ( auto symbol : symbols ) {
    symbol = loadSymbol ();
  }
}

Symbol
Core::loadSymbol () 
{
  Symbol symbol;
  ifs.read ( (char*)&symbol, sizeof ( Symbol ) );
  if ( !ifs.good () ) {
    throw Exception ( "*.fas symbol not loaded" );
  }

  return std::move ( symbol );
}

void 
Core::deleteUndefinedSymbols () 
{
  auto it = std::begin ( symbols );
  while ( it != std::end ( symbols ) ) {
    if ( !it->wasDefined ) {
      it = symbols.erase ( it );
    }
    else {
      ++it;
    }
  }
}
