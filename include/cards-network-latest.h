#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <span>
#include "cards-core-latest.h"

namespace LeanDockCards {
inline bool IncludeNetworkInterface(MIB_IF_ROW2 const& row) {
    auto const flags=row.InterfaceAndOperStatusFlags;
    if(flags.FilterInterface||flags.EndPointInterface||flags.NotMediaConnected||
       flags.Paused||row.OperStatus!=IfOperStatusUp||row.TunnelType!=TUNNEL_TYPE_NONE||
       (row.Type!=IF_TYPE_ETHERNET_CSMACD&&row.Type!=IF_TYPE_IEEE80211))return false;
    if(flags.HardwareInterface)return true;
    // Wi-Fi Direct/mobile-hotspot ports are virtual, but carry real wireless
    // traffic. Use NDIS metadata, not a localized alias or driver-name match.
    // Ethernet-shaped virtual adapters (TAP, WAN miniports, bridges) and
    // IP/tunnel/filter layers remain excluded.
    return row.Type==IF_TYPE_IEEE80211&&
        (row.PhysicalMediumType==NdisPhysicalMediumNative802_11||
         row.PhysicalMediumType==NdisPhysicalMediumWirelessLan);
}
inline NetworkSnapshot NetworkCounters(std::span<const MIB_IF_ROW2> rows) {
    NetworkSnapshot result;
    for(auto const& row:rows)if(IncludeNetworkInterface(row))
        result.push_back({row.InterfaceLuid.Value,row.InOctets,row.OutOctets});
    // This is the sum of link RX/TX, including hotspot forwarding on each
    // observed link. It is deliberately not deduplicated Internet throughput.
    return result;
}
inline std::optional<NetworkSnapshot> ReadNetwork() {
    MIB_IF_TABLE2* table=nullptr;
    if(GetIfTable2(&table)!=NO_ERROR||!table)return {};
    struct Release{MIB_IF_TABLE2* p;~Release(){FreeMibTable(p);}} release{table};
    if(table->NumEntries>4096)return {};
    return NetworkCounters({table->Table,table->NumEntries});
}
}
