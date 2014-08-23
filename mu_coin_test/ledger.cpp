#include <gtest/gtest.h>
#include <mu_coin/mu_coin.hpp>
#include <cryptopp/filters.h>
#include <cryptopp/randpool.h>

#include <thread>
#include <condition_variable>

TEST (ledger, empty)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::address address;
    auto balance (ledger.account_balance (address));
    ASSERT_TRUE (balance.is_zero ());
}

TEST (ledger, genesis_balance)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 500);
    genesis.initialize (store);
    auto balance (ledger.account_balance (key1.pub));
    ASSERT_EQ (500, balance);
}

TEST (system, system_genesis)
{
    mu_coin::system system (1, 24000, 25000, 2, 500);
    for (auto & i: system.clients)
    {
        ASSERT_EQ (500, i->client_m->ledger.account_balance (system.test_genesis_address.pub));
    }
}

TEST (uint256_union, key_encryption)
{
    mu_coin::keypair key1;
    mu_coin::uint256_union secret_key;
    secret_key.bytes.fill (0);
    mu_coin::uint256_union encrypted (key1.prv, secret_key, key1.pub.owords [0]);
    mu_coin::private_key key4 (encrypted.prv (secret_key, key1.pub.owords [0]));
    ASSERT_EQ (key1.prv, key4);
    mu_coin::public_key pub;
    ed25519_publickey (key4.bytes.data (), pub.bytes.data ());
    ASSERT_EQ (key1.pub, pub);
}

TEST (ledger, process_send)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send));
    ASSERT_EQ (50, ledger.account_balance (key1.pub));
    mu_coin::frontier frontier2;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier2));
    auto latest6 (store.block_get (frontier2.hash));
    ASSERT_NE (nullptr, latest6);
    auto latest7 (dynamic_cast <mu_coin::send_block *> (latest6.get ()));
    ASSERT_NE (nullptr, latest7);
    ASSERT_EQ (send, *latest7);
    mu_coin::open_block open;
    open.hashables.source = hash1;
    mu_coin::block_hash hash2 (open.hash ());
    mu_coin::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (open));
    ASSERT_EQ (50, ledger.account_balance (key2.pub));
    mu_coin::frontier frontier3;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier3));
    auto latest2 (store.block_get (frontier3.hash));
    ASSERT_NE (nullptr, latest2);
    auto latest3 (dynamic_cast <mu_coin::send_block *> (latest2.get ()));
    ASSERT_NE (nullptr, latest3);
    ASSERT_EQ (send, *latest3);
    mu_coin::frontier frontier4;
    ASSERT_FALSE (store.latest_get (key2.pub, frontier4));
    auto latest4 (store.block_get (frontier4.hash));
    ASSERT_NE (nullptr, latest4);
    auto latest5 (dynamic_cast <mu_coin::open_block *> (latest4.get ()));
    ASSERT_NE (nullptr, latest5);
    ASSERT_EQ (open, *latest5);
	ledger.rollback (hash2);
	mu_coin::frontier frontier5;
	ASSERT_TRUE (ledger.store.latest_get (key2.pub, frontier5));
	ASSERT_FALSE (ledger.store.pending_get (hash1));
	ASSERT_EQ (0, ledger.account_balance (key2.pub));
	ASSERT_EQ (50, ledger.account_balance (key1.pub));
    mu_coin::frontier frontier6;
	ASSERT_FALSE (ledger.store.latest_get (key1.pub, frontier6));
	ASSERT_EQ (hash1, frontier6.hash);
	ledger.rollback (frontier6.hash);
    mu_coin::frontier frontier7;
	ASSERT_FALSE (ledger.store.latest_get (key1.pub, frontier7));
	ASSERT_EQ (frontier1.hash, frontier7.hash);
	ASSERT_TRUE (ledger.store.pending_get (hash1));
	ASSERT_EQ (100, ledger.account_balance (key1.pub));
}

