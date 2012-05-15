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
#include "server_entry_auth.h"
#include "cryptlib.h"
#include "cryptlib.h"
#include "rsa.h"
#include "base64.h"
#include "embeddedvalues.h"

// TEMP: test
const char* base64DerPublicKey = "MIICIDANBgkqhkiG9w0BAQEFAAOCAg0AMIICCAKCAgEAsaSgD99AMT98Z3hh44egpL4jBuPg98o8b/dB8aWGl3SodI+ElJ/WCq0aJaEoGJYny3WMfrlMTZGp286HIDplt4cPI+rUtHR7eLfwubJ8gPXk0B6kKs4k/BOXjNp8b9uvdDqMk6d3Bgn7kTFg9zMVeXpqbp1ftFTdu3v3c5xKW/oyTA2E9hOfX59Ns53vZU0qvIStU3NE0W0txoEhwM4YP+Vql2ZqKQDtIX8dp4Zi+pzJcO1OLooCOZoNNtYSKoqJiGjE/yzXisIwcuRcSe2+vIBgF6gCZ2opsHSFmfYOZ0yHVR8RBrG3nWUX5/Rs/ieOdaUD4lRaYib/tXptXGwd3O9xCzof6Z12oHZFPSBmXNce8aQNxm2ymUWuU0h+h6blXcaLbF8InMr7DyF/5d8SkuMM+ZpQD3XDHJaNFaiKQMPmnW+738HXrVmEviNG5Ka5P7cLSN+aRDovv5+nQ+68Y9XQ5H5LNo+sQFDZlcA7DPfFflDMRbUGeSiwa+XBPfWg3gsyddH61YLenvHeW1BM9lCPzAe8KwE4JNT5yzp3eV/qAfTLDDCTuWEMZjUs/iCKEg+6QH6/s2JfXHIIs87LxAodRNaeOAYSUcipKWHngWMVZGtKZ+LprUv8lD4GMCAixCSGfCUt8+pvqj4rYSyFp2l5rD+FDGczJJN0HkGFUNcCAQM=";
const char* data = "abcdef";
const char* base64Signature = "lPSrQhb5ZNnkUkp5u/jFZ9oU+8eTVuIA3PrEXRObNIrygkudlC/QRlNCAegPspUMD9Ajj33fgkPVtWpn60C7l3SQOUsO7IxOeqn3sWS9dDF+DYjZf+qPNFRncbWNkb8O/KMITlM1AwMLebxUSqzQQhmT4RnTqnwtuRSCpd1cG2f/UZkPljnBQHnew7B7GcO1aHOJxmVvdTSamPdR8Ol9ArFY4jserNkagf1mYr8gH2wVolgI//Zmals73Ku+3Yih72/6P4iOAAvCxq8HN+gslml7xTygliNQRqzRKt6oZPxRd5VAkOYjb0lL6VpWZ4BKj6Gt9ie8Q0IT6ScF3WkKw0EAUEjrVBFfxASJJ2JEEYAhzi/6IoFv6vCeocJxhitZzTmeF3A6oS9XgPSzVxzG1s0SICs17dNdH2fbannCy34UoKGGflLOssKxZ8b+nQTwN0Zy2zQPMlwVhWtGGvdc2I4ixEBqYBXMQFQzNPVzAd9dyDhJswJfXzixRJMTLon8Bkp92pDWptGh6+tkpbQKxZMp7VrGjWz0AsR3vEkf6RQjMhHs8Y+2fgkTGaaCpZI9/TYsYSJuTBlZ0RrIbn0VqhzhkQzjgXH6lcRYQLHcLW1/nXYo6oI7pGedK41a56C1bG9DknayNooTU71VQkfPMs2mTRFplqFQiNam2hFlkw8=";

bool verifySignedServerList(const char* signedServerList, vector<string>& authenticServerList)
{
    // TODO: match public key hash

    // TODO: parse JSON

    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA256>::Verifier verifier(
        CryptoPP::StringSource(
            base64DerPublicKey,
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

    return result;
}
