/*
 * Copyright (c) 2012, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

class ITransport;
class ITransportManager;


//
// Factory for creating new transport instances.
// All transports must register themselves to become available.
// Transports must use unique names.
//

typedef ITransport* (*TransportFactory)(ITransportManager*);

class TransportRegistry
{
public:
    // Register as an available transport.
    template<class TRANSPORT_TYPE>
    static int Register();

    // Create new instance of a particular transport
    static ITransport* New(tstring transportName, ITransportManager* manager);
    
    // Create new instances of all available transports.
    static void NewAll(vector<ITransport*>& all_transports, ITransportManager* manager);

private:
    static map<tstring, TransportFactory> m_registeredTransports;
};
