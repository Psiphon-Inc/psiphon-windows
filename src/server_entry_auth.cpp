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
#include "psiclient.h"
#include "server_entry_auth.h"
#include "cryptlib.h"
#include "cryptlib.h"
#include "rsa.h"
#include "base64.h"
#include "embeddedvalues.h"


bool verifySignedServerList(const char* signedServerList, string& authenticServerList)
{
    // Read the values out of the JSON-formatted signedServerList
    // See psi_ops_server_entry_auth.py for details

    Json::Value json_entry;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(signedServerList, json_entry);
    if (!parsingSuccessful)
    {
        string fail = reader.getFormattedErrorMessages();
        my_print(false, _T("%s: JSON parse failed: %S"), __TFUNCTION__, fail.c_str());
        return false;
    }

    string data;
    string base64Signature;
    string signingPublicKeyDigest;

    try
    {
        data = json_entry.get("data", 0).asString();
        base64Signature = json_entry.get("signature", 0).asString();
        signingPublicKeyDigest = json_entry.get("signingPublicKeyDigest", 0).asString();
    }
    catch (exception& e)
    {
        my_print(false, _T("%s: JSON parse exception: %S"), __TFUNCTION__, e.what());
        return false;
    }

    // Match the presented public key digest against the embedded public key

    string expectedPublicKeyDigest;
    CryptoPP::SHA256 hash;
    CryptoPP::StringSource(
        REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY,
        true,
        new CryptoPP::HashFilter(hash,
            new CryptoPP::Base64Encoder(new CryptoPP::StringSink(expectedPublicKeyDigest), false)));
    if (0 != expectedPublicKeyDigest.compare(signingPublicKeyDigest))
    {
        my_print(false, _T("%s: public key mismatch.  This build must be too old."), __TFUNCTION__);
        return false;
    }

    // Verify the signature of the data and output the data

    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA256>::Verifier verifier(
        CryptoPP::StringSource(
            REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY,
            true,
            new CryptoPP::Base64Decoder()));

    bool result = false;

    string signature;

    CryptoPP::StringSource(
        base64Signature,
        true,
        new CryptoPP::Base64Decoder(new CryptoPP::StringSink(signature)));

    CryptoPP::StringSource(
        string(data) + signature,
        true,
        new CryptoPP::SignatureVerificationFilter(
            verifier,
            new CryptoPP::ArraySink((byte*)&result, sizeof(result)),
            CryptoPP::SignatureVerificationFilter::PUT_RESULT |
            CryptoPP::SignatureVerificationFilter::SIGNATURE_AT_END));

    if (result)
    {
        authenticServerList = data;
    }

    return result;
}
