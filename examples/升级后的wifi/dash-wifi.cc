/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 TEI of Western Macedonia, Greece
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Dimitrios J. Vergados <djvergad@gmail.com>
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/dash-module.h"
#include "ns3/stats-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Dash-Wifi");

int
main(int argc, char *argv[])
{
    uint32_t nWifi = 5;
    uint32_t maxBytes = 0;
    uint32_t users = 3;
    uint32_t bulkNo = 1;
    double target_dt = 35.0;
    double stopTime = 100.0;
    std::string linkRate = "10Mbps";
    std::string delay = "5ms";
    std::string protocol = "ns3::DashClient";
    std::string window = "10s";

    LogComponentEnable ("DashServer", LOG_LEVEL_ALL);
    LogComponentEnable ("DashClient", LOG_LEVEL_ALL);

  //
  // Allow the user to override any of the defaults at
  // run-time, via command-line arguments
  //
    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send",maxBytes);
    cmd.AddValue("users", "The number of concurrent videos", users);
    cmd.AddValue("bulkNo", "The number of background TCP transfers", bulkNo);
    cmd.AddValue("targetDt",
      "The target time difference between receiving and playing a frame.",
      target_dt);
    cmd.AddValue("stopTime",
      "The time when the clients will stop requesting segments", stopTime);
    cmd.AddValue("linkRate",
      "The bitrate of the link connecting the clients to the server (e.g. 500kbps)",
      linkRate);
    cmd.AddValue("delay",
      "The delay of the link connecting the clients to the server (e.g. 5ms)",
      delay);
    cmd.AddValue("protocol",
      "The adaptation protocol. It can be 'ns3::DashClient' or 'ns3::OsmpClient (for now).",
      protocol);
    cmd.AddValue("window",
      "The window for measuring the average throughput (Time).", window);
    cmd.Parse(argc, argv);

    if (bulkNo + users > nWifi + 1)   // users less than 8
    {
      std::cerr << "you need more stations" << std::endl;
      return -1;
    }


    //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer p2pNodes;
    p2pNodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (linkRate));
    pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install (p2pNodes);

    NodeContainer wifiApNode = p2pNodes.Get (1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    std::string phyMode ("DsssRate1Mbps");
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
        "Exponent", DoubleValue (3.0),
        "ReferenceLoss", DoubleValue (40.0459));
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode",StringValue (phyMode),
        "ControlMode",StringValue (phyMode));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX",DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(3),
        "LayoutType",StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");   // keep current position
    mobility.Install (wifiApNode);

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install (wifiApNode);
    stack.Install (p2pNodes.Get (0));

    Ipv4AddressHelper address;

    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign (p2pDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaInterfaces;
    Ipv4InterfaceContainer wifiApInterfaces;
    wifiApInterfaces = address.Assign (apDevices);
    wifiStaInterfaces = address.Assign (staDevices);


    std::vector<std::string> protocols;
    std::stringstream ss(protocol);
    std::string proto;
    uint32_t protoNum = 0; // The number of protocols (algorithms)
    while (std::getline(ss, proto, ',') && protoNum++ < users)
    {
      protocols.push_back(proto);
    }

    uint16_t port = 80;  // well-known echo port number

    std::vector<DashClientHelper> clients;
    std::vector<ApplicationContainer> clientApps;

    for (uint32_t user = 0; user < users; user++)
    {
      DashClientHelper client("ns3::TcpSocketFactory",
          InetSocketAddress(p2pInterfaces.GetAddress(0), port),protocols[user % protoNum]);
      std::cout << user << " client: " << p2pInterfaces.GetAddress(0) << std::endl;
      //client.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
      client.SetAttribute("VideoId", UintegerValue(user + 1)); // VideoId should be positive
      client.SetAttribute("TargetDt", TimeValue(Seconds(target_dt)));
      client.SetAttribute("window", TimeValue(Time(window)));
      ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(user));
      clientApp.Start(Seconds(0.25));
      clientApp.Stop(Seconds(stopTime));

      clients.push_back(client);
      clientApps.push_back(clientApp);

    }

    DashServerHelper server("ns3::TcpSocketFactory",
      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = server.Install(p2pNodes.Get (0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(stopTime + 5.0));

    for (uint32_t bulk = 0; bulk < bulkNo; bulk++)
    {
      BulkSendHelper source("ns3::TcpSocketFactory",
          InetSocketAddress(wifiStaInterfaces.GetAddress(bulk+4), port));

      source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
      ApplicationContainer sourceApps = source.Install(p2pNodes.Get(0));
      sourceApps.Start(Seconds(0.0));
      sourceApps.Stop(Seconds(stopTime));

      PacketSinkHelper sink("ns3::TcpSocketFactory",
          InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApps = sink.Install(
          wifiStaNodes.Get(bulk+4));
      sinkApps.Start(Seconds(0.0));
      sinkApps.Stop(Seconds(stopTime));

    }


    //
    GnuplotHelper plotHelper21;
    plotHelper21.ConfigurePlot ("wifi-bitrates1",
                             "bitrates",
                             "Time (seconds)",
                             "bitrates");
    plotHelper21.PlotProbe ("ns3::Uinteger32Probe",
                         "/NodeList/4/ApplicationList/*/$ns3::DashClient/BitRate",
                         "Output",
                         "client",
                         GnuplotAggregator::KEY_INSIDE);
    GnuplotHelper plotHelper22;
    plotHelper22.ConfigurePlot ("wifi-buffer1",
                              "buffer",
                              "Time (seconds)",
                              "buffer");
    plotHelper22.PlotProbe ("ns3::TimeProbe",
                          "/NodeList/4/ApplicationList/*/$ns3::DashClient/Buffer",
                          "Output",
                          "client",
                          GnuplotAggregator::KEY_INSIDE);
    //
    GnuplotHelper plotHelper31;
    plotHelper31.ConfigurePlot ("wifi-bitrates2",
                             "bitrates",
                             "Time (seconds)",
                             "bitrates");
    plotHelper31.PlotProbe ("ns3::Uinteger32Probe",
                         "/NodeList/3/ApplicationList/*/$ns3::DashClient/BitRate",
                         "Output",
                         "client",
                         GnuplotAggregator::KEY_INSIDE);
    GnuplotHelper plotHelper32;
    plotHelper32.ConfigurePlot ("wifi-buffer2",
                              "buffer",
                              "Time (seconds)",
                              "buffer");
    plotHelper32.PlotProbe ("ns3::TimeProbe",
                          "/NodeList/3/ApplicationList/*/$ns3::DashClient/Buffer",
                          "Output",
                          "client",
                          GnuplotAggregator::KEY_INSIDE);
    //
    GnuplotHelper plotHelper41;
    plotHelper41.ConfigurePlot ("wifi-bitrates3",
                             "bitrates",
                             "Time (seconds)",
                             "bitrates");
    plotHelper41.PlotProbe ("ns3::Uinteger32Probe",
                         "/NodeList/2/ApplicationList/*/$ns3::DashClient/BitRate",
                         "Output",
                         "client",
                         GnuplotAggregator::KEY_INSIDE);
    GnuplotHelper plotHelper42;
    plotHelper42.ConfigurePlot ("wifi-buffer3",
                              "buffer",
                              "Time (seconds)",
                              "buffer");
    plotHelper42.PlotProbe ("ns3::TimeProbe",
                          "/NodeList/2/ApplicationList/*/$ns3::DashClient/Buffer",
                          "Output",
                          "client",
                          GnuplotAggregator::KEY_INSIDE);


    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


    Simulator::Stop(Seconds(stopTime));
    AnimationInterface anim("dash-wifi.xml");
    Simulator::Run();
    Simulator::Destroy();

    uint32_t k;
    for (k = 0; k < users; k++)
    {
      Ptr<DashClient> app = DynamicCast<DashClient>(clientApps[k].Get(0));
      std::cout << protocols[k % protoNum] << "-Node: " << k;
      app->GetStats();
    }

    Ptr<DashServer> serapp = DynamicCast<DashServer>(serverApps.Get(0));
    serapp->GetStats();

  return 0;
}
