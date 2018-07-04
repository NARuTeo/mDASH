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

#include <ns3/log.h>
#include <ns3/uinteger.h>
#include <ns3/tcp-socket-factory.h>
#include <ns3/simulator.h>
#include <ns3/inet-socket-address.h>
#include <ns3/inet6-socket-address.h>
#include "http-header.h"
#include "dash-client.h"
#include <unistd.h>
#include <ctime>

NS_LOG_COMPONENT_DEFINE("DashClient");

namespace ns3
{

  NS_OBJECT_ENSURE_REGISTERED(DashClient);

  int DashClient::m_countObjs = 0;

  TypeId
  DashClient::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::DashClient").SetParent<Application>().AddConstructor<DashClient>()
            .AddAttribute("VideoId","The Id of the video that is played.", UintegerValue(0),
                MakeUintegerAccessor(&DashClient::m_videoId),MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("Remote","The address of the destination", AddressValue(),
                MakeAddressAccessor(&DashClient::m_peer), MakeAddressChecker())
            .AddAttribute("Protocol", "The type of TCP protocol to use.",TypeIdValue(TcpSocketFactory::GetTypeId()),
                MakeTypeIdAccessor(&DashClient::m_tid), MakeTypeIdChecker())
            .AddAttribute("TargetDt", "The target buffering time", TimeValue(Time("35s")),
                MakeTimeAccessor(&DashClient::m_target_dt), MakeTimeChecker())
            .AddAttribute("window", "The window for measuring the average throughput (Time)",TimeValue(Time("10s")),
                MakeTimeAccessor(&DashClient::m_window),MakeTimeChecker())
            .AddTraceSource("BitRate", "The request bitRates",
                MakeTraceSourceAccessor(&DashClient::tr_bitRate))
            .AddTraceSource("Buffer", "The buffer length",
                MakeTraceSourceAccessor(&DashClient::tr_currDt));
    return tid;
  }

  DashClient::DashClient() :
      m_rateChanges(0), m_target_dt("35s"), m_bitrateEstimate(0.0), m_segmentId(0), realBitRate(100000),
      m_targetDataRate(100000.0), m_smoothDataRate(100000.0), m_switchCount(0), m_switchFlag(1),
      m_minDt(Seconds(15)), m_maxDt(Seconds(30)), m_idleTime(Seconds(0)), inCount(0), deCount(0),
      m_segmentFetchTime(Seconds(0)), m_id(m_countObjs++), estc_sleep(false),
      m_socket(0), m_connected(false), m_totBytes(0), m_startedReceiving(Seconds(0)),
      m_sumDt(Seconds(0)), m_lastDt(Seconds(-1)), m_requestTime("0s"),
      m_segment_bytes(0), m_bitRate(100000), m_window(Seconds(10))
  {
    NS_LOG_FUNCTION(this);
    m_parser.SetApp(this); // So the parser knows where to send the received messages
  }

  DashClient::~DashClient()
  {
    NS_LOG_FUNCTION(this);
  }

  Ptr<Socket>
  DashClient::GetSocket(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_socket;
  }

  void
  DashClient::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);

    m_socket = 0;
    // chain up
    Application::DoDispose();
  }

// Application Methods
  void
  DashClient::StartApplication(void) // Called at time specified by Start
  {
    NS_LOG_FUNCTION(this);

    // Create the socket if not already
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);

        // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
        if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM
            && m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
          {
            NS_FATAL_ERROR("Using HTTP with an incompatible socket type. "
                "HTTP requires SOCK_STREAM or SOCK_SEQPACKET. "
                "In other words, use TCP instead of UDP.");
          }

        if (Inet6SocketAddress::IsMatchingType(m_peer))
          {
            m_socket->Bind6();
            std::cout << " client *************** bind6 ****************" << std::endl;
          }
        else if (InetSocketAddress::IsMatchingType(m_peer))
          {
            m_socket->Bind();
            std::cout << " client *************** bind ****************" << std::endl;
          }
        m_socket->Connect(m_peer);
        m_socket->SetRecvCallback(MakeCallback(&DashClient::HandleRead, this));
        m_socket->SetConnectCallback(
            MakeCallback(&DashClient::ConnectionSucceeded, this),
            MakeCallback(&DashClient::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&DashClient::DataSend, this));
        std::cout << " client *************** b ****************" << m_peer << std::endl;
      }
  }

  void
  DashClient::StopApplication(void) // Called at time specified by Stop
  {
    NS_LOG_FUNCTION(this);

    if (m_socket != 0)
      {
        m_socket->Close();
        m_connected = false;
        m_player.m_state = MPEG_PLAYER_DONE;
      }
    else
      {
        NS_LOG_WARN("DashClient found null socket to close in StopApplication");
      }
  }

