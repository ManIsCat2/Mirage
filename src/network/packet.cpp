#include "packet.hpp"
#include "network.hpp"
#include "../config.hpp"
#include "../savefile.hpp"
#include "../gui.hpp"
#include "log.hpp"
#include <algorithm>
#include <iostream>
#include <zlib.h>
#include <cstdint>
#include <fstream>

std::list<ReliablePacket> gReliablePackets;
std::map<std::pair<uint8_t, uint16_t>, OrderedState> gOrderedStates;

static bool sOrderedPackets = false;
static uint16_t sCurrentOrderedGroupId = 0;
static uint16_t sCurrentOrderedSeqId = 0;
static uint16_t sNextSeqNum = 1;

static inline bool isValidGlobalIndex(int idx) {
    return idx >= 0 && idx < MAX_PLAYERS;
}

static inline bool getBit(uint8_t val, uint8_t num) {
    return (val >> num) & 1;
}

void packetOrderedBegin() {
    if (sOrderedPackets) return;
    sOrderedPackets = true;

    sCurrentOrderedGroupId++;
    if (sCurrentOrderedGroupId == 0) sCurrentOrderedGroupId++;
    sCurrentOrderedSeqId = 1;
}

void packetOrderedEnd() {
    sOrderedPackets = false;
    sCurrentOrderedSeqId = 0;
}

CoopPacket::CoopPacket(socket_t s, sockaddr_in a, uint64_t pId, const uint8_t *compData, size_t compLen) : sock(s), addr(a), peerId(pId) {
    if (!compData || compLen == 0) return;

    std::vector<uint8_t> dest(65536);
    uLongf destLen = dest.size();

    if (uncompress(dest.data(), &destLen, compData, compLen) != Z_OK) {
        Logging::log("SERVER", "failed to decompress packet");
        return;
    }

    if (destLen >= 4) {
        destLen -= 4;
    }

    dest.resize(destLen);
    data = std::move(dest);

    if (data.size() < 3) return;

    pktType = data[0];
    seqId = data[1] | (data[2] << 8);
    offset = 3;
    flags = read<uint8_t>();

    levelAreaMustMatch = getBit(flags, 0);
    requestBroadcast = getBit(flags, 1);
    isOrdered = getBit(flags, 2);
    levelMustMatch = getBit(flags, 3);

    destGlobalId = read<uint8_t>();

    if (isOrdered) {
        orderedFromGlobalId = read<uint8_t>();
        orderedGroupId = read<uint16_t>();
        orderedSeqId = read<uint16_t>();
    }

    if (levelAreaMustMatch) {
        courseNum = read<uint8_t>();
        actNum = read<uint8_t>();
        levelNum = read<int16_t>();
        areaIndex = read<uint8_t>();
    } else if (levelMustMatch) {
        courseNum = read<uint8_t>();
        actNum = read<uint8_t>();
        levelNum = read<int16_t>();
    }
}

CoopPacket::CoopPacket(socket_t s, sockaddr_in a, uint64_t pId, uint8_t pType, bool reliable, uint8_t levelMatchType, int asGlobalIndex) : sock(s), addr(a), peerId(pId), pktType(pType), isReliable(reliable) {
    data.reserve(256);

    uint8_t initFlags = 0;
    if (levelMatchType == PLMT_AREA) initFlags |= (1 << 0);
    if (levelMatchType == PLMT_LEVEL) initFlags |= (1 << 3);
    if (sOrderedPackets) initFlags |= (1 << 2);

    if (reliable) {
        seqId = sNextSeqNum++;
        if (sNextSeqNum == 0) sNextSeqNum = 1;
    }

    write<uint8_t>(pType);
    write<uint16_t>(seqId);
    write<uint8_t>(initFlags);
    write<uint8_t>(PACKET_DESTINATION_BROADCAST);

    int globalId = isValidGlobalIndex(asGlobalIndex) ? asGlobalIndex : 0;
    NetworkPlayer *localNp = &gNetworkPlayers[globalId];

    if (sOrderedPackets) {
        uint8_t localGlobalIndex = localNp->globalIndex;
        write<uint8_t>(localGlobalIndex);
        write<uint16_t>(sCurrentOrderedGroupId);
        write<uint16_t>(0);
    }

    if (levelMatchType == PLMT_AREA) {
        write<uint8_t>(localNp->currCourseNum);
        write<uint8_t>(localNp->currActNum);
        write<int16_t>(localNp->currLevelNum);
        write<uint8_t>(localNp->currAreaIndex);
    } else if (levelMatchType == PLMT_LEVEL) {
        write<uint8_t>(localNp->currCourseNum);
        write<uint8_t>(localNp->currActNum);
        write<int16_t>(localNp->currLevelNum);
    }

    isOrdered = sOrderedPackets;
    orderedGroupId = sOrderedPackets ? sCurrentOrderedGroupId : 0;
    orderedSeqId = 0;
}

