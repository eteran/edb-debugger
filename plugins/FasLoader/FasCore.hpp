#pragma once

#include <string>

class FasCore
{
public:
  FasCore () = default;
  virtual ~FasCore () = default;
  void load ( const std::string& fileName );

protected:
};
