/*
 * Copyright (c) 2013, Psiphon Inc.
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
#include "logging.h"
#include "psiclient.h"
#include "authenticated_data_package.h"
#include "cryptlib.h"
#include "cryptlib.h"
#include "rsa.h"
#include "base64.h"
#include "embeddedvalues.h"
#include "gzip.h"


// signedDataPackage may be binary, so we also need the length.
bool verifySignedDataPackage(
    const char* signaturePublicKey,
    const char* signedDataPackage, 
    const size_t signedDataPackageLen,
    bool compressed, 
    string& authenticDataPackage)
{
    char* jsonData = NULL;
    size_t jsonSize = 0;
    bool jsonDataAllocated = false;

    // The package may be compressed.
    if (compressed)
    {
        CryptoPP::Gunzip unzipper;
        size_t n = unzipper.Put((const byte*)signedDataPackage, signedDataPackageLen);
        unzipper.MessageEnd();

        jsonSize = (size_t)unzipper.MaxRetrievable();

        // The null terminator is probably in the compressed data, but to be safe...
        jsonData = new char[jsonSize+1];

        unzipper.Get((byte*)jsonData, jsonSize);
        jsonData[jsonSize] = '\0';

        jsonDataAllocated = true;
    }
    else
    {
        jsonData = (char*)signedDataPackage;
        jsonSize = signedDataPackageLen;
        jsonDataAllocated = false;
    }

    // Read the values out of the JSON-formatted jsonData
    // See psi_ops_server_entry_auth.py for details

    Json::Value json_entry;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonData, jsonData+jsonSize, json_entry);
    if (!parsingSuccessful)
    {
        if (jsonDataAllocated) delete[] jsonData;
        string fail = reader.getFormattedErrorMessages();
        my_print(NOT_SENSITIVE, false, _T("%s: JSON parse failed: %S"), __TFUNCTION__, fail.c_str());
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
        if (jsonDataAllocated) delete[] jsonData;
        my_print(NOT_SENSITIVE, false, _T("%s: JSON parse exception: %S"), __TFUNCTION__, e.what());
        return false;
    }

    if (jsonDataAllocated) delete[] jsonData;

    // Match the presented public key digest against the embedded public key

    string expectedPublicKeyDigest;
    CryptoPP::SHA256 hash;
    CryptoPP::StringSource(
        signaturePublicKey,
        true,
        new CryptoPP::HashFilter(hash,
            new CryptoPP::Base64Encoder(new CryptoPP::StringSink(expectedPublicKeyDigest), false)));
    if (0 != expectedPublicKeyDigest.compare(signingPublicKeyDigest))
    {
        my_print(NOT_SENSITIVE, false, _T("%s: public key mismatch.  This build must be too old."), __TFUNCTION__);
        return false;
    }

    // Verify the signature of the data and output the data

    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA256>::Verifier verifier(
        CryptoPP::StringSource(
            signaturePublicKey,
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
        authenticDataPackage = data;
    }

    return result;
}