CoopPacket CoopPacket::createOutgoing(socket_t s, sockaddr_in a, uint64_t pId, uint8_t pType, bool reliable, uint8_t levelMatchType, int asGlobalIndex) {
    return CoopPacket(s, a, pId, pType, reliable, levelMatchType, asGlobalIndex);
}

CoopPacket CoopPacket::duplicate() {
    CoopPacket p = *this;
    p.data = data;

    if (seqId != 0) {
        p.isReliable = true;
        p.seqId = sNextSeqNum++;
        if (sNextSeqNum == 0) sNextSeqNum = 1;
    } else {
        p.isReliable = false;
        p.seqId = 0;
    }

    if (p.data.size() >= 3) {
        p.data[1] = p.seqId & 0xFF;
        p.data[2] = (p.seqId >> 8) & 0xFF;
    }

    p.offset = p.data.size();
    return p;
}

void CoopPacket::setOrderedData() {
    if (orderedGroupId == 0 || orderedSeqId != 0) return;

    orderedSeqId = sCurrentOrderedSeqId++;

    if (data.size() >= 10) {
        data[8] = orderedSeqId & 0xFF;
        data[9] = (orderedSeqId >> 8) & 0xFF;
    }
}

std::vector<uint8_t> CoopPacket::compressAndHash() {
    if (data.empty()) return {};

    if (isOrdered) {
        setOrderedData();
    }

    uint32_t hashVal = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        hashVal ^= static_cast<uint32_t>(data[i]) << (8 * (i % 4));
    }
    write<uint32_t>(hashVal);

    std::vector<uint8_t> compressed(compressBound(data.size()));
    uLongf destLen = compressed.size();

    if (compress2(compressed.data(), &destLen, data.data(), data.size(), Z_BEST_COMPRESSION) == Z_OK) {
        compressed.resize(destLen);
        return compressed;
    }
    return {};
}

std::string CoopPacket::getSenderIdentifier(void) {
    NetworkPlayer *senderNp = gNetworkSystem->getPlayerFromSender(addr, peerId);
    uint8_t senderGlobalIndex = senderNp ? senderNp->globalIndex : 0;
    std::string ident;

    if (gServerConfig.networkSystem == SYS_SOCKET) {
        ident = std::to_string(senderGlobalIndex);
    } else if (gServerConfig.networkSystem == SYS_COOPNET) {
        ident = std::to_string(peerId);
    } else {
        ident = std::format("{} - {}", peerId, senderGlobalIndex);
    }
    return ident;
}

void CoopPacket::sendTo(sockaddr_in dest, uint64_t destPeerId) {
    if (gServerConfig.networkSystem == 1) {
        if (gServerConfig.whitelist.size() > 0) {
            if (std::find(gServerConfig.whitelist.begin(), gServerConfig.whitelist.end(), gNetworkPlayerCoopNetIds[peerId]) != gServerConfig.whitelist.end()) {
            } else {
                Logging::log("SERVER", "ignoring send for blacklisted client with peer id {}", peerId);
                return;
            }
        }
        if (std::find(gServerConfig.blacklist.begin(), gServerConfig.blacklist.end(), gNetworkPlayerCoopNetIds[peerId]) != gServerConfig.blacklist.end()) {
            Logging::log("SERVER", "ignoring send for blacklisted client with peer id {}", peerId);
            return;
        }
    }
    auto compressed = compressAndHash();
    if (compressed.empty()) return;

    if (gNetworkSystem) {
        gNetworkSystem->sendTo(dest, destPeerId, compressed.data(), compressed.size());
    }

    if (isReliable && seqId != 0) {
        gReliablePackets.push_back({
            seqId,
            sock,
            dest,
            destPeerId,
            std::move(compressed),
            std::chrono::steady_clock::now(),
            1
        });
    }
    data.clear();
}