TEST (ledger, process_receive)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
	ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send));
    mu_coin::open_block open;
    open.hashables.source = hash1;
    mu_coin::block_hash hash2 (open.hash ());
    mu_coin::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (open));
	mu_coin::send_block send2;
	send2.hashables.balance = 25;
	send2.hashables.previous = hash1;
	send2.hashables.destination = key2.pub;
    mu_coin::block_hash hash3 (send2.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash3, send2.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send2));
	mu_coin::receive_block receive;
	receive.hashables.previous = hash2;
	receive.hashables.source = hash3;
	auto hash4 (receive.hash ());
	mu_coin::sign_message (key2.prv, key2.pub, hash4, receive.signature);
	ASSERT_EQ (mu_coin::process_result::progress, ledger.process (receive));
	ASSERT_EQ (hash4, ledger.latest (key2.pub));
	ASSERT_EQ (25, ledger.account_balance (key1.pub));
	ASSERT_EQ (75, ledger.account_balance (key2.pub));
	ledger.rollback (hash4);
	ASSERT_EQ (25, ledger.account_balance (key1.pub));
	ASSERT_EQ (50, ledger.account_balance (key2.pub));
	ASSERT_EQ (hash2, ledger.latest (key2.pub));
	ASSERT_FALSE (ledger.store.pending_get (hash3));
}

TEST (ledger, rollback_receiver)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
	ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send));
    mu_coin::open_block open;
    open.hashables.source = hash1;
    mu_coin::block_hash hash2 (open.hash ());
    mu_coin::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (open));
	ASSERT_EQ (hash2, ledger.latest (key2.pub));
	ASSERT_EQ (50, ledger.account_balance (key1.pub));
	ASSERT_EQ (50, ledger.account_balance (key2.pub));
	ledger.rollback (hash1);
	ASSERT_EQ (100, ledger.account_balance (key1.pub));
	ASSERT_EQ (0, ledger.account_balance (key2.pub));
	mu_coin::frontier frontier2;
	ASSERT_TRUE (ledger.store.latest_get (key2.pub, frontier2));
	ASSERT_TRUE (ledger.store.pending_get (frontier2.hash));
}

TEST (ledger, process_duplicate)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send));
    ASSERT_EQ (mu_coin::process_result::old, ledger.process (send));
    mu_coin::open_block open;
    open.hashables.source = hash1;
    mu_coin::block_hash hash2 (open.hash ());
    mu_coin::sign_message(key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (open));
    ASSERT_EQ (mu_coin::process_result::old, ledger.process (open));
}

TEST (processor_service, bad_send_signature)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.previous = frontier1.hash;
    send.hashables.balance = 50;
    send.hashables.destination = key1.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
    send.signature.bytes [32] ^= 0x1;
    ASSERT_EQ (mu_coin::process_result::bad_signature, ledger.process (send));
}

TEST (processor_service, bad_receive_signature)
{
    mu_coin::keypair key1;
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::genesis genesis (key1.pub, 100);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block send;
    mu_coin::keypair key2;
    send.hashables.previous = frontier1.hash;
    send.hashables.balance = 50;
    send.hashables.destination = key2.pub;
    mu_coin::block_hash hash1 (send.hash ());
    mu_coin::sign_message (key1.prv, key1.pub, hash1, send.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (send));
    mu_coin::frontier frontier2;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier2));
    mu_coin::receive_block receive;
    receive.hashables.source = hash1;
    receive.hashables.previous = key2.pub;
    mu_coin::block_hash hash2 (receive.hash ());
    receive.sign (key2.prv, key2.pub, hash2);
    receive.signature.bytes [32] ^= 0x1;
    ASSERT_EQ (mu_coin::process_result::bad_signature, ledger.process (receive));
}

TEST (processor_service, empty)
{
    mu_coin::processor_service service;
    std::thread thread ([&service] () {service.run ();});
    service.stop ();
    thread.join ();
}

TEST (processor_service, one)
{
    mu_coin::processor_service service;
    std::atomic <bool> done (false);
    std::mutex mutex;
    std::condition_variable condition;
    service.add (std::chrono::system_clock::now (), [&] ()
    {
        std::lock_guard <std::mutex> lock (mutex);
        done = true;
        condition.notify_one ();
    });
    std::thread thread ([&service] () {service.run ();});
    std::unique_lock <std::mutex> unique (mutex);
    condition.wait (unique, [&] () {return !!done;});
    service.stop ();
    thread.join ();
}

TEST (processor_service, many)
{
    mu_coin::processor_service service;
    std::atomic <int> count (0);
    std::mutex mutex;
    std::condition_variable condition;
    for (auto i (0); i < 50; ++i)
    {
        service.add (std::chrono::system_clock::now (), [&] ()
                 {
                     std::lock_guard <std::mutex> lock (mutex);
                     count += 1;
                     condition.notify_one ();
                 });
    }
    std::vector <std::thread> threads;
    for (auto i (0); i < 50; ++i)
    {
        threads.push_back (std::thread ([&service] () {service.run ();}));
    }
    std::unique_lock <std::mutex> unique (mutex);
    condition.wait (unique, [&] () {return count == 50;});
    service.stop ();
    for (auto i (threads.begin ()), j (threads.end ()); i != j; ++i)
    {
        i->join ();
    }
}

