/*
 * PortDefinition.cpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jack/jack.h>

#include <speakerman/jack/ErrorHandler.hpp>
#include <speakerman/jack/PortDefinition.hpp>

namespace speakerman {

using namespace tdap;
using namespace std;

const char *const port_direction_name(PortDirection direction) {
  switch (direction) {
  case PortDirection::IN:
    return "IN";
  case PortDirection::OUT:
    return "OUT";
  default:
    return "Unknown";
  }
}

unsigned long int PortDefinition::Data::flags() const {
  return (direction == PortDirection::OUT ? JackPortFlags::JackPortIsOutput
                                          : JackPortFlags::JackPortIsInput) |
         (terminal == PortIsTerminal::YES ? JackPortFlags::JackPortIsTerminal
                                          : 0);
}

PortDefinition::Data PortDefinition::validated(Data data) {
  Names::valid_port(data.name);

  return data;
}

PortDefinition PortDefinition::input(const char *name) {
  return PortDefinition(Names::valid_port(name), PortDirection::IN,
                        PortIsTerminal::NO);
}

PortDefinition PortDefinition::output(const char *name) {
  return PortDefinition(Names::valid_port(name), PortDirection::OUT,
                        PortIsTerminal::NO);
}

PortDefinition PortDefinition::terminal_port() const {
  return PortDefinition(data.name, data.direction, PortIsTerminal::YES);
}

PortDefinition PortDefinition::renamed(const char *newName) const {
  return PortDefinition(newName, data.direction, data.terminal);
}

PortDefinition::PortDefinition(const PortDefinition::Data source)
    : data(validated(source)){};

PortDefinition::PortDefinition(const char *name, PortDirection direction,
                               PortIsTerminal terminal)
    : data(Data{name, direction, terminal}) {
  data.flags();
}

size_t PortDefinitions::validSize(size_t size, size_t maxPorts) {
  if (size > maxPorts) {
    size_t p = Count<char>::product(size, maxPorts);
    if (p > 0) {
      return p;
    }
  }
  throw std::invalid_argument("Invalid size for name storage");
}

void PortDefinitions::addValidated(PortDefinition data) {
  size_t ports = portCount();
  if (ports >= maxPorts()) {
    throw std::runtime_error("Too many ports");
  }
  size_t length = strnlen(data.data.name, Names::get_port_size());
  size_t size = length + 1;
  size_t offset = nameStorage.size();
  size_t newSize = offset + size;
  nameStorage.setSize(newSize);
  char *copy = nameStorage + offset;
  strncpy(copy, data.data.name, Names::get_port_size());
  copy[offset + length] = 0;
  definitions.setSize(ports + 1);
  definitions[ports].name = copy;
  definitions[ports].direction = data.data.direction;
  definitions[ports].terminal = data.data.terminal;
}

PortDefinitions::PortDefinitions(size_t maxPorts, size_t nameStorageSize)
    : definitions(maxPorts, 0),
      nameStorage(validSize(nameStorageSize, maxPorts), 0) {}

PortDefinitions::PortDefinitions(size_t maxPorts)
    : PortDefinitions(maxPorts, maxPorts * 32) {}

PortDefinitions::PortDefinitions() : PortDefinitions(16) {}

PortDefinitions::PortDefinitions(const PortDefinitions &source,
                                 ConstructionPolicy policy)
    : definitions(source.definitions, policy),
      nameStorage(source.nameStorage, policy) {
  const char *sourceNames = source.nameStorage + 0;
  const char *destinationNames = nameStorage + 0;
  for (size_t i = 0; i < portCount(); i++) {
    definitions[i].name =
        destinationNames + (source.definitions[i].name - sourceNames);
  }
}

int PortDefinitions::indexOf(const char *name) const {
  for (size_t i = 0; i < definitions.size(); i++) {
    if (strncasecmp(name, definitions[i].name, Names::get_port_size()) == 0) {
      return i;
    }
  }
  return -1;
}

int PortDefinitions::indexOf(const char *name, PortDirection direction) const {
  for (size_t i = 0; i < portCount(); i++) {
    if (strncasecmp(name, definitions[i].name, Names::get_port_size()) == 0) {
      if (definitions[i].direction == direction) {
        return i;
      }
    }
  }
  return -1;
}

const char *PortDefinitions::ensuredNewName(const char *name) const {
  if (indexOf(name) < 0) {
    return name;
  }
  string message = "Port name already in use: '";
  message += name;
  message += "'";
  throw std::invalid_argument(message);
}

void PortDefinitions::add(PortDefinition definition) {
  ensuredNewName(definition.data.name);
  addValidated(definition);
}

void PortDefinitions::addInput(const char *name) {
  add(PortDefinition::input(name));
}

void PortDefinitions::addOutput(const char *name) {
  add(PortDefinition::output(name));
}

const speakerman::PortDefinition::Data
PortDefinitions::getByName(const char *name) const {
  int idx = indexOf(name);
  if (idx >= 0) {
    return operator[](idx);
  }
  const char *nm = Names::valid_port(name);
  string message = "Have no port with name: '";
  message += nm;
  message += "'";
  throw invalid_argument(message);
}

const speakerman::PortDefinition::Data *
PortDefinitions::getByNamePtr(const char *name) const {
  int idx = indexOf(name);
  return idx < 0 ? nullptr : &operator[](idx);
}

const speakerman::PortDefinition::Data &
    PortDefinitions::operator[](size_t index) const {
  return definitions[index];
}

const speakerman::PortDefinition
PortDefinitions::operator()(size_t index) const {
  return PortDefinition(definitions[index]);
}

} /* End of namespace speakerman */