void CoopPacket::sendTo(int globalIdx) {
    if (globalIdx >= 0 && globalIdx < MAX_PLAYERS) {
        sendTo(gNetworkPlayerAddrs[globalIdx], gNetworkPlayerPeerIds[globalIdx]);
    }
}

void CoopPacket::sendBack() {
    sendTo(addr, peerId);
}

void CoopPacket::sendToAll() {
    auto compressed = compressAndHash();
    if (compressed.empty()) return;

    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!gNetworkPlayers[i].connected) continue;

        if (gNetworkSystem->isSameEndpoint(addr, peerId, gNetworkPlayerAddrs[i], gNetworkPlayerPeerIds[i])) {
            continue;
        }

        const sockaddr_in &dest = gNetworkPlayerAddrs[i];
        uint64_t destPeerId = gNetworkPlayerPeerIds[i];

        if (gNetworkSystem) {
            gNetworkSystem->sendTo(dest, destPeerId, compressed.data(), compressed.size());
        }

        if (isReliable && seqId != 0) {
            gReliablePackets.push_back({
                seqId,
                sock,
                dest,
                destPeerId,
                compressed,
                std::chrono::steady_clock::now(),
                1
            });
        }
    }

    data.clear();
}

void CoopPacket::handle() {
    if (data.empty()) return;

    if (seqId != 0 && pktType != PACKET_ACK) {
        auto ackPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_ACK, false, PLMT_NONE);
        ackPkt.write<uint16_t>(seqId);
        ackPkt.sendBack();
    }

    if (destGlobalId != PACKET_DESTINATION_BROADCAST && destGlobalId != 0 && pktType != PACKET_ACK && pktType != PACKET_MOD_LIST_REQUEST) {
        CoopPacket p = duplicate();
        p.sendTo(destGlobalId);
        return;
    }

    if (isOrdered) {
        processOrdered();
    } else {
        execute();
    }

    if (requestBroadcast && gNetworkSystem->requireServerBroadcast()) {
        CoopPacket p = duplicate();
        if (p.pktType == PACKET_CHAT && peerId == 0 && gServerConfig.networkSystem == SYS_BOTH && p.data.size() >= 8) {
            uint8_t senderIdx = p.data[5];
            uint16_t msgLen = p.data[6] | (p.data[7] << 8);
            size_t end = std::min<size_t>(8 + msgLen, p.data.size());
            std::string oldMsg(p.data.begin() + 8, p.data.begin() + end);

            std::string senderName = isValidGlobalIndex(senderIdx) ? gNetworkPlayers[senderIdx].name : "Unknown";
            std::string newMsg = "[from " + senderName + "\\#dcdcdc\\] " + oldMsg;
            if (newMsg.size() > MAX_CHAT_MSG_LENGTH - 1) newMsg.resize(MAX_CHAT_MSG_LENGTH - 1);

            p.data.resize(8);
            p.data[5] = 0;
            p.data[6] = newMsg.size() & 0xFF;
            p.data[7] = (newMsg.size() >> 8) & 0xFF;
            p.data.insert(p.data.end(), newMsg.begin(), newMsg.end());
        }
        p.sendToAll();
    }
}

void CoopPacket::processOrdered() {
    std::pair<uint8_t, uint16_t> key = {orderedFromGlobalId, orderedGroupId};
    auto &state = gOrderedStates[key];

    if (orderedSeqId < state.processSeqId) {
        return;
    }

    state.queuedPackets.emplace(orderedSeqId, *this);

    auto it = state.queuedPackets.begin();
    while (it != state.queuedPackets.end() && it->first == state.processSeqId) {
        it->second.execute();
        it = state.queuedPackets.erase(it);
        state.processSeqId++;
    }
}

