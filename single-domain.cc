/*
 * Copyright (c) 2014 University of Campinas (Unicamp)
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
 * Author: Luciano Jerez Chaves <ljerezchaves@gmail.com>
 *         Vitor M. Eichemberger <vitor.marge@gmail.com>
 */

/*
 * Two hosts connected to different OpenFlow switches.
 * Both switches are managed by the default learning controller application.
 *
 *                       Learning Controller
 *                                |
 *                         +-------------+
 *                         |             |
 *                  +----------+     +----------+
 *       Host 0 === | Switch 0 | === | Switch 1 | === Host 1
 *                  +----------+     +----------+
 */

#include <ns3/core-module.h>
#include <ns3/csma-module.h>
#include <ns3/internet-apps-module.h>
#include <ns3/internet-module.h>
#include <ns3/network-module.h>
#include <ns3/ofswitch13-module.h>
#include <ns3/applications-module.h>
#include <ns3/flow-monitor-module.h>

using namespace ns3;

int
main(int argc, char* argv[])
{
    uint16_t simTime = 10;
    bool verbose = false;
    bool trace = false;
    uint32_t SentPackets = 0;
    uint32_t ReceivedPackets = 0;
    uint32_t LostPackets = 0;

    // Configure command line parameters
    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("trace", "Enable datapath stats and pcap traces", trace);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        OFSwitch13Helper::EnableDatapathLogs();
        LogComponentEnable("OFSwitch13Interface", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13Device", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13Port", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13Queue", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13Controller", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13LearningController", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13Helper", LOG_LEVEL_ALL);
        LogComponentEnable("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
    }

    // Enable checksum computations (required by OFSwitch13 module)
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create two host nodes
    NodeContainer hosts;
    hosts.Create(20);

    // Create two switch nodes
    NodeContainer switches;
    switches.Create(2);

    // Use the CsmaHelper to connect hosts and switches
    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer pair;
    NetDeviceContainer pairDevs;
    NetDeviceContainer hostDevices;
    NetDeviceContainer switchPorts[2];
    switchPorts[0] = NetDeviceContainer();
    switchPorts[1] = NetDeviceContainer();
    
    for (int i = 0; i < 10; ++i) {
        NodeContainer pair = NodeContainer(hosts.Get(i), switches.Get(0));
        NetDeviceContainer pairDevs = csmaHelper.Install(pair);
        hostDevices.Add(pairDevs.Get(0));
        switchPorts[0].Add(pairDevs.Get(1));
     }
    for (int i = 10; i < 20; ++i) {
        NodeContainer pair = NodeContainer(hosts.Get(i), switches.Get(1));
        NetDeviceContainer pairDevs = csmaHelper.Install(pair);
        hostDevices.Add(pairDevs.Get(0));
        switchPorts[1].Add(pairDevs.Get(1));
     }
    // Connect host 0 to first switch
    //pair = NodeContainer(hosts.Get(0), switches.Get(0));
    //pairDevs = csmaHelper.Install(pair);
    //hostDevices.Add(pairDevs.Get(0));
    //switchPorts[0].Add(pairDevs.Get(1));

    // Connect host 1 to second switch
    //pair = NodeContainer(hosts.Get(1), switches.Get(1));
    //pairDevs = csmaHelper.Install(pair);
    //hostDevices.Add(pairDevs.Get(0));
    //switchPorts[1].Add(pairDevs.Get(1));

    // Connect the switches
    pair = NodeContainer(switches.Get(0), switches.Get(1));
    pairDevs = csmaHelper.Install(pair);
    switchPorts[0].Add(pairDevs.Get(0));
    switchPorts[1].Add(pairDevs.Get(1));

    // Create the controller node
    Ptr<Node> controllerNode = CreateObject<Node>();

    // Configure the OpenFlow network domain
    Ptr<OFSwitch13InternalHelper> of13Helper = CreateObject<OFSwitch13InternalHelper>();
    of13Helper->InstallController(controllerNode);
    of13Helper->InstallSwitch(switches.Get(0), switchPorts[0]);
    of13Helper->InstallSwitch(switches.Get(1), switchPorts[1]);
    of13Helper->CreateOpenFlowChannels();

    // Install the TCP/IP stack into hosts nodes
    InternetStackHelper internet;
    internet.Install(hosts);

    // Set IPv4 host addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    ipv4.Assign (hostDevices);
    
    
    // Create an OnOff application to send UDP datagrams from n0 to n1.
    uint16_t port = 9;   // Discard port (RFC 863)
    OnOffHelper onoff ("ns3::TcpSocketFactory",
                       Address (InetSocketAddress (Ipv4Address ("10.1.1.4"), port)));
    onoff.SetConstantRate (DataRate ("100kb/s"));
    ApplicationContainer app = onoff.Install (hosts.Get (0));
    // Start the application
    app.Start (Seconds (1.0));
    app.Stop (Seconds (10.0));
    

    // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
    if (trace)
    {
        of13Helper->EnableOpenFlowPcap("openflow");
        of13Helper->EnableDatapathStats("switch-stats");
        csmaHelper.EnablePcap("switch", switchPorts[0], true);
        csmaHelper.EnablePcap("switch", switchPorts[1], true);
        csmaHelper.EnablePcap("host", hostDevices);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Run();
    int j=0;
    float AvgThroughput = 0;
    Time Jitter;
    Time Delay;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

        NS_LOG_UNCOND("----Flow ID:" <<iter->first);
        NS_LOG_UNCOND("Src Addr" <<t.sourceAddress << "Dst Addr "<< t.destinationAddress);
        NS_LOG_UNCOND("Sent Packets=" <<iter->second.txPackets);
        NS_LOG_UNCOND("Received Packets =" <<iter->second.rxPackets);
        NS_LOG_UNCOND("Lost Packets =" <<iter->second.txPackets-iter->second.rxPackets);
        NS_LOG_UNCOND("Packet delivery ratio =" <<iter->second.rxPackets*100/iter->second.txPackets << "%");
        NS_LOG_UNCOND("Packet loss ratio =" << (iter->second.txPackets-iter->second.rxPackets)*100/iter->second.txPackets << "%");
        NS_LOG_UNCOND("Delay =" <<iter->second.delaySum);
        NS_LOG_UNCOND("Jitter =" <<iter->second.jitterSum);
        NS_LOG_UNCOND("Throughput =" <<iter->second.rxBytes * 8.0/(iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds())/1024<<"Kbps");

        SentPackets = SentPackets +(iter->second.txPackets);
        ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
        LostPackets = LostPackets + (iter->second.txPackets-iter->second.rxPackets);
        AvgThroughput = AvgThroughput + (iter->second.rxBytes * 8.0/(iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds())/1024);
        Delay = Delay + (iter->second.delaySum);
        Jitter = Jitter + (iter->second.jitterSum);

        j = j + 1;

    }

    AvgThroughput = AvgThroughput/j;
    NS_LOG_UNCOND("--------Total Results of the simulation----------"<<std::endl);
    NS_LOG_UNCOND("Total sent packets  =" << SentPackets);
    NS_LOG_UNCOND("Total Received Packets =" << ReceivedPackets);
    NS_LOG_UNCOND("Total Lost Packets =" << LostPackets);
    NS_LOG_UNCOND("Packet Loss ratio =" << ((LostPackets*100)/SentPackets)<< "%");
    NS_LOG_UNCOND("Packet delivery ratio =" << ((ReceivedPackets*100)/SentPackets)<< "%");
    NS_LOG_UNCOND("Average Throughput =" << AvgThroughput<< "Kbps");
    NS_LOG_UNCOND("End to End Delay =" << Delay);
    NS_LOG_UNCOND("End to End Jitter delay =" << Jitter);
    NS_LOG_UNCOND("Total Flod id " << j);
    monitor->SerializeToXmlFile("manet-routing.xml", true, true);
    Simulator::Destroy();
}