TEST (processor_service, top_execution)
{
    mu_coin::processor_service service;
    int value (0);
    std::mutex mutex;
    std::unique_lock <std::mutex> lock1 (mutex);
    service.add (std::chrono::system_clock::now (), [&] () {value = 1; service.stop (); lock1.unlock ();});
    service.add (std::chrono::system_clock::now () + std::chrono::milliseconds (1), [&] () {value = 2; service.stop (); lock1.unlock ();});
    service.run ();
    std::unique_lock <std::mutex> lock2 (mutex);
    ASSERT_EQ (1, value);
}

TEST (ledger, representative_genesis)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::address address;
    mu_coin::genesis genesis (address);
    genesis.initialize (store);
    ASSERT_EQ (address, ledger.representative (ledger.latest (address)));
}

TEST (ledger, weight)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::address address;
    mu_coin::genesis genesis (address);
    genesis.initialize (store);
    ASSERT_EQ (std::numeric_limits <mu_coin::uint256_t>::max (), ledger.weight (address));
}

TEST (ledger, representative_change)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::keypair key1;
    mu_coin::keypair key2;
    mu_coin::genesis genesis (key1.pub);
    genesis.initialize (store);
    ASSERT_EQ (std::numeric_limits <mu_coin::uint256_t>::max (), ledger.weight (key1.pub));
    ASSERT_EQ (0, ledger.weight (key2.pub));
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::change_block block;
    block.hashables.representative = key2.pub;
    block.hashables.previous = frontier1.hash;
    mu_coin::sign_message (key1.prv, key1.pub, block.hash (), block.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block));
    ASSERT_EQ (0, ledger.weight (key1.pub));
    ASSERT_EQ (std::numeric_limits <mu_coin::uint256_t>::max (), ledger.weight (key2.pub));
	mu_coin::frontier frontier2;
	ASSERT_FALSE (store.latest_get (key1.pub, frontier2));
	ASSERT_EQ (block.hash (), frontier2.hash);
	ledger.rollback (frontier2.hash);
	mu_coin::frontier frontier3;
	ASSERT_FALSE (store.latest_get (key1.pub, frontier3));
	ASSERT_EQ (frontier1.hash, frontier3.hash);
	ASSERT_EQ (std::numeric_limits <mu_coin::uint256_t>::max (), ledger.weight (key1.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
}

TEST (ledger, send_fork)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::keypair key1;
    mu_coin::keypair key2;
    mu_coin::keypair key3;
    mu_coin::genesis genesis (key1.pub);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block block;
    block.hashables.destination = key2.pub;
    block.hashables.previous = frontier1.hash;
    block.hashables.balance = 100;
    mu_coin::sign_message (key1.prv, key1.pub, block.hash (), block.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block));
    mu_coin::send_block block2;
    block2.hashables.destination = key3.pub;
    block2.hashables.previous = frontier1.hash;
    block2.hashables.balance.clear ();
    mu_coin::sign_message (key1.prv, key1.pub, block2.hash (), block2.signature);
    ASSERT_EQ (mu_coin::process_result::fork, ledger.process (block2));
}

TEST (ledger, receive_fork)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::keypair key1;
    mu_coin::keypair key2;
    mu_coin::keypair key3;
    mu_coin::genesis genesis (key1.pub);
    genesis.initialize (store);
    mu_coin::frontier frontier1;
    ASSERT_FALSE (store.latest_get (key1.pub, frontier1));
    mu_coin::send_block block;
    block.hashables.destination = key2.pub;
    block.hashables.previous = frontier1.hash;
    block.hashables.balance = 100;
    mu_coin::sign_message (key1.prv, key1.pub, block.hash (), block.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block));
    mu_coin::open_block block2;
    block2.hashables.representative = key2.pub;
    block2.hashables.source = block.hash ();
    mu_coin::sign_message(key2.prv, key2.pub, block2.hash (), block2.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block2));
    mu_coin::change_block block3;
    block3.hashables.representative = key3.pub;
    block3.hashables.previous = block2.hash ();
    mu_coin::sign_message (key2.prv, key2.pub, block3.hash (), block3.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block3));
    mu_coin::send_block block4;
    block4.hashables.destination = key2.pub;
    block4.hashables.previous = block.hash ();
    block4.hashables.balance.clear ();
    mu_coin::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block4));
    mu_coin::receive_block block5;
    block5.hashables.previous = block2.hash ();
    block5.hashables.source = block4.hash ();
    mu_coin::sign_message (key2.prv, key2.pub, block5.hash (), block5.signature);
    ASSERT_EQ (mu_coin::process_result::fork, ledger.process (block5));
}

