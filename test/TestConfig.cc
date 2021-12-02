//
// Created by michel on 01-12-21.
//

#include <iostream>
#include "TestConfig.hh"
#include <speakerman/utils/Config.hpp>

typedef speakerman::ConfigValue<size_t> SizeConfigValue;
typedef speakerman::ConfigValue<char *> TextConfigValue;
typedef speakerman::ConfigValue<char [15]> StringConfigValue;

void testConfig() {
  std::cout << "Value of a StringConfigValue is " << sizeof(StringConfigValue) << std::endl;
}
