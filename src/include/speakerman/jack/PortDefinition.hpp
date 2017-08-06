/*
 * PortDefinition.hpp
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

#ifndef SMS_SPEAKERMAN_PORTDEFINITION_GUARD_H_
#define SMS_SPEAKERMAN_PORTDEFINITION_GUARD_H_

#include <jack/types.h>

#include <tdap/Array.hpp>

#include "Names.hpp"

namespace speakerman {

    using namespace tdap;
    using namespace std;

    enum class PortDirection
    {
        IN, OUT
    };

    enum class PortIsTerminal
    {
        NO, YES
    };

    const char *const port_direction_name(PortDirection direction);

/**
 * Conveniently defines an audio port.
 * Please take into account that this class DOES NOT OWN the name of the port.
 */
    struct PortDefinition
    {
        struct Data
        {
            const char *name;
            PortDirection direction;
            PortIsTerminal terminal;

            unsigned long int flags() const;

            const char *type() const
            { return JACK_DEFAULT_AUDIO_TYPE; }
        };

        static Data validated(Data data);

        const Data data;

        static PortDefinition input(const char *name);

        static PortDefinition output(const char *name);

        PortDefinition terminal_port() const;

        PortDefinition renamed(const char *newName) const;

        PortDefinition(const PortDefinition::Data source);

    private:

        PortDefinition(const char *name, PortDirection direction, PortIsTerminal terminal);
    };

    class PortDefinitions
    {
        Array<PortDefinition::Data> definitions;
        Array<char> nameStorage;

        static size_t validSize(size_t size, size_t maxPorts);

        void addValidated(PortDefinition data);

    public:
        PortDefinitions(size_t maxPorts, size_t nameStorageSize);

        PortDefinitions(size_t maxPorts);

        PortDefinitions();

        PortDefinitions(const PortDefinitions &source, ConstructionPolicy policy);

        size_t portCount() const
        { return definitions.size(); }

        size_t maxPorts() const
        { return definitions.capacity(); }

        int indexOf(const char *name) const;

        int indexOf(const char *name, PortDirection direction) const;

        const char *ensuredNewName(const char *name) const;

        void add(PortDefinition definition);

        void addInput(const char *name);

        void addOutput(const char *name);

        const PortDefinition::Data getByName(const char *name) const;

        const PortDefinition::Data *getByNamePtr(const char *name) const;

        const PortDefinition::Data &operator[](size_t index) const;

        const PortDefinition operator()(size_t index) const;
    };


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_PORTDEFINITION_GUARD_H_ */

