#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "cards-core-latest.h"

namespace LeanDockCards {
inline std::optional<NetworkSnapshot> ReadNetwork() {
    MIB_IF_TABLE2* table=nullptr;
    if(GetIfTable2(&table)!=NO_ERROR||!table)return {};
    struct Release{MIB_IF_TABLE2* p;~Release(){FreeMibTable(p);}} release{table};
    if(table->NumEntries>4096)return {};
    NetworkSnapshot result;
    for(ULONG i=0;i<table->NumEntries;++i){auto const& row=table->Table[i];auto const flags=row.InterfaceAndOperStatusFlags;
        // Aggregate physical Ethernet/Wi-Fi only. Tunnel, filter, loopback and
        // virtual endpoint counters would count the same traffic again.
        if(!flags.HardwareInterface||flags.FilterInterface||flags.EndPointInterface||flags.NotMediaConnected||
           flags.Paused||row.OperStatus!=IfOperStatusUp||
           (row.Type!=IF_TYPE_ETHERNET_CSMACD&&row.Type!=IF_TYPE_IEEE80211))continue;
        result.push_back({row.InterfaceLuid.Value,row.InOctets,row.OutOctets});
    }
    return result;
}
}
