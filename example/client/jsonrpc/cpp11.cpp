//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "lib/client.hpp"
#include "methods.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/rts/context.hpp>

#include <functional>
#include <iostream>

using namespace boost;
using namespace std::placeholders;

// Ethereum JSON-RPC methods
using namespace methods;

class session
{
    jsonrpc::client client_;

public:
    session(
        asio::io_context& ioc,
        asio::ssl::context& ssl_ctx,
        const rts::context& rts_ctx)
        : client_(
              urls::url("https://ethereum.publicnode.com"),
              rts_ctx,
              ioc.get_executor(),
              ssl_ctx)
    {
    }

    void
    run()
    {
        // Set the user agent
        client_.http_fields().set(
            http_proto::field::user_agent, "Boost.Http.Io");

        // Connect to the endpoint
        client_.async_connect(
            std::bind(&session::on_connect, this, _1));
    }

private:
    void
    on_connect(system::error_code ec)
    {
        if(ec)
            return fail(ec, "connect");

        // Ethereum node's client software and version
        client_[web3_clientVersion](
            std::bind(&session::on_clientVersion, this, _1, _2));
    }

    void
    on_clientVersion(
        jsonrpc::error error, json::string version)
    {
        if(error.code())
            return fail(error);

        std::cout
            << "web3 client:  "
            << version << '\n';

        // Get the latest block number
        client_[eth_blockNumber](
            std::bind(&session::on_blockNumber, this, _1, _2));
    }

    void
    on_blockNumber(
        jsonrpc::error error, json::string block_num)
    {
        if(error.code())
            return fail(error);

        std::cout
            << "Block height: "
            << block_num << '\n';
        
        // Get block details
        client_[eth_getBlockByNumber](
            { block_num, false },
            std::bind(&session::on_getBlockByNumber, this, _1, _2));
    }

    void
    on_getBlockByNumber(
        jsonrpc::error error, json::object block)
    {
        if(error.code())
            return fail(error);

        std::cout<< "Block hash:   " << block["hash"] << '\n';
        std::cout<< "Block size:   " << block["size"] << " Bytes\n";
        std::cout<< "Timestamp:    " << block["timestamp"] << '\n';
        std::cout<< "Transactions: " << block["transactions"].as_array().size() << '\n';

        // Get account balance
        client_[eth_getBalance](
            {
                "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae",
                block["number"]
            },
            std::bind(&session::on_getBalance, this, _1, _2));
    }

    void
    on_getBalance(
        jsonrpc::error error, json::string balance)
    {
        if(error.code())
            return fail(error);

        std::cout
            << "Balance:      "
            << balance << '\n';
        
        // Estimate gas for a transfer
        client_[eth_estimateGas](
            {
                json::object
                {
                    { "from",  "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae" },
                    { "to",    "0x281055afc982d96fab65b3a49cac8b878184cb16" },
                    { "value", "0x2386F26FC10000" } // 0.01 ETH in wei
                }
            },
            std::bind(&session::on_estimateGas, this, _1, _2));
    }

    void
    on_estimateGas(
        jsonrpc::error error, json::string gas_estimate)
    {
        if(error.code())
            return fail(error);

        std::cout
            << "Gas estimate: "
            << gas_estimate << '\n';
    
        // Get the current gas price
        client_[eth_gasPrice](
            std::bind(&session::on_gasPrice, this, _1, _2));
    }

    void
    on_gasPrice(
        jsonrpc::error error, json::string gas_price)
    {
        if(error.code())
            return fail(error);

        std::cout
            << "Gas price:    "
            << gas_price << '\n';

        // Gracefully close the stream
        client_.async_shutdown(
            std::bind(&session::on_shutdown, this, _1));
    }

    void
    on_shutdown(system::error_code ec)
    {
        if(ec && ec != asio::ssl::error::stream_truncated)
            return fail(ec, "shutdown");
    }

    static
    void
    fail(system::error_code ec, const char* operation)
    {
        std::cerr
            << operation << ": "
            << ec.message() << "\n";
    }

    static
    void
    fail(const jsonrpc::error& error)
    {
        std::cerr
            << error.object() << " "
            << error.code().what() << "\n";
    }
};

/*
Sample output:

web3 client:  "erigon/3.0.4/linux-amd64/go1.23.9"
Block height: "0x161a027"
Block hash:   "0xe35f0fb6199e295ff7ba864b034fbbcf3f722b089cf5fe18181f5a83352b2645"
Block size:   "0x1604d" Bytes
Timestamp:    "0x68a47207"
Transactions: 201
Balance:      "0x2647cc23d6974bdb8179"
Gas estimate: "0x5208"
Gas price:    "0x1c9ad53b"
*/

int
main(int, char*[])
{
    try
    {
        // The io_context is required for all I/O
        asio::io_context ioc;

        // The SSL context is required, and holds certificates
        asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);

        // RTS context holds optional deflate and
        // required configuration services
        rts::context rts_ctx;

        // Install parser service
        {
            http_proto::response_parser::config cfg;
            cfg.min_buffer = 64 * 1024;
            http_proto::install_parser_service(rts_ctx, cfg);
        }

        // Install serializer service with default configuration
        http_proto::serializer::config cfg;
        http_proto::install_serializer_service(rts_ctx, cfg);

        // Root certificates used for verification
        ssl_ctx.set_default_verify_paths();

        // Verify the remote server's certificate
        ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

        session s(ioc, ssl_ctx, rts_ctx);
        s.run();

        ioc.run();

        return EXIT_SUCCESS;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
