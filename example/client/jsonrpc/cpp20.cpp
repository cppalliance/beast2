//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "jsonrpc/client.hpp"
#include "eth_methods.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rts/context.hpp>

#include <iostream>

using namespace boost;

asio::awaitable<void>
co_main(
    asio::ssl::context& ssl_ctx,
    const rts::context& rts_ctx)
{
    using dec_float = multiprecision::cpp_dec_float_50;
    const auto to_int = [](std::string_view s)
    {
        return multiprecision::cpp_int(s);
    };

    jsonrpc::client client(
        urls::url("https://ethereum.publicnode.com"),
        rts_ctx,
        co_await asio::this_coro::executor,
        ssl_ctx);

    // Connect to the endpoint
    co_await client.async_connect();

    // Bring Ethereum methods into scope
    using namespace eth_methods;

    // Get Ethereum node client software and version
    std::cout
        << "web3 client:  "
        << co_await client[web3_clientVersion]() << '\n';

    // Get the latest block number
    auto block_num = co_await client[eth_blockNumber]();
    std::cout
        << "Block number: "
        << to_int(block_num) << '\n';

    // Get block details
    auto block = co_await client[eth_getBlockByNumber]({ block_num, false });
    std::cout<< "Block hash:   " << block["hash"] << '\n';
    std::cout<< "Block size:   " << to_int(block["size"].as_string()) << " Bytes\n";
    std::cout<< "Timestamp:    " << to_int(block["timestamp"].as_string()) << '\n';
    std::cout<< "Transactions: " << block["transactions"].as_array().size() << '\n';

    // Exammple addresses
    auto addr1 = "0xde0b295669a9fd93d5f28d9ec85e40f4cb697bae";
    auto addr2 = "0x281055afc982d96fab65b3a49cac8b878184cb16";

    // Get account balance
    auto balance = co_await client[eth_getBalance]({ addr1, block_num });
    std::cout
        << "Balance:      "
        << std::setprecision(18) << dec_float(to_int(balance)) / dec_float("1e18")
        << " ETH\n";

    // Estimate gas for a transfer
    auto gas_estimate = co_await client[eth_estimateGas](
        {
            json::object
            {
                { "from",  addr1 },
                { "to",    addr2 },
                { "value", "0x2386F26FC10000" } // 0.01 ETH in wei
            }
        });
    std::cout
        << "Gas estimate: "
        << to_int(gas_estimate) << '\n';

    // Get the current gas price
    auto gas_price = co_await client[eth_gasPrice]();
    std::cout
        << "Gas price:    "
        << std::setprecision(3) << dec_float(to_int(gas_price)) / dec_float("1e10")
        << " GWEI\n";

    // Gracefully close the stream
    auto [ec] = co_await client.async_shutdown(asio::as_tuple);
    if(ec && ec != asio::ssl::error::stream_truncated)
        throw system::system_error(ec);
}

/*
Sample output:

web3 client:  "erigon/3.0.4/linux-amd64/go1.23.9"
Block number: 23175159
Block hash:   "0xd476f0a0dd29be4d63bc737937f6c40b024906f1a864ee38aa1a03f91ce55efd"
Block size:   87166 Bytes
Timestamp:    1755606983
Transactions: 176
Balance:      180774.35501503312 ETH
Gas estimate: 21000
Gas price:    0.0337 GWEI
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

        asio::co_spawn(
            ioc,
            co_main(ssl_ctx, rts_ctx),
            [](auto eptr)
            {
                if(eptr)
                    std::rethrow_exception(eptr);
            });

        ioc.run();

        return EXIT_SUCCESS;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
