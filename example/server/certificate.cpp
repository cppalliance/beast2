//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "certificate.hpp"
#include <string>
#include <utility>

namespace boost {
namespace beast2 {

void
load_server_certificate(
    asio::ssl::context& ctx)
{
/*
    Using Windows with OpenSSL version "1.1.1s  1 Nov 2022"

    1. Generate a Root CA

        openssl genrsa -out rootCA.key 4096
        openssl req -x509 -new -nodes -key rootCA.key -sha256 -days 10000 -out rootCA.pem -subj "//CN=Boost Test Root CA"

    2. Create a Server Key

        openssl genrsa -out server.key 2048
*/
    using keypair = std::pair<std::string, std::string>;

    // certificates for SSL listening ports
    keypair const certs[] = {
{
/*  Create a Signed Server Certificate with the Test Root CA

    openssl req -new -key server.key -out server.csr -subj "//CN=localhost"
    openssl x509 -req -in server.csr -CA rootCA.pem -CAkey rootCA.key -CAcreateserial -out server.crt -days 10000 -sha256 -extfile san.cnf
*/
    "-----BEGIN CERTIFICATE-----\n"
    "MIID2TCCAcGgAwIBAgIURQ6waOrVlt/YykIgwb+46o0UtCUwDQYJKoZIhvcNAQEL\n"
    "BQAwHTEbMBkGA1UEAwwSQm9vc3QgVGVzdCBSb290IENBMCAXDTI1MTAxMjAxMjIx\n"
    "N1oYDzIwNTMwMjI3MDEyMjE3WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0G\n"
    "CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDkHGuSplDQrW5f0JJwUK9UivHZMkE5\n"
    "CDEThsUwjrqubofLpR49EzfAcBRWWQ1R0QmzK2sKnKQnku4IliFyitw/OAHsJrr4\n"
    "c3OHXfpOOwtd4Kg3BP3P3oeAsO+IELrQIsJp/mrjOJKtBVTZ8kl5ZYrf94fEMivn\n"
    "JnZ+neb2kPiSPTnAtFSBVSQc9aHU7Wg1gtQkUuEIkjUBvPzxGKi0m3nuZfUDJpev\n"
    "2OB7fRftIPjiqZ/1n1k2CYLqLMBIXAeeYAjBgzM0x4UG3SW7jlPeoDI34XQ7dYxQ\n"
    "K5jjs3OhoLs5x0za1sZ7MXkDRAqO5Cgeg3kNb5VlhjVzR8Njtapx4mXtAgMBAAGj\n"
    "GDAWMBQGA1UdEQQNMAuCCWxvY2FsaG9zdDANBgkqhkiG9w0BAQsFAAOCAgEAjEHn\n"
    "AIfxiYXWBsVPtYsRbHzWYoNSIWGkzwauMaDPDGzeMV1ajKV1dBp8NHKBg/jlKdaQ\n"
    "vGiKHLlPwSGRlDIyyglG1qsH7unQHV0w+cCh19Uoc0gtv9q4zTUuyDhk8eufIdEr\n"
    "1SPNMIqJQ47A2KrYo31rd+HxnyoCit1fM8SUWwM0tLaoEM3iTF33LI5CBkT2VPbv\n"
    "qJL/68qQFDeTIUGQJjPK9rs/cElqsweVfWF+O2Mn9wA9aJyc5+0jsOGrfADO/cI1\n"
    "OA1HXgawSQjXKST74aprd0gBxACXrG+wA1G6NNsawp80xjADSTyCfNxtQVvY1W2n\n"
    "9kJuionCnDUnhqEOnhnZdq1XikoOYMRJP0BxFHX8SNbxmJouJRWvusmh91lAgbQv\n"
    "JVXlHcEQGYMIYIr5y6dLlZ55SamC9aK3vO+q8s9AEYZO4OTUi2WQ2bfrxcn3vI/0\n"
    "UzLh8LzE1A2OT5Gin0jWKbeKpqXH2RAfgaEAf1bzDA92xMEjVGBrBoXagvUOGzMG\n"
    "cDcj4WHzcO6BWENPkRZ2JNNIxIZRK/wr9Mw/q/Yu9iDNVw4XOEx+iGjh1cRHy1FH\n"
    "VuS/N5CQCBnmhDKYrZ3Lz+D/l0CGc30A1jjfvlrDiE4k9NyrV8wVHqmO73vivAvW\n"
    "hYG9wLo13LnWG9JrtT8Drl/H0YbuL4C46n/mua8=\n"
    "-----END CERTIFICATE-----\n",

    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpgIBAAKCAQEA5BxrkqZQ0K1uX9CScFCvVIrx2TJBOQgxE4bFMI66rm6Hy6Ue\n"
    "PRM3wHAUVlkNUdEJsytrCpykJ5LuCJYhcorcPzgB7Ca6+HNzh136TjsLXeCoNwT9\n"
    "z96HgLDviBC60CLCaf5q4ziSrQVU2fJJeWWK3/eHxDIr5yZ2fp3m9pD4kj05wLRU\n"
    "gVUkHPWh1O1oNYLUJFLhCJI1Abz88RiotJt57mX1AyaXr9jge30X7SD44qmf9Z9Z\n"
    "NgmC6izASFwHnmAIwYMzNMeFBt0lu45T3qAyN+F0O3WMUCuY47NzoaC7OcdM2tbG\n"
    "ezF5A0QKjuQoHoN5DW+VZYY1c0fDY7WqceJl7QIDAQABAoIBAQCfXyvZPdHgugsP\n"
    "bk2hov2cd6cZNH9VNV/0YIiMsGvFSvwdT7OcwDyHesb6vSUNMJsyTvduZppZ+9HK\n"
    "tfmQaWwPzzWopDalNyRUQ1iKJ759TGS6bAZYoQTS6MuxqN6cZGyoWVScg/4WXE84\n"
    "JosnAcbRS8PTU6pQyRKoy/F9+zNwF30B0GwQpNwEZMX7QvPQPoUIcurE8HmP7NhB\n"
    "fuZn/af6q9N+D56fOaD7Rg4kdtBMa0cv4mKojaq3ymx5RK8ccPTDiXC0idg7uRVC\n"
    "13sFf9H46Z7kIimyhPUrwTapMyrx1kjQNZa97YYBAHGfLwRoYsGu5XMWIXKDkZHz\n"
    "Bt2U92iBAoGBAPJRdRqoHdZIe42rq1kEpIis4Eitn4idjuW6D8kgzPU8IT7UreT/\n"
    "eS5c7ThnbwI3B966pHWzAPMiktc3sXQUIXlBy1vu1A6paqU+HpA9pbwB3tHfLsgm\n"
    "akabqfv/nWpzKJt5kLlbNOkkCvJ8FvotyeCCrdCeip+q6d8NZc7s9YQZAoGBAPD9\n"
    "m5nUHKR7tcE8SNc9pcBp1AoILuhp9XbUgP2YkePO8KfnHPUSU5LIT0c1sasm1k/w\n"
    "ehpaqirjSAjYkjKQTfwj4SVP9LAlCb5kz0rhZJqdiQUtSK3oPIDzcXfutgZjRbyX\n"
    "vR9r3a2DLoYJ/IxuajyvICImKSMHEySnCoxaEgr1AoGBALI2VGC5edAp2Kx1r/w1\n"
    "HOjj88Of5a+s6PZtY8SxGevWQEEcW5QKi84cS97qu0quvFwDeoaRksY+DC66aAkN\n"
    "8Rxj1jMTr+Pkl2lWCVZd8HEYEw7ZDGfpUMoDG/4YnWY3sYq+2kBoIr7AYki6GJAA\n"
    "cvNqSHkg0KTjJ0ODb/fCcEKpAoGBALUX3rXaDywLSqnLA3G7gbL108E2JQnBlhOV\n"
    "3Ni0rezitTV3FuuSufqzS9/XGYvjw2iO7TKgrv9Li/YZyML2baPr0mSXkOhM7OWG\n"
    "G7/JYDBP8YdSYCtPOSgtyDa3y1FBiEYQQK48AHlC+tL+7ikZT/wKHbuLsZ4A0wHY\n"
    "BLUzehuBAoGBAMC9mGnaYU4D9PjCqC96MyEnZjdpf5eGByx5GhV6O6kEx0wJbjPc\n"
    "kq+QAn1cp1tVjPcNfOK62LxV5E5t4eCv/Su5dmQHWGVDZBrdug5osyLFaFZA8Lw/\n"
    "GKCs22hqgjPqSrGnCXMV6bZkwvy3cmxNkWuEiiA67PYyc6aFpTcOj8fi\n"
    "-----END RSA PRIVATE KEY-----\n"
}
    };

/*  More secure Diffie-Helman parameters

    openssl dhparam -out dh.pem 2048
*/
    std::string const dh =
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIBCAKCAQEAu7R9qRNtiuayUH9FLFIIQJ9GmhKpdL/gcLG8+5/6x+RN+cgPwQgQ\n"
    "FYqTtIHRgINxtxdZqUxnrcg6jbW13r7b8A7uWURsrW5T3Hy68v4SFY5F+c/a97m+\n"
    "LyUHW12iwCqZPlwdl4Zvb/uAtrn3xjvl3Buea4nGPAlTlHVKR1OH8IuWPnxUvjXp\n"
    "slcI5c20LQ3Z2znM3csLNGkgiGKIPLCb9Sq8Zx1+gCDQk9DjDC4K8ELDqvbwDz8m\n"
    "760pgC5eQ0z1lgmxvRVgPZOx9twwO1/VhpISpGnb7vihEb+06jQtXZIC3LrANfhe\n"
    "bnbac08nYv9yt7Caf2Zfy1UDvkeLtPYs2wIBAg==\n"
    "-----END DH PARAMETERS-----\n";
    
    ctx.set_password_callback(
        [](std::size_t,
            asio::ssl::context_base::password_purpose)
        {
            return "test";
        });

    ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::single_dh_use);

    for(auto const& t : certs)
    {
        ctx.use_certificate_chain(asio::buffer(t.first));

        // use_private_key applies to the last inserted certificate,
        // see: https://linux.die.net/man/3/ssl_ctx_use_privatekey
        //
        ctx.use_private_key(asio::buffer(t.second),
            asio::ssl::context::file_format::pem);
    }

    ctx.use_tmp_dh(asio::buffer(dh));
}

} // beast2
} // boost
