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

#include "stdafx.h"
#include "transport_registry.h"
#include "transport.h"
#include "vpntransport.h"
#include "sshtransport.h"




/******************************************************************************
 TransportFactory
******************************************************************************/

map<tstring, TransportFactory, TransportRegistry::RegistryEntryComparison> 
    TransportRegistry::m_registeredTransports;

vector<tstring> TransportRegistry::m_registeredTransportsPriority;

bool TransportRegistry::RegistryEntryComparison::operator() (const tstring& lhs, const tstring& rhs) const
{
    vector<tstring>::const_iterator it;
    for (it = m_registeredTransportsPriority.begin();
            it != m_registeredTransportsPriority.end();
            it++)
    {
        // The order of these checks is important. "Strict weak ordering" is 
        // required, so... need to return false if the values are the same?.
        if (*it == rhs)
        {
            return false;
        }
        else if (*it == lhs)
        {
            return true;
        }
    }
    assert(0); // shouldn't get here
    return false;
}


// static
template<class TRANSPORT_TYPE>
int TransportRegistry::Register()
{
    tstring transportName;
    TransportFactory transportFactory;

    TRANSPORT_TYPE::GetFactory(transportName, transportFactory);

    // Map entry ordering comes into play when when we add a new item to the
    // map in the next line. So make sure we add the name to the priority 
    // vector first.
    m_registeredTransportsPriority.push_back(transportName);

    m_registeredTransports[transportName] = transportFactory;

    // The return value is essentially meaningless, but some return value 
    // is needed, so that an assignment can be done -- because only assignments
    // are allowed at the global scope.
    return TransportRegistry::m_registeredTransports.size();
}

// static 
ITransport* TransportRegistry::New(tstring transportName)
{
    return m_registeredTransports[transportName]();
}

// static 
void TransportRegistry::NewAll(vector<ITransport*>& all_transports)
{
    all_transports.clear();

    map<tstring, TransportFactory, RegistryEntryComparison>::const_iterator it;
    for (it = m_registeredTransports.begin(); it != m_registeredTransports.end(); it++)
    {
        all_transports.push_back(it->second());
    }
}

// This is the actual registration of the available transports.
// NOTE: The order of these lines indicates the priority of the transports.

static int _ossh = TransportRegistry::Register<OSSHTransport>();
static int _ssh = TransportRegistry::Register<SSHTransport>();
static int _vpn = TransportRegistry::Register<VPNTransport>();



