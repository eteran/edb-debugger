#pragma once

#include "Header.hpp"

#include <string>

class FasCore
{
public:
  FasCore ();
  virtual ~FasCore () = default;
  void load ( const std::string& fileName );

private:
  const uint32_t signature;
  Header header;
};
