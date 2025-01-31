/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

// Network topology
//
//       n0 ----------- n1
//            40 Gbps
//            0.01 ms

// This programs illustrates how QUIC pacing can be used and how user can set
// pacing rate. The program gives information about each flow like transmitted
// and received bytes (packets) and throughput of that flow. Currently, it is 
// using QUIC NewReno-like but in future after having congestion control algorithms 
// which can change pacing rate can be used.

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/quic-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/quic-client-server-helper.h"


using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("QuicPacingExample");

int
main (int argc, char *argv[])
{
  uint32_t QUICFlows = 1;
  bool isPacingEnabled = false;
  std::string pacingRate = "1Mbps";

  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (isPacingEnabled));

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("0.01ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  QuicHelper stack;
  stack.InstallQuic (nodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  NS_LOG_INFO ("Create Applications.");

  ApplicationContainer sourceApps;
  ApplicationContainer destApps;
  for (uint32_t iterator = 0; iterator < QUICFlows; iterator++)
  {
      QuicServerHelper source(2048);
      sourceApps.Add(source.Install(nodes.Get(0)));

      cout << "server address: " << i.GetAddress(0) << endl;
      QuicClientHelper dest(Ipv4Address(i.GetAddress(0)), 2048);

      destApps.Add(dest.Install(nodes.Get(1)));
  }

  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10));

  destApps.Start (Seconds (2.0));
  destApps.Stop (Seconds (10));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("quic-pacing.tr"));
  pointToPoint.EnablePcapAll ("quic-pacing", false);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.sourceAddress == "10.1.1.2")
      {
          continue;
      }
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
  }

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
