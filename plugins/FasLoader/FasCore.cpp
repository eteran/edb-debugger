#include "FasCore.hpp"
#include "Exception.hpp"

#include <iostream>

FasCore::FasCore () 
  : signature ( 0x1A736166 )
{
  
}


void 
FasCore::load ( const std::string& fileName ) 
{
  this->fileName = fileName;
  load ();
}

void 
FasCore::load () 
{
  try {
    open ();
    loadHeader ();
  }
  catch ( std::exception& e ) {
    std::cerr << e.what () << std::endl;  
  }
}

void 
FasCore::open () 
{
  ifs.open ( fileName, std::ios::binary );
  if ( !ifs.is_open () ) {
    throw Exception ( "*.fas file not loaded." );
  }
}

void 
FasCore::loadHeader () 
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
