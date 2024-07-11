#include "aurora_pch.h"

int main()
{
  Aurora::Log::Init();
  AR_CORE_INFO("Initialized Log!");
  int a = 5;
  AR_TRACE("variable: a={0}", a);
    std::cout << "hello world!" << std::endl;
  std::cin.get();
    Aurora::print_hello();
}
