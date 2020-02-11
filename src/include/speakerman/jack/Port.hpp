/*
 * Port.hpp
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

#ifndef SMS_SPEAKERMAN_PORT_GUARD_H_
#define SMS_SPEAKERMAN_PORT_GUARD_H_

#include "Names.hpp"
#include "PortDefinition.hpp"
#include <jack/types.h>
#include <tdap/Array.hpp>

namespace speakerman {

using namespace tdap;

class PortNames;

class Port {
  static int disconnect_port_internal(jack_client_t *client, jack_port_t *port,
                                      const char *target,
                                      bool throw_if_disconnect_fails);

  static int connect_port_internal(jack_client_t *client, jack_port_t *port,
                                   const char *target,
                                   bool throw_if_disconnect_fails);

public:
  struct BufferFaultResult {
    jack_port_t *port;
    jack_nframes_t frames;
  };

  static size_t max_port_name_length();

  static RefArray<jack_default_audio_sample_t>
  get_buffer(jack_port_t *port, jack_nframes_t frames);

  static jack_port_t *create_port(jack_client_t *client,
                                  PortDefinition::Data definition);

  static jack_port_t *create_port(jack_client_t *client,
                                  PortDefinition definition);

  static void connect_port(jack_client_t *client, jack_port_t *port,
                           const char *target);

  static bool try_connect_port(jack_client_t *client, jack_port_t *port,
                               const char *target, int *result = nullptr);

  static void connect_ports(jack_client_t *client, const char *output,
                            const char *input);

  static bool try_connect_ports(jack_client_t *client, const char *output,
                                const char *input, int *result = nullptr);

  static void disconnect_port_all(jack_client_t *client, jack_port_t *port);

  static bool try_disconnect_port_all(jack_client_t *client, jack_port_t *port,
                                      int *result = nullptr);

  static void disconnect_port(jack_client_t *client, jack_port_t *port,
                              const char *target);

  static bool try_disconnect_port(jack_client_t *client, jack_port_t *port,
                                  const char *target, int *result = nullptr);

  static void unregister_port(jack_client_t *client, jack_port_t *port);

  static bool try_unregister_port(jack_client_t *client, jack_port_t *port,
                                  int *result = nullptr);
};

class Ports;

class Ports {
  struct PortData {
    jack_port_t *port = nullptr;
    RefArray<jack_default_audio_sample_t> buffer;
  };

  PortDefinitions definitions_;
  Array<PortData> ports_;
  volatile bool registered_;

  static NameListPolicy &nameListPolicy();

  void unregister(jack_client_t *client, size_t limit);

  size_t portCountInDirection(PortDirection dir) const;

  size_t totalPortNameLengthInDirection(PortDirection dir) const;

  NameList portsInDirection(PortDirection dir) const;

public:
  Ports(const PortDefinitions &definitions);

  const PortDefinitions &definitions() const { return definitions_; }

  size_t portCount() const { return ports_.size(); }

  size_t inputCount() const;

  size_t outputCount() const;

  NameList inputNames() const;

  NameList outputNames() const;

  const char *portName(size_t i) const;

  void getBuffers(jack_nframes_t frames);

  RefArray<jack_default_audio_sample_t> getBuffer(size_t i) const;

  void registerPorts(jack_client_t *client);

  void unregisterPorts(jack_client_t *client);
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_PORT_GUARD_H_ */
