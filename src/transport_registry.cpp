/*
 * Copyright (c) 2015, Psiphon Inc.
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
#include "logging.h"
#include "transport.h"
#include "vpntransport.h"
#include "coretransport.h"
#include "serverlist.h"
#include "psiclient.h"


/******************************************************************************
 TransportFactory
******************************************************************************/

vector<RegisteredTransport> TransportRegistry::m_registeredTransports;


// static
template<class TRANSPORT_TYPE>
int TransportRegistry::Register()
{
    RegisteredTransport registeredTransport;
    TRANSPORT_TYPE::GetFactory(
                        registeredTransport.transportDisplayName, 
                        registeredTransport.transportProtocolName, 
                        registeredTransport.transportFactoryFn,
                        registeredTransport.addServerEntriesFn);

    m_registeredTransports.push_back(registeredTransport);

    // The return value is essentially meaningless, but some return value 
    // is needed, so that an assignment can be done -- because only assignments
    // are allowed at the global scope.
    return TransportRegistry::m_registeredTransports.size();
}


// static 
ITransport* TransportRegistry::New(tstring transportProtocolName)
{
    for (vector<RegisteredTransport>::const_iterator it = m_registeredTransports.begin();
         it != m_registeredTransports.end();
         ++it)
    {
        if (it->transportProtocolName == transportProtocolName)
        {
            return it->transportFactoryFn();
        }
    }

    assert(FALSE);
    return NULL;
}


// static 
void TransportRegistry::NewAll(vector<ITransport*>& all_transports)
{
    all_transports.clear();

    for (vector<RegisteredTransport>::const_iterator it = m_registeredTransports.begin();
         it != m_registeredTransports.end();
         ++it)
    {
        all_transports.push_back(it->transportFactoryFn());
    }
}


// static
void TransportRegistry::AddServerEntries(
                            const vector<string>& newServerEntryList, 
                            const ServerEntry* serverEntry)
{
    tstringstream results;
    results << _T("Discovered new Psiphon servers: ");

    bool discovered = false;

    for (vector<RegisteredTransport>::const_iterator it = m_registeredTransports.begin();
         it != m_registeredTransports.end();
         ++it)
    {
        size_t newEntries = it->addServerEntriesFn(it->transportProtocolName.c_str(), newServerEntryList, serverEntry);

        if (newEntries > 0)
        {
            if (discovered && it != m_registeredTransports.begin())
            {
                results << _T("; ");
            }

            results << newEntries << _T(" for ") << it->transportDisplayName;

            discovered = true;
        }
    }

    if (discovered)
    {
        my_print(NOT_SENSITIVE, false, results.str().c_str());
    }
}


// This is the actual registration of the available transports.
// NOTE: The order of these lines indicates the priority of the transports.

static int _coreTransport = TransportRegistry::Register<CoreTransport>();
static int _vpn = TransportRegistry::Register<VPNTransport>();
