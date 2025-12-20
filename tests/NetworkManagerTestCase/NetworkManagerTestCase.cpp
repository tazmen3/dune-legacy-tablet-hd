/*
 *  NetworkManagerTestCase.cpp - Unit tests for NetworkManager functionality
 *
 *  Tests packet serialization, address handling, and protocol helpers.
 *  Does NOT test actual network I/O (that requires integration tests).
 */

#include <catch2/catch_all.hpp>

#include <Network/ENetHelper.h>
#include <Network/ENetPacketOStream.h>
#include <Network/ENetPacketIStream.h>
#include <Network/NetworkManager.h>

#include <enet/enet.h>

// ENet initialization fixture
struct ENetFixture {
    ENetFixture() {
        if (enet_initialize() != 0) {
            throw std::runtime_error("Failed to initialize ENet");
        }
    }
    ~ENetFixture() {
        enet_deinitialize();
    }
};

// =============================================================================
// Packet Type Constants Tests
// =============================================================================

TEST_CASE("NetworkManager: Packet type constants are unique", "[network][protocol]") {
    // Verify packet types are unique and in expected order
    REQUIRE(NETWORKPACKET_SENDGAMEINFO == 1);
    REQUIRE(NETWORKPACKET_SENDINITIALSTATE == 2);
    REQUIRE(NETWORKPACKET_SENDNEXTEXPECTEDCYCLE == 3);
    REQUIRE(NETWORKPACKET_READY == 4);
    REQUIRE(NETWORKPACKET_SENDNAME == 5);
    REQUIRE(NETWORKPACKET_CHATMESSAGE == 6);
    REQUIRE(NETWORKPACKET_CHANGEEVENTLIST == 7);
    REQUIRE(NETWORKPACKET_STARTGAME == 8);
    REQUIRE(NETWORKPACKET_COMMANDLIST == 9);
    REQUIRE(NETWORKPACKET_SELECTIONLIST == 10);
    REQUIRE(NETWORKPACKET_CONFIG_HASH == 11);
    REQUIRE(NETWORKPACKET_SETPATHBUDGET == 12);
    REQUIRE(NETWORKPACKET_CLIENTSTATS == 13);
    REQUIRE(NETWORKPACKET_MOD_INFO == 14);
    REQUIRE(NETWORKPACKET_MOD_REQUEST == 15);
    REQUIRE(NETWORKPACKET_MOD_CHUNK == 16);
    REQUIRE(NETWORKPACKET_MOD_COMPLETE == 17);
    REQUIRE(NETWORKPACKET_MOD_ACK == 18);
    REQUIRE(NETWORKPACKET_KEEPALIVE == 19);
    
    REQUIRE(NETWORK_PROTOCOL_VERSION == 3);
}

// =============================================================================
// Address Utility Tests (require ENet)
// =============================================================================

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Address2String for IPv4", "[network][address]") {
    ENetAddress addr;
    addr.host = 0x0100007F;  // 127.0.0.1 in little-endian
    addr.port = 12345;
    
    REQUIRE(Address2String(addr) == "127.0.0.1");
}

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Address2String for localhost", "[network][address]") {
    ENetAddress addr;
    enet_address_set_host(&addr, "localhost");
    addr.port = 8080;
    
    REQUIRE(Address2String(addr) == "127.0.0.1");
}

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Address2String for broadcast", "[network][address]") {
    ENetAddress addr;
    addr.host = ENET_HOST_BROADCAST;
    addr.port = 12345;
    
    REQUIRE(Address2String(addr) == "255.255.255.255");
}

// =============================================================================
// ENet Packet Stream Tests (require ENet)
// =============================================================================

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Packet stream write/read uint32", "[network][packet]") {
    ENetPacketOStream ostream(ENET_PACKET_FLAG_RELIABLE);
    ostream.writeUint32(0x12345678);
    ostream.writeUint32(0xDEADBEEF);
    
    ENetPacket* packet = ostream.getPacket();
    REQUIRE(packet != nullptr);
    REQUIRE(packet->dataLength == 8);
    
    ENetPacketIStream istream(packet);
    REQUIRE(istream.readUint32() == 0x12345678);
    REQUIRE(istream.readUint32() == 0xDEADBEEF);
}

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Packet stream write/read string", "[network][packet]") {
    ENetPacketOStream ostream(ENET_PACKET_FLAG_RELIABLE);
    ostream.writeString("Hello, Dune Legacy!");
    
    ENetPacket* packet = ostream.getPacket();
    REQUIRE(packet != nullptr);
    
    ENetPacketIStream istream(packet);
    REQUIRE(istream.readString() == "Hello, Dune Legacy!");
}

TEST_CASE_METHOD(ENetFixture, "NetworkManager: Packet stream complex packet", "[network][packet]") {
    // Write a complex packet similar to NETWORKPACKET_CLIENTSTATS
    ENetPacketOStream ostream(ENET_PACKET_FLAG_RELIABLE);
    ostream.writeUint32(NETWORKPACKET_CLIENTSTATS);
    ostream.writeUint32(750);
    ostream.writeFloat(60.0f);
    ostream.writeFloat(0.5f);
    ostream.writeUint32(100);
    ostream.writeUint32(15000);
    
    ENetPacket* packet = ostream.getPacket();
    REQUIRE(packet != nullptr);
    
    ENetPacketIStream istream(packet);
    REQUIRE(istream.readUint32() == NETWORKPACKET_CLIENTSTATS);
    REQUIRE(istream.readUint32() == 750);
    REQUIRE(istream.readFloat() == Catch::Approx(60.0f));
    REQUIRE(istream.readFloat() == Catch::Approx(0.5f));
    REQUIRE(istream.readUint32() == 100);
    REQUIRE(istream.readUint32() == 15000);
}

// =============================================================================
// Keep-Alive Timing Tests
// =============================================================================

TEST_CASE("NetworkManager: Keep-alive interval is reasonable", "[network][keepalive]") {
    // Should be less than typical NAT timeout (30s)
    REQUIRE(NetworkManager::KEEPALIVE_INTERVAL_MS == 10000);
    REQUIRE(NetworkManager::KEEPALIVE_INTERVAL_MS < 30000);
    REQUIRE(NetworkManager::KEEPALIVE_INTERVAL_MS >= 1000);
}

// =============================================================================
// NAT Traversal Helper Tests
// =============================================================================

TEST_CASE("NetworkManager: Hole punch constants are reasonable", "[network][nat]") {
    REQUIRE(NetworkManager::PUNCH_PACKET_COUNT >= 5);
    REQUIRE(NetworkManager::PUNCH_PACKET_COUNT <= 50);
    REQUIRE(NetworkManager::PUNCH_PACKET_INTERVAL_MS >= 10);
    REQUIRE(NetworkManager::PUNCH_PACKET_INTERVAL_MS <= 200);
}