void CoopPacket::execute() {
    NetworkPlayer *senderNp = gNetworkSystem->getPlayerFromSender(addr, peerId);
    uint8_t senderGlobalIndex = senderNp ? senderNp->globalIndex : 0;
    std::string senderIdentifier = getSenderIdentifier();

    switch (pktType) {
        case PACKET_ACK: {
            uint16_t ackedSeq = read<uint16_t>();
            gReliablePackets.remove_if([this, ackedSeq](const ReliablePacket &p) {
                if (p.seqId != ackedSeq) return false;
                return gNetworkSystem->isSameEndpoint(p.addr, p.peerId, this->addr, this->peerId);
            });
            break;
        }

        case PACKET_PLAYER:
        case PACKET_OBJECT:
        case PACKET_COLLECT_COIN:
        case PACKET_COLLECT_STAR:
        case PACKET_COLLECT_ITEM:
        case PACKET_SPAWN_OBJECTS:
        case PACKET_SPAWN_STAR:
        case PACKET_SPAWN_STAR_NLE:
        case PACKET_LEVEL_RESPAWN_INFO:
            break;

        case PACKET_PLAYER_SETTINGS: {
            uint8_t globalIndex = read<uint8_t>();
            if (!isValidGlobalIndex(globalIndex)) break;

            std::string name = read<std::string>(64);
            uint8_t model = read<uint8_t>();
            PlayerPalette palette;
            for (int i = 0; i < 24; i++) {
                palette.colors[i] = read<uint8_t>();
            }

            gNetworkPlayers[globalIndex].name = std::string(name.c_str());
            gNetworkPlayers[globalIndex].modelIndex = model;
            std::memcpy(gNetworkPlayers[globalIndex].palette.colors, palette.colors, 24);
            break;
        }
        case PACKET_SAVE_FILE: {
            int32_t fileIndex = read<int32_t>();
            uint8_t backupSlot = read<uint8_t>();

            gSaveFile.save();
            break;
        }
        case PACKET_SAVE_SET_FLAG: {
            int32_t fileIndex = read<int32_t>();
            int32_t courseIndex = read<int32_t>();
            uint8_t courseData = read<uint8_t>();
            uint32_t flags = read<uint32_t>();
            uint8_t backupSlot = read<uint8_t>();

            gSaveFile.setFlags(flags, courseIndex, courseData);
            break;
        }
        case PACKET_SAVE_REMOVE_FLAG: {
            int32_t fileIndex = read<int32_t>();
            int32_t courseIndex = read<int32_t>();
            uint8_t courseData = read<uint8_t>();
            uint32_t flags = read<uint32_t>();
            uint8_t backupSlot = read<uint8_t>();

            gSaveFile.removeFlags(flags, courseIndex, courseData);
            break;
        }
        case PACKET_MOD_LIST_REQUEST: {
            std::string version = read<std::string>(128);
            Logging::log("SERVER", "[{}] PACKET_MOD_LIST_REQUEST received: client version '{}'", senderIdentifier, version);

            packetOrderedBegin();

            auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_MOD_LIST, true, PLMT_NONE);
            outPkt.write<std::string>(gServerConfig.version, 128);
            outPkt.write<uint16_t>(gServerConfig.mods.size());
            outPkt.sendBack();

            for (uint16_t i = 0; i < gServerConfig.mods.size(); i++) {
                CoopMod &mod = gServerConfig.mods[i];

                auto entryPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_MOD_LIST_ENTRY, true, PLMT_NONE);
                entryPkt.write<uint16_t>(i);
                uint16_t nameLen = mod.name.size();
                entryPkt.write<uint16_t>(nameLen);
                entryPkt.write<std::string>(mod.name, nameLen);
                entryPkt.write<uint16_t>(0);
                entryPkt.write<std::string>("", 0);
                uint16_t relLen = mod.relativePath.size();
                entryPkt.write<uint16_t>(relLen);
                entryPkt.write<std::string>(mod.relativePath, relLen);
                entryPkt.write<uint64_t>(mod.size);
                entryPkt.write<uint8_t>(mod.isDirectory);
                entryPkt.write<uint8_t>(mod.pausable);
                entryPkt.write<uint8_t>(mod.ignoreScriptWarnings);
                entryPkt.write<uint16_t>(mod.files.size());
                entryPkt.sendBack();

                for (uint16_t j = 0; j < mod.files.size(); j++) {
                    CoopModFile &modFile = mod.files[j];
                    auto filePkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_MOD_LIST_FILE, true, PLMT_NONE);
                    filePkt.write<uint16_t>(i);
                    filePkt.write<uint16_t>(j);
                    filePkt.write<uint16_t>(modFile.relativePath.size());
                    filePkt.write<std::string>(modFile.relativePath, modFile.relativePath.size());
                    filePkt.write<uint64_t>(modFile.size);
                    for (int h = 0; h < 16; h++) filePkt.write<uint8_t>(0); // hash
                    filePkt.sendBack();
                }
            }

            auto donePkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_MOD_LIST_DONE, true, PLMT_NONE);
            donePkt.sendBack();

            packetOrderedEnd();
            break;
        }
        case PACKET_DOWNLOAD_REQUEST: {
            uint64_t requestOffset = read<uint64_t>();

            uint64_t totalModsSize = 0;
            for (const auto &mod : gServerConfig.mods) {
                totalModsSize += mod.size;
            }

            for (uint64_t i = 0; i < 50; i++) {
                uint64_t sendOffset = requestOffset + (i * 800);
                if (sendOffset >= totalModsSize) break;

                uint8_t chunk[800] = { 0 };
                uint64_t chunkFill = 0;
                uint64_t fileStartOffset = 0;

                for (const auto &mod : gServerConfig.mods) {
                    if ((fileStartOffset + mod.size) < sendOffset) {
                        fileStartOffset += mod.size;
                        continue;
                    }

                    for (const auto &modFile : mod.files) {
                        if ((fileStartOffset + modFile.size) < sendOffset) {
                            fileStartOffset += modFile.size;
                            continue;
                        }

                        uint64_t fileReadOffset = (sendOffset > fileStartOffset) ? (sendOffset - fileStartOffset) : 0;
                        uint64_t fileReadLength = std::min((uint64_t)(modFile.size - fileReadOffset), (uint64_t)(800 - chunkFill));

                        std::ifstream file(modFile.realPath, std::ios::binary);
                        if (file.is_open()) {
                            file.seekg(fileReadOffset, std::ios::beg);
                            if (file.good()) {
                                file.read(reinterpret_cast<char*>(&chunk[chunkFill]), fileReadLength);
                            }
                        } else {
                            Logging::log("SERVER", "[{}] PACKET_DOWNLOAD_REQUEST received: failed to open mod file '{}'", senderIdentifier, modFile.realPath);
                        }

                        chunkFill += fileReadLength;
                        fileStartOffset += modFile.size;

                        if (chunkFill >= 800) {
                            goto after_filled;
                        }
                    }
                }
            after_filled:
                auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_DOWNLOAD, true, PLMT_NONE);
                outPkt.write<uint64_t>(sendOffset);
                outPkt.write<uint64_t>(chunkFill);

                for (uint64_t j = 0; j < chunkFill; j++) {
                    outPkt.write<uint8_t>(chunk[j]);
                }

                outPkt.sendBack();
            }

            break;
        }
        case PACKET_JOIN_REQUEST: {
            bool exists = (gNetworkSystem->getPlayerFromSender(addr, peerId) != nullptr);

            if (exists) {
                Logging::log("SERVER", "[{}] PACKET_JOIN_REQUEST received: already joined, ignoring", peerId, senderGlobalIndex);
                break;
            }
            std::string version = read<std::string>(128);
            uint8_t model = read<uint8_t>();
            PlayerPalette palette;
            for (int i = 0; i < 24; i++) {
                palette.colors[i] = read<uint8_t>();
            }
            std::string name = read<std::string>(64);

            Logging::log("SERVER", "[{}] PACKET_JOIN_REQUEST received: version '{}', name '{}', model {}", senderIdentifier, version, name + "\\#reset\\", model);

            uint8_t globalIndex = 0;
            uint8_t connectedCount = 0;
            for (uint8_t i = 1; i < MAX_PLAYERS; i++) {
                if (!gNetworkPlayers[i].connected) {
                    globalIndex = i;
                    break;
                } else {
                    connectedCount++;
                }
            }

            if (!globalIndex || !isValidGlobalIndex(globalIndex)) {
                Logging::log("SERVER", "[{}] PACKET_JOIN_REQUEST Rejected: server full, dropping request from '{}'", senderIdentifier, name);
                break;
            }

            NetworkPlayer *np = &gNetworkPlayers[globalIndex];
            gNetworkPlayerAddrs[globalIndex] = addr;
            gNetworkPlayerPeerIds[globalIndex] = peerId;

            auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_JOIN, true, PLMT_NONE);
            outPkt.write<std::string>(gServerConfig.version, 128);
            outPkt.write<uint8_t>(globalIndex);

            outPkt.write<int16_t>(gServerConfig.savefileIndex + 1);
            outPkt.write<uint8_t>(gServerConfig.playerInteractions);
            outPkt.write<uint8_t>(gServerConfig.bouncyBounds);
            outPkt.write<uint8_t>(gServerConfig.knockStrength);
            outPkt.write<uint8_t>(gServerConfig.starStaying);
            outPkt.write<uint8_t>(gServerConfig.skipIntro);
            outPkt.write<uint8_t>(gServerConfig.bubbleDeath);
            outPkt.write<uint8_t>(1); // headless
            outPkt.write<uint8_t>(gServerConfig.nametags);
            outPkt.write<uint8_t>(gServerConfig.maxPlayers);
            outPkt.write<uint8_t>(gServerConfig.pauseAnywhere);
            outPkt.write<uint8_t>(0);

            std::vector<uint8_t> eeprom = gSaveFile.getBuffer();
            for (int i = 0; i < 512; i++) {
                outPkt.write<uint8_t>(eeprom[i]);
            }
            outPkt.sendBack();

            np->connected = true;
            np->globalIndex = globalIndex;
            np->name = std::string(name.c_str());
            np->modelIndex = model;
            std::memcpy(np->palette.colors, palette.colors, 24);
            np->networkId = peerId;
            np->currLevelNum = 16;
            np->currAreaIndex = 1;
            np->type = NPT_CLIENT;

            auto broadcastPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_NETWORK_PLAYERS, true, PLMT_NONE);
            broadcastPkt.write<uint8_t>(1);
            broadcastPkt.write<uint8_t>(np->type);
            broadcastPkt.write<uint8_t>(np->globalIndex);
            broadcastPkt.write<uint16_t>(np->currLevelAreaSeqId);
            broadcastPkt.write<int16_t>(np->currCourseNum);
            broadcastPkt.write<int16_t>(np->currActNum);
            broadcastPkt.write<int16_t>(np->currLevelNum);
            broadcastPkt.write<int16_t>(np->currAreaIndex);
            broadcastPkt.write<uint8_t>(np->currLevelSyncValid);
            broadcastPkt.write<uint8_t>(np->currAreaSyncValid);
            broadcastPkt.write<int64_t>(np->networkId);
            broadcastPkt.write<uint8_t>(np->modelIndex);
            for (int i = 0; i < 24; i++) {
                broadcastPkt.write<uint8_t>(np->palette.colors[i]);
            }
            broadcastPkt.write<std::string>(np->name, 64);
            broadcastPkt.write<std::string>(np->discordId, 64);
            broadcastPkt.sendToAll();
            break;
        }
        case PACKET_NETWORK_PLAYERS_REQUEST: {
            uint8_t connectedCount = 0;
            for (const auto &player : gNetworkPlayers) {
                if (player.connected) {
                    connectedCount++;
                }
            }

            Logging::log("SERVER", "[{}] PACKET_NETWORK_PLAYERS_REQUEST received: {} connected player(s)", senderIdentifier, connectedCount);

            auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_NETWORK_PLAYERS, true, PLMT_NONE);
            outPkt.write<uint8_t>(connectedCount);
            for (const auto &player : gNetworkPlayers) {
                if (!player.connected || !isValidGlobalIndex(player.globalIndex)) continue;
                if (gNetworkSystem->isSameEndpoint(addr, peerId, gNetworkPlayerAddrs[player.globalIndex], gNetworkPlayerPeerIds[player.globalIndex])) {
                    continue;
                }

                uint8_t npType = player.type;
                if (npType == NPT_LOCAL) { npType = NPT_SERVER; }
                else if (player.globalIndex == senderGlobalIndex) { npType = NPT_LOCAL; }

                outPkt.write<uint8_t>(npType);
                outPkt.write<uint8_t>(player.globalIndex);
                outPkt.write<uint16_t>(player.currLevelAreaSeqId);
                outPkt.write<int16_t>(player.currCourseNum);
                outPkt.write<int16_t>(player.currActNum);
                outPkt.write<int16_t>(player.currLevelNum);
                outPkt.write<int16_t>(player.currAreaIndex);
                outPkt.write<uint8_t>(player.currLevelSyncValid);
                outPkt.write<uint8_t>(player.currAreaSyncValid);
                outPkt.write<int64_t>(player.networkId);
                outPkt.write<uint8_t>(player.modelIndex);
                for (int i = 0; i < 24; i++) {
                    outPkt.write<uint8_t>(player.palette.colors[i]);
                }
                outPkt.write<std::string>(player.name, MAX_CONFIG_STRING);
                outPkt.write<std::string>(player.discordId, 64);
            }
            outPkt.sendBack();
            break;
        }
        case PACKET_PING: {
            uint8_t globalIndex = read<uint8_t>();
            if (!isValidGlobalIndex(globalIndex)) break;
            double timestamp = read<double>();

            if (globalIndex != 0) {
                Logging::log("SERVER", "[{}] PACKET_PING received: globalindex {}, name '{}', timestamp {}", senderIdentifier, globalIndex, getNetworkPlayerName(globalIndex), timestamp);
            }
            auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_PONG, false, PLMT_NONE);
            outPkt.write<uint8_t>(globalIndex);
            outPkt.write<double>(timestamp);
            outPkt.sendBack();
            break;
        }
        case PACKET_PONG: {
            uint8_t globalIndex = read<uint8_t>();
            if (!isValidGlobalIndex(globalIndex)) break;
            double timestamp = read<double>();

            //Logging::log("SERVER", "[{}] PACKET_PONG received: globalindex {}, name '{}', timestamp {}", senderIdentifier, globalIndex, getNetworkPlayerName(globalIndex), timestamp);
            break;
        }
        case PACKET_CHANGE_LEVEL: {
            int16_t courseNum = read<int16_t>();
            int16_t actNum = read<int16_t>();
            int16_t levelNum = read<int16_t>();
            int16_t areaIndex = read<int16_t>();

            Logging::log("SERVER", "[{}] PACKET_CHANGE_LEVEL received: course {}, act {}, level {}, area {}", senderIdentifier, courseNum, actNum, levelNum, areaIndex);

            NetworkPlayer *np = gNetworkSystem->getPlayerFromSender(addr, peerId);
            if (np) {
                np->currCourseNum = courseNum;
                np->currActNum = actNum;
                np->currLevelNum = levelNum;
                np->currAreaIndex = areaIndex;

                NetworkPlayer *npInArea = getNetworkPlayerFromArea(courseNum, actNum, levelNum, areaIndex);
                NetworkPlayer *npInLevel = getNetworkPlayerFromLevel(courseNum, actNum, levelNum);
                NetworkPlayer *npAny = (npInArea == nullptr) ? npInLevel : npInArea;

                if (npAny == nullptr || np->currActNum == 99) {
                    np->currLevelSyncValid = true;
                    np->currAreaSyncValid = true;

                    auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_SYNC_VALID, true, PLMT_NONE);
                    outPkt.write<int16_t>(courseNum);
                    outPkt.write<int16_t>(actNum);
                    outPkt.write<int16_t>(levelNum);
                    outPkt.write<int16_t>(areaIndex);
                    outPkt.write<uint8_t>(0);
                    outPkt.write<uint8_t>(np->globalIndex);
                    outPkt.sendBack();
                    return;
                }

                if (npAny == npInArea) {
                    auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_LEVEL_AREA_REQUEST, true, PLMT_NONE);
                    outPkt.write<uint8_t>(np->globalIndex);
                    outPkt.write<int16_t>(np->currCourseNum);
                    outPkt.write<int16_t>(np->currActNum);
                    outPkt.write<int16_t>(np->currLevelNum);
                    outPkt.write<int16_t>(np->currAreaIndex);
                    outPkt.sendTo(npAny->globalIndex);
                } else {
                    auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_LEVEL_REQUEST, true, PLMT_NONE);
                    outPkt.write<uint8_t>(np->globalIndex);
                    outPkt.write<int16_t>(np->currCourseNum);
                    outPkt.write<int16_t>(np->currActNum);
                    outPkt.write<int16_t>(np->currLevelNum);
                    outPkt.sendTo(npAny->globalIndex);
                }
            }
            break;
        }
        case PACKET_LEVEL_AREA_INFORM: {
            uint16_t seq = read<uint16_t>();
            uint8_t globalIndex = read<uint8_t>();
            if (!isValidGlobalIndex(globalIndex)) break;

            int16_t courseNum = read<int16_t>();
            int16_t actNum = read<int16_t>();
            int16_t levelNum = read<int16_t>();
            int16_t areaIndex = read<int16_t>();
            uint8_t levelSyncValid = read<uint8_t>();
            uint8_t areaSyncValid = read<uint8_t>();

            Logging::log("SERVER", "[{}] PACKET_LEVEL_AREA_INFORM received: globalindex {}, seq {}, course {}, act {}, level {}, area {}, level sync {}, area sync {}", senderIdentifier, globalIndex, seq, courseNum, actNum, levelNum, areaIndex, levelSyncValid, areaSyncValid);

            NetworkPlayer *np = &gNetworkPlayers[globalIndex];

            np->currLevelAreaSeqId = seq;
            np->currLevelSyncValid = levelSyncValid;
            np->currAreaSyncValid = areaSyncValid;

            np->currCourseNum = courseNum;
            np->currActNum = actNum;
            np->currLevelNum = levelNum;
            np->currAreaIndex = areaIndex;
            break;
        }
        case PACKET_CHAT: {
            uint8_t globalIndex = read<uint8_t>();
            if (!isValidGlobalIndex(globalIndex)) break;

            uint16_t msgLen = read<uint16_t>();
            if (msgLen >= MAX_CHAT_MSG_LENGTH) {
                msgLen = MAX_CHAT_MSG_LENGTH - 1;
            }
            std::string msg = read<std::string>(msgLen);

            Logging::log("SERVER", "[{}] PACKET_CHAT received: globalindex {}, size {}, str '{}'", senderIdentifier, globalIndex, msgLen, msg);
            gGui.chatLog.push_back(gNetworkPlayers[globalIndex].name + ": " + msg);
            break;
        }
        case PACKET_LEAVING: {
            uint8_t globalIndex = read<uint8_t>();

            Logging::log("SERVER", "[{}] PACKET_LEAVING received: globalindex {}", senderIdentifier, globalIndex);

            if (globalIndex == 0 || !isValidGlobalIndex(globalIndex) || !senderNp || senderGlobalIndex != globalIndex || !gNetworkPlayers[globalIndex].connected) break;

            auto outPkt = CoopPacket::createOutgoing(sock, addr, peerId, PACKET_LEAVING, true, PLMT_NONE);
            outPkt.write<uint8_t>(globalIndex);
            outPkt.sendToAll();

            Logging::log("SERVER", "player '{}' (globalindex {}) disconnected", getNetworkPlayerName(globalIndex), globalIndex);
            clearNetworkPlayer(globalIndex);
            break;
        }
        default: {
            Logging::log("SERVER", "[{}] UNIMPLEMENTED_PACKET received: type {}, size {}", senderIdentifier, pktType, data.size());
            break;
        }
    }
}