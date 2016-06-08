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
#include "embeddedvalues.h"
#include "utilities.h"

#pragma warning(push, 0)
#pragma warning(disable: 4244)
#include "cryptlib.h"
#include "rsa.h"
#include "base64.h"
#include "gzip.h"
#include "zlib.h"
#pragma warning(pop)


// signedDataPackage may be binary, so we also need the length.
bool verifySignedDataPackage(
    const char* signaturePublicKey,
    const char* signedDataPackage, 
    const size_t signedDataPackageLen,
    bool gzipped, 
    string& authenticDataPackage)
{
    const int SANITY_CHECK_SIZE = 10 * 1024 * 1024;

    authenticDataPackage.clear();

    string jsonString;

    // The package is compressed with either gzip or zip.
    if (gzipped)
    {
        // See https://www.cryptopp.com/wiki/Gunzip#Decompress_to_String_using_Put.2FGet
        CryptoPP::Gunzip unzipper;
        (void)unzipper.Put((const byte*)signedDataPackage, signedDataPackageLen);
        (void)unzipper.MessageEnd();

        auto jsonSize = unzipper.MaxRetrievable();
        if (jsonSize == 0) 
        {
            my_print(NOT_SENSITIVE, false, _T("%s: unzipper.MaxRetrievable returned 0 (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }
        else if (jsonSize > SANITY_CHECK_SIZE)
        {
            my_print(NOT_SENSITIVE, false, _T("%s: Gunzip overflow (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }

        jsonString.resize((size_t)jsonSize);

        unzipper.Get((byte*)&jsonString[0], jsonString.size());
    }
    else // zip compressed
    {
        const int CHUNK_SIZE = 1024;
        int ret;
        z_stream stream;
        char out[CHUNK_SIZE + 1];

        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = signedDataPackageLen;
        stream.next_in = (unsigned char*)signedDataPackage;

        if (Z_OK != inflateInit(&stream))
        {
            my_print(NOT_SENSITIVE, false, _T("%s: inflateInit failed (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }

        auto cleanup = finally([&]() { inflateEnd(&stream); });

        do
        {
            stream.avail_out = CHUNK_SIZE;
            stream.next_out = (unsigned char*)out;
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                my_print(NOT_SENSITIVE, false, _T("%s: inflate failed (%d)"), __TFUNCTION__, GetLastError());
                return false;
            }

            out[CHUNK_SIZE - stream.avail_out] = '\0';

            jsonString += out;

            if (jsonString.length() > SANITY_CHECK_SIZE)
            {
                my_print(NOT_SENSITIVE, false, _T("%s: inflate overflow (%d)"), __TFUNCTION__, GetLastError());
                return false;
            }

        } while (ret != Z_STREAM_END);
    }

    // Read the values out of the JSON-formatted jsonData
    // See psi_ops_server_entry_auth.py for details

    Json::Value json_entry;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString, json_entry);
    if (!parsingSuccessful)
    {
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
        my_print(NOT_SENSITIVE, false, _T("%s: JSON parse exception: %S"), __TFUNCTION__, e.what());
        return false;
    }

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

#pragma warning(push, 0)
#pragma warning(disable: 4239)
    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA256>::Verifier verifier(
        CryptoPP::StringSource(
            signaturePublicKey,
            true,
            new CryptoPP::Base64Decoder()));
#pragma warning(pop)

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
