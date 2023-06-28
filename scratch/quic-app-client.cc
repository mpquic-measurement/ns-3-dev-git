/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/fd-net-device-helper.h"
#include "ns3/fd-net-device-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/quic-client-server-helper.h"
#include "ns3/quic-module.h"
#include <fstream>
#include <string>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("QuicPacingExample");

static void PingRtt(string context, uint16_t seqNo, Time rtt) {
  NS_LOG_UNCOND("Received " << seqNo << " Response with RTT = " << rtt);
}

int main(int argc, char *argv[]) {
  GlobalValue::Bind("SimulatorImplementationType",
                    StringValue("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

  string deviceName("eth0");
  string localAddress("172.17.0.3");
  string remoteAddress("172.17.0.1");
  string localGateway("172.17.0.1");
  Ipv4Mask localMask("255.255.255.0");

  Ipv4Address serverAddress(remoteAddress.c_str());
  Ipv4Address localIp(localAddress.c_str());

  // Create Node
  NS_LOG_INFO("Create client node.");
  Ptr<Node> clientNode = CreateObject<Node>();

  NS_LOG_INFO("Create Device");
  FdNetDeviceHelper *helper = nullptr;
  EmuFdNetDeviceHelper *raw = new EmuFdNetDeviceHelper;
  raw->SetDeviceName(deviceName);
  helper = raw;

  NetDeviceContainer devices = helper->Install(clientNode);
  Ptr<NetDevice> device = devices.Get(0);
  device->SetAttribute("Address", Mac48AddressValue(Mac48Address::Allocate()));

  NS_LOG_INFO("Install QUIC Stack");
  // InternetStackHelper internetStack;
  // internetStack.Install(clientNode);
  QuicHelper stack;
  stack.InstallQuic(clientNode);

  NS_LOG_INFO("Create IPv4 Interface");
  Ptr<Ipv4> ipv4 = clientNode->GetObject<Ipv4>();
  uint32_t interface = ipv4->AddInterface(device);
  Ipv4InterfaceAddress address = Ipv4InterfaceAddress(localIp, localMask);
  ipv4->AddAddress(interface, address);
  ipv4->SetMetric(interface, 1);
  ipv4->SetUp(interface);

  NS_LOG_INFO("Setup Static Routing");
  Ipv4Address gateway(localGateway.c_str());
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting =
      ipv4RoutingHelper.GetStaticRouting(ipv4);
  staticRouting->SetDefaultRoute(gateway, interface);

#if 0
  NS_LOG_INFO("Create Ping App");
  Ptr<Ping> app = CreateObject<Ping>();
  app->SetAttribute("Destination", AddressValue(serverAddress));
  app->SetAttribute("VerboseMode", EnumValue(Ping::VerboseMode::VERBOSE));
  clientNode->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(60.0));

  Names::Add("app", app);
  Config::Connect("/Names/app/Rtt", MakeCallback(&PingRtt));

#endif

#if 0
  NS_LOG_INFO("Create QUIC App");
  QuicClientHelper quicClient(serverAddress, 2048);
  ApplicationContainer clientApps;
  clientApps.Add(quicClient.Install(clientNode));

  clientApps.Stop(Seconds(10));
  clientApps.Start(Seconds(2.0));

  // Packet::EnableChecking();
  Packet::EnablePrinting();
#endif

#if 1
  QuicClientHelper quicClient(serverAddress, 2048);
  quicClient.SetAttribute("Interval", TimeValue(MicroSeconds(1000.0)));
  quicClient.SetAttribute("PacketSize", UintegerValue(1000));
  quicClient.SetAttribute("MaxPackets", UintegerValue(10000000));

  ApplicationContainer clientApps;
  clientApps.Add(quicClient.Install(clientNode));

  clientApps.Stop(Seconds(10));
  clientApps.Start(Seconds(1));

  // Packet::EnableChecking();
  Packet::EnablePrinting();
#endif

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  helper->EnablePcapAll("quic", true);
  helper->EnableAsciiAll("quic.tr");

  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(15));
  Simulator::Run();

  delete helper;
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000
              << " Mbps\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000
              << " Mbps\n";
  }

  Simulator::Destroy();
  NS_LOG_INFO("Done.");
}
