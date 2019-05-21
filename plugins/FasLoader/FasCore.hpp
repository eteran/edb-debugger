#pragma once

#include "Header.hpp"
#include "Symbol.hpp"

#include <string>
#include <fstream>

class FasCore
{
public:
  FasCore ();
  virtual ~FasCore () = default;
  void load ( const std::string& fileName );
  void load ();
  void open ();
  void loadHeader ();

private:
  std::ifstream ifs;
  std::string fileName;
  const uint32_t signature;
  Header header;
};
