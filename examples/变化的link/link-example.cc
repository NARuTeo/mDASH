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
 * Revised: Dong Hanze
 */

// Network topology
//
//       n0 ----------- n1
//            500 Kbps
//             5 ms
//
#include <string>
#include <fstream>
#include <sstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/dash-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DashExample");

void
CallBack(Ptr<NetDevice> device, std::string rate)
{
  std::cout << "============================================\n"
      "=== CALLBACK: new rate: " << rate << " =====\n"
      "============================================" << std::endl;

  device->SetAttribute("DataRate", StringValue(rate));

}

void
SwitchCallBack(Ptr<NetDevice> device, const std::string & switchTime,
    const std::string & lowRate, const std::string & highRate, bool flag)
{

  std::cout << "============================================\n"
      "=== CALLBACK: new rate: " << (flag ? lowRate : highRate) << " =====\n"
      "============================================" << std::endl;

  device->SetAttribute("DataRate",
      flag ? StringValue(lowRate) : StringValue(highRate));
  flag = !flag;

  Simulator::Schedule(Time(switchTime), SwitchCallBack, device, switchTime,
      lowRate, highRate, flag);
}

void
ServerCallBack(Ptr<Application> device, uint32_t rate)
{
  std::cout << "============================================\n"
      "=== ServerCALLBACK: new rate: " << rate << " =====\n"
      "============================================" << std::endl;

  device->SetAttribute("Bw", UintegerValue(rate));

}

void
ServerSwitchCallBack(Ptr<Application> device, const std::string & switchTime,
    uint32_t &lowRate, uint32_t &highRate, bool flag)
{

  std::cout << "============================================\n"
      "=== ServerCALLBACK: new rate: " << (flag ? lowRate : highRate) << " =====\n"
      "============================================" << std::endl;

  device->SetAttribute("Bw",
      flag ? UintegerValue(lowRate) : UintegerValue(highRate));
  flag = !flag;

  Simulator::Schedule(Time(switchTime), ServerSwitchCallBack, device, switchTime,
      lowRate, highRate, flag);
}

int
main(int argc, char *argv[])
{
  bool tracing = true;
  uint32_t maxBytes = 0;
  uint32_t users = 4;
  double target_dt = 35.0;
  double stopTime = 500.0;  // initial as 500
  double linkHigh = 100.0;
  double linkLow = 300.0;
  std::string linkRate = "10000000";
  std::string highRate = "5000000";
  std::string lowRate = "10000000";
  std::string delay = "5ms";
  std::string switchTime = "10s";
  std::string switchStart = "200s";
  std::string protocol = "ns3::DashClient";
  std::string window = "10s";

  uint32_t linkRate_tmp;
  std::stringstream linkstream(linkRate);
  linkstream >> linkRate_tmp;
  uint32_t highRate_tmp;
  std::stringstream highstream(highRate);
  highstream >> highRate_tmp;
  uint32_t lowRate_tmp;
  std::stringstream lowstream(lowRate);
  lowstream >> lowRate_tmp;

  LogComponentEnable ("DashServer", LOG_LEVEL_ALL);
  LogComponentEnable ("DashClient", LOG_LEVEL_ALL);

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue("maxBytes", "Total number of bytes for application to send",
      maxBytes);
  cmd.AddValue("users", "The number of concurrent videos", users);
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
  cmd.AddValue("switchTime",
      "The time spent on each state (HIGH or LOW). 0 for no switch",
      switchTime);
  cmd.AddValue("protocol",
      "The adaptation protocol. It can be 'ns3::DashClient' or 'ns3::OsmpClient (for now).",
      protocol);
  cmd.AddValue("window",
      "The window for measuring the average throughput (Time).", window);
  cmd.Parse(argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO("Create nodes.");
  NodeContainer nodes;
  nodes.Create(2);

  NS_LOG_INFO("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(linkRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);


//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  internet.Install(nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  NS_LOG_INFO("Create Applications.");

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
          InetSocketAddress(i.GetAddress(1), port), protocols[user % protoNum]);
      //client.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
      client.SetAttribute("TargetDt", TimeValue(Seconds(target_dt)));
      client.SetAttribute("VideoId", UintegerValue(user + 1)); // VideoId should be positive
      client.SetAttribute("window",TimeValue(Time(window)));

      ApplicationContainer clientApp = client.Install(nodes.Get(0));
      clientApp.Start(Seconds(0.25));
      clientApp.Stop(Seconds(stopTime));

      clients.push_back(client);
      clientApps.push_back(clientApp);

    }

  DashServerHelper server("ns3::TcpSocketFactory",
      InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(stopTime + 5.0));

  // if want to set bandwidth of server and client respectively
  //devices.Get(0)->SetAttribute("DataRate", StringValue(linkRate));
  //devices.Get(1)->SetAttribute("DataRate", StringValue("10000000"));


  if (Time(switchTime) == Seconds(0))
    {
      Simulator::Schedule(Seconds(linkHigh), CallBack, devices.Get(0),
          highRate);
      Simulator::Schedule(Seconds(linkLow), CallBack, devices.Get(0), lowRate);
      Simulator::Schedule(Seconds(linkHigh), CallBack, devices.Get(1),
          highRate);
      Simulator::Schedule(Seconds(linkLow), CallBack, devices.Get(1), lowRate);
      Simulator::Schedule(Seconds(linkHigh), ServerCallBack, serverApps.Get(0), highRate_tmp);
      Simulator::Schedule(Seconds(linkLow), ServerCallBack, serverApps.Get(0), lowRate_tmp);
    }
  else
    {
      Simulator::Schedule(Time(switchStart), SwitchCallBack, devices.Get(0),
          switchTime, lowRate, highRate, false);
      Simulator::Schedule(Time(switchStart), SwitchCallBack, devices.Get(1),
          switchTime, lowRate, highRate, false);
      Simulator::Schedule(Time(switchStart), ServerSwitchCallBack, serverApps.Get(0),
          switchTime, lowRate_tmp, highRate_tmp, false);
    }


  // to solve "unused varialbe" error
/*  if (Time(switchTime)==Seconds(0))
  {
      linkHigh = linkLow;
      linkLow = linkHigh;
  }
*/



  GnuplotHelper plotHelper1;
  plotHelper1.ConfigurePlot ("dash-bitrates",
                             "bitrates",
                             "Time (seconds)",
                             "bitrates");
  plotHelper1.PlotProbe ("ns3::Uinteger32Probe",
                         "/NodeList/0/ApplicationList/*/$ns3::DashClient/BitRate",
                         "Output",
                         "client",
                         GnuplotAggregator::KEY_INSIDE);

  GnuplotHelper plotHelper2;
  plotHelper2.ConfigurePlot ("dash-buffer",
                              "buffer",
                              "Time (seconds)",
                              "buffer");
  plotHelper2.PlotProbe ("ns3::TimeProbe",
                          "/NodeList/0/ApplicationList/*/$ns3::DashClient/Buffer",
                          "Output",
                          "client",
                          GnuplotAggregator::KEY_INSIDE);





//
// Set up tracing if enabled
//
  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll(ascii.CreateFileStream("dash-send.tr"));
      pointToPoint.EnablePcapAll("dash-send", false);
    }

//
// Now, do the actual simulation.
//
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  uint32_t k;
  for (k = 0; k < users; k++)
    {
      Ptr<DashClient> app = DynamicCast<DashClient>(clientApps[k].Get(0));
      std::cout << protocols[k % protoNum] << "-Node: " << k;
      app->GetStats();
    }

  Ptr<DashServer> serapp = DynamicCast<DashServer>(serverApps.Get(0));
  serapp->GetStats();

}
