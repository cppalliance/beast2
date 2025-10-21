//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_EXAMPLE_CLIENT_JSONRPC_METHODS_HPP
#define BOOST_BEAST2_EXAMPLE_CLIENT_JSONRPC_METHODS_HPP

#include "jsonrpc/method.hpp"

#include <boost/json/array.hpp>
#include <boost/json/string.hpp>

using namespace boost;
template<typename Signature>
using method = jsonrpc::method<Signature>;

/// Ethereum JSON-RPC endpoint methods
namespace eth_methods
{
BOOST_INLINE_CONSTEXPR method<json::string()> web3_clientVersion = "web3_clientVersion";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> web3_sha3 = "web3_sha3";
BOOST_INLINE_CONSTEXPR method<json::string()> net_version = "net_version";
BOOST_INLINE_CONSTEXPR method<bool()> net_listening = "net_listening";
BOOST_INLINE_CONSTEXPR method<json::string()> net_peerCount = "net_peerCount";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_protocolVersion = "eth_protocolVersion";
BOOST_INLINE_CONSTEXPR method<json::object()> eth_syncing = "eth_syncing";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_coinbase = "eth_coinbase";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_chainId = "eth_chainId";
BOOST_INLINE_CONSTEXPR method<bool()> eth_mining = "eth_mining";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_hashrate = "eth_hashrate";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_gasPrice = "eth_gasPrice";
BOOST_INLINE_CONSTEXPR method<json::array()> eth_accounts = "eth_accounts";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_blockNumber = "eth_blockNumber";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getBalance = "eth_getBalance";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getStorageAt = "eth_getStorageAt";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getTransactionCount = "eth_getTransactionCount";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getBlockTransactionCountByHash = "eth_getBlockTransactionCountByHash";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getBlockTransactionCountByNumber = "eth_getBlockTransactionCountByNumber";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getUncleCountByBlockHash = "eth_getUncleCountByBlockHash";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getUncleCountByBlockNumber = "eth_getUncleCountByBlockNumber";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_getCode = "eth_getCode";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_sign = "eth_sign";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_signTransaction = "eth_signTransaction";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_sendTransaction = "eth_sendTransaction";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_sendRawTransaction = "eth_sendRawTransaction";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_call = "eth_call";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_estimateGas = "eth_estimateGas";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getBlockByHash = "eth_getBlockByHash";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getBlockByNumber = "eth_getBlockByNumber";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getTransactionByHash = "eth_getTransactionByHash";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getTransactionByBlockHashAndIndex = "eth_getTransactionByBlockHashAndIndex";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getTransactionByBlockNumberAndIndex = "eth_getTransactionByBlockNumberAndIndex";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getTransactionReceipt = "eth_getTransactionReceipt";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getUncleByBlockHashAndIndex = "eth_getUncleByBlockHashAndIndex";
BOOST_INLINE_CONSTEXPR method<json::object(json::array)> eth_getUncleByBlockNumberAndIndex = "eth_getUncleByBlockNumberAndIndex";
BOOST_INLINE_CONSTEXPR method<json::string(json::array)> eth_newFilter = "eth_newFilter";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_newBlockFilter = "eth_newBlockFilter";
BOOST_INLINE_CONSTEXPR method<json::string()> eth_newPendingTransactionFilter = "eth_newPendingTransactionFilter";
BOOST_INLINE_CONSTEXPR method<bool(json::string)> eth_uninstallFilter = "eth_uninstallFilter";
BOOST_INLINE_CONSTEXPR method<json::array(json::array)> eth_getFilterChanges = "eth_getFilterChanges";
BOOST_INLINE_CONSTEXPR method<json::array(json::array)> eth_getFilterLogs = "eth_getFilterLogs";
BOOST_INLINE_CONSTEXPR method<json::array(json::array)> eth_getLogs = "eth_getLogs";
}

#endif