TEST (ledger, checksum_single)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::keypair key1;
    mu_coin::genesis genesis (key1.pub);
    genesis.initialize (store);
    mu_coin::ledger ledger (store);
    store.checksum_put (0, 0, genesis.hash ());
	ASSERT_EQ (genesis.hash (), ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
    mu_coin::change_block block1;
    block1.hashables.previous = ledger.latest (key1.pub);
    mu_coin::sign_message (key1.prv, key1.pub, block1.hash (), block1.signature);
    mu_coin::checksum check1 (ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
	ASSERT_EQ (genesis.hash (), check1);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block1));
    mu_coin::checksum check2 (ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
    ASSERT_EQ (block1.hash (), check2);
}

TEST (ledger, checksum_two)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::keypair key1;
    mu_coin::genesis genesis (key1.pub);
    genesis.initialize (store);
    mu_coin::ledger ledger (store);
    store.checksum_put (0, 0, genesis.hash ());
	mu_coin::keypair key2;
    mu_coin::send_block block1;
    block1.hashables.previous = ledger.latest (key1.pub);
	block1.hashables.destination = key2.pub;
    mu_coin::sign_message (key1.prv, key1.pub, block1.hash (), block1.signature);
    ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block1));
	mu_coin::checksum check1 (ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
	mu_coin::open_block block2;
	block2.hashables.source = block1.hash ();
	mu_coin::sign_message (key2.prv, key2.pub, block2.hash (), block2.signature);
	ASSERT_EQ (mu_coin::process_result::progress, ledger.process (block2));
	mu_coin::checksum check2 (ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
	ASSERT_EQ (check1, check2 ^ block2.hash ());
}

TEST (ledger, DISABLED_checksum_range)
{
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::checksum check1 (ledger.checksum (0, std::numeric_limits <mu_coin::uint256_t>::max ()));
    ASSERT_TRUE (check1.is_zero ());
    mu_coin::block_hash hash1 (42);
    mu_coin::checksum check2 (ledger.checksum (0, 42));
    ASSERT_TRUE (check2.is_zero ());
    mu_coin::checksum check3 (ledger.checksum (42, std::numeric_limits <mu_coin::uint256_t>::max ()));
    ASSERT_EQ (hash1, check3);
}

TEST (client, balance)
{
    mu_coin::system system (1, 24000, 25000, 1, std::numeric_limits <mu_coin::uint256_t>::max ());
    system.clients [0]->client_m->wallet.insert (system.test_genesis_address.prv, system.clients [0]->client_m->wallet.password);
    ASSERT_EQ (std::numeric_limits <mu_coin::uint256_t>::max (), system.clients [0]->client_m->balance ());
}

TEST (system, generate_send_existing)
{
}

TEST (system, generate_send_new)
{
    mu_coin::system system (1, 24000, 25000, 1, std::numeric_limits <mu_coin::uint256_t>::max ());
    system.clients [0]->client_m->wallet.insert (system.test_genesis_address.prv, system.clients [0]->client_m->wallet.password);
    auto iterator1 (system.clients [0]->client_m->store.latest_begin ());
    ++iterator1;
    ASSERT_EQ (system.clients [0]->client_m->store.latest_end (), iterator1);
    system.generate_send_new (*system.clients [0]->client_m);
    mu_coin::address new_address;
    auto iterator2 (system.clients [0]->client_m->store.latest_begin ());
    if (iterator2->first != system.test_genesis_address.pub)
    {
        new_address = iterator2->first;
    }
    ++iterator2;
    ASSERT_NE (system.clients [0]->client_m->store.latest_end (), iterator2);
    if (iterator2->first != system.test_genesis_address.pub)
    {
        new_address = iterator2->first;
    }
    ++iterator2;
    ASSERT_EQ (system.clients [0]->client_m->store.latest_end (), iterator2);
    ASSERT_LT (system.clients [0]->client_m->ledger.account_balance (system.test_genesis_address.pub), std::numeric_limits <mu_coin::uint256_t>::max ());
    ASSERT_GT (system.clients [0]->client_m->ledger.account_balance (system.test_genesis_address.pub), 0);
}

TEST (system, generate_mass_activity)
{
    
}