// Private helpers

  void
  DashClient::RequestSegment()
  {

    NS_LOG_FUNCTION(this);

    if (m_connected == false)
      {
        return;
      }

    //sleep(m_idleTime.GetSeconds());
    Ptr<Packet> packet = Create<Packet>(100);

    HTTPHeader httpHeader;
    httpHeader.SetSeq(1);
    httpHeader.SetMessageType(HTTP_REQUEST);
    httpHeader.SetVideoId(m_videoId);
    httpHeader.SetResolution(m_bitRate);
    httpHeader.SetThroughPut(realBitRate);
    httpHeader.SetSegmentId(m_segmentId++);
    packet->AddHeader(httpHeader);

    int res = 0;
    if (((unsigned) (res = m_socket->Send(packet))) != packet->GetSize())
      {
        NS_FATAL_ERROR(
            "Oh oh. Couldn't send packet! res=" << res << " size=" << packet->GetSize());
      }

    m_requestTime = Simulator::Now();
    m_segment_bytes = 0;
    //std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## HERE ########"
    //                << " " << Simulator::Now() << " m_idleTime: " << m_idleTime << std::endl;

  }

  void
  DashClient::HandleRead(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
    m_parser.ReadSocket(socket);

  }

  void
  DashClient::ConnectionSucceeded(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("DashClient Connection succeeded");
    m_connected = true;
    RequestSegment();
    std::cout << " client *************** connect succeed ****************" << std::endl;
  }

  void
  DashClient::ConnectionFailed(Ptr<Socket> socket)
  {
      std::cout << " client *************** connect fail ****************" << std::endl;
    NS_LOG_FUNCTION(this << socket);NS_LOG_LOGIC(
        "DashClient, Connection Failed");
  }

  void
  DashClient::DataSend(Ptr<Socket>, uint32_t)
  {
    NS_LOG_FUNCTION(this);
    if (m_connected)
      { // Only send new data if the connection has completed

        NS_LOG_INFO("Something was sent");

      }
    else
      {
        NS_LOG_INFO("NOT CONNECTED!!!!");
      }
  }

  void
  DashClient::MessageReceived(Packet message)
  {
    NS_LOG_FUNCTION(this << message);

    MPEGHeader mpegHeader;
    HTTPHeader httpHeader;

    // Send the frame to the player
    m_player.ReceiveFrame(&message);
    m_segment_bytes += message.GetSize();
    m_totBytes += message.GetSize();

    message.RemoveHeader(mpegHeader);
    message.RemoveHeader(httpHeader);

    // Calculate the buffering time
    switch (m_player.m_state)
      {
    case MPEG_PLAYER_PLAYING:
      m_sumDt += m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
      break;
    case MPEG_PLAYER_PAUSED:
      break;
    case MPEG_PLAYER_DONE:
      return;
    default:
      NS_FATAL_ERROR("WRONG STATE");
      }

    // If we received the last frame of the segment
    if (mpegHeader.GetFrameId() == MPEG_FRAMES_PER_SEGMENT - 1)
      {
        m_segmentFetchTime = Simulator::Now() - m_requestTime;

        // other clients' average bitrate
        uint32_t totalRate = mpegHeader.GetTotalRate();
        uint32_t clientNo = mpegHeader.GetClientNo();
        uint32_t averageRate;
        if (clientNo == 1)
            averageRate = m_bitRate;
        else
            averageRate = (totalRate - m_bitRate)/(clientNo - 1);

        NS_LOG_INFO(
            Simulator::Now().GetSeconds() << " bytes: " << m_segment_bytes << " segmentTime: " << m_segmentFetchTime.GetSeconds() << " segmentRate: " << 8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());

        // Feed the bitrate info to the player
        realBitRate = 8 * m_segment_bytes / m_segmentFetchTime.GetSeconds();
        AddBitRate(Simulator::Now(), realBitRate, httpHeader.GetSegmentId());

        m_currDt = m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
        tr_currDt = m_currDt;
        // And tell the player to monitor the buffer level
        LogBufferLevel(m_currDt);

        m_bitRate = httpHeader.GetResolution();    // in estc, bitrate may be changed by server.
        uint32_t prevBitrate = m_bitRate;

        CalcNextSegment(prevBitrate, m_bitRate, averageRate);
        tr_bitRate = m_bitRate;

        // the fluctuation of network
        if (prevBitrate == m_bitRate)
        {
            m_bitSwitch[m_switchCount++] = 0;
            //std::cout << "+++ " << realBitRate << " = " << preRealBitRate << std::endl;
        }
        else if ((double(m_bitRate)-double(prevBitrate))*m_switchFlag > 0)
        {
            m_bitSwitch[m_switchCount++] = 0;
            m_switchFlag = double(m_bitRate)-double(prevBitrate);
            //std::cout << "+++0 " << realBitRate << " - " << preRealBitRate << " - " << m_switchFlag << std::endl;
        }
        else
        {
            m_bitSwitch[m_switchCount++] = 1;
            m_switchFlag = double(m_bitRate)-double(prevBitrate);
            //std::cout << "+++1 " << realBitRate << " - " << preRealBitRate << " - " << m_switchFlag << std::endl;
        }

        if (prevBitrate != m_bitRate)
          {
            m_rateChanges++;
          }

        //std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## begin ########"
        //            << " " << Simulator::Now() << " m_idleTime: " << m_idleTime << std::endl;
        //clock_t now = clock();
        //m_idleTime = Seconds(2);

        Simulator::Schedule(m_idleTime, &DashClient::RequestSegment, this);

        //while (clock()-now < Seconds(2).GetMilliSeconds())
        //    std::cout << " Now: " << now << " clock(): " << clock() << " m_idleTime: " << Simulator::Now().GetSeconds() << std::endl;
        //Time wait_tmp = Simulator::Now();
        //while(Simulator::Now()-wait_tmp <= m_idleTime)
        //    std::cout << " Now: " << Simulator::Now() << " wait_tmp: " << wait_tmp << " m_idleTime: " << m_idleTime << std::endl;;
        //m_condition.TimedWait(m_idleTime.GetNanoSeconds());
        //RequestSegment();
        //Ptr<SystemThread> st = Create<SystemThread> (
        //                    MakeCallback(&DashClient::RequestSegment, this));
        //st->Start();
        //std::thread m_task(&DashClient::RequestSegment, this, m_idleTime);
        //m_task.detach();
        //std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " $$$$$$$$$$$ end $$$$$$$$$" << " idleTime: " << m_idleTime.GetSeconds() << std::endl;
        //RequestSegment(m_idleTime);


        // the instability coeffitiont
        struct fetchRate fet_tmp;
        fet_tmp.str_bitrate = m_bitRate;
        double prevBitrate_tmp = prevBitrate;
        fet_tmp.str_diff = m_bitRate-prevBitrate_tmp;
        m_staRate[Simulator::Now()] = fet_tmp;
        double bitSum = 0.0;
        double diffSum = 0.0;
        Time sta_time = Simulator::Now();

        if (sta_time >= Seconds(20))
        {
            for (std::map<Time, struct fetchRate>::iterator it = m_staRate.begin();
                it != m_staRate.end(); it++)
            {
                if (it->first < (sta_time-Seconds(20)))
                    m_staRate.erase(it->first);
                else
                {
                    bitSum += it->second.str_bitrate*(20-sta_time.GetSeconds()+it->first.GetSeconds());
                    diffSum += fabs(it->second.str_diff)*(20-sta_time.GetSeconds()+it->first.GetSeconds());
                }
            }
            double t_stability = diffSum / bitSum;
            m_staCoeff[Simulator::Now()] = t_stability;
            int sta_Count = 0;
            double sta_sum = 0.0;
            for (std::map<Time, double>::iterator it = m_staCoeff.begin();
                it != m_staCoeff.end(); it++)
            {
                sta_sum += it->second;
                sta_Count++;
            }
            m_stability = sta_sum / sta_Count;
            //std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " sta_sum: " << sta_sum << " sta_cout: " << sta_Count
            //            << " diffSum: " << diffSum << " bitSum: " << bitSum << std::endl;
        }

        /*
        if (m_idleTime == Seconds(0))
            RequestSegment();
        else
        {
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "sleep!!!  :)" << m_idleTime.GetSeconds() << std::endl;
            sleep(m_idleTime.GetSeconds());
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "=================23232323232================="
                << m_idleTime.GetSeconds() << std::endl;
            RequestSegment();
        }
        */

        std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id
            << " newBitRate: " << m_bitRate << " oldBitRate: " << prevBitrate
            << " idleTime: " << m_idleTime.GetSeconds() << " interTime: "
            << m_player.m_interruption_time.GetSeconds() << " T: "
            << m_currDt.GetSeconds() << " dT: "
            << (m_lastDt >= 0 ? (m_currDt - m_lastDt).GetSeconds() : 0)
            << " tracedT: " << tr_currDt << std::endl;

        NS_LOG_INFO(
            "==== Last frame received. Requesting segment " << m_segmentId);

        m_lastDt = m_currDt;

      }

  }

  void
  DashClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate,
      double aveRate)
  {
    nextRate = currRate;
    m_idleTime = Seconds(0);
  }

  void
  DashClient::GetStats()
  {
    std::cout << " InterruptionTime: "
        << m_player.m_interruption_time.GetSeconds() << " interruptions: "
        << m_player.m_interrruptions << " avgRate: "
        << (1.0 * m_player.m_totalRate) / m_player.m_framesPlayed
        << " minRate: " << m_player.m_minRate << " AvgDt: "
        << m_sumDt.GetSeconds() / m_player.m_framesPlayed << " changes: "
        << m_rateChanges << " instability: " << m_stability << std::endl;
  }

  void
  DashClient::LogBufferLevel(Time t)
  {
    m_bufferState[Simulator::Now()] = t;
    for (std::map<Time, Time>::iterator it = m_bufferState.begin();
        it != m_bufferState.end(); ++it)
      {
        if (it->first < (Simulator::Now() - m_window))
          {
            m_bufferState.erase(it->first);
          }
      }
  }

  double
  DashClient::GetBufferEstimate()
  {
    double sum = 0;
    int count = 0;
    for (std::map<Time, Time>::iterator it = m_bufferState.begin();
        it != m_bufferState.end(); ++it)
      {
        sum += it->second.GetSeconds();
        count++;
      }
    return sum / count;
  }

  double
  DashClient::GetBufferDifferential()
  {
    std::map<Time, Time>::iterator it = m_bufferState.end();

    if (it == m_bufferState.begin())
      {
        // Empty buffer
        return 0;
      }
    it--;
    Time last = it->second;

    if (it == m_bufferState.begin())
      {
        // Only one element
        return 0;
      }
    it--;
    Time prev = it->second;
    return (last - prev).GetSeconds();
  }

  double
  DashClient::GetSegmentFetchTime()
  {
    return m_segmentFetchTime.GetSeconds();
  }

  // get average bitrate. used by estc and panda.
  void
  DashClient::AddBitRate(Time time, double bitrate, uint32_t segId)
  {
    m_bitrates[time] = bitrate;
    double sum = 0;
    int count = 0;
    for (std::map<Time, double>::iterator it = m_bitrates.begin();
        it != m_bitrates.end(); ++it)
      {
        if (it->first < (Simulator::Now() - Seconds(40)))
          {
            m_bitrates.erase(it->first);
          }
        else
          {
            sum += 1 / it->second;
            count++;
          }
      }
    if (segId < 20)
        m_bitrateEstimate = bitrate;
    else
        m_bitrateEstimate = count / sum;
  }

} // Namespace ns3
