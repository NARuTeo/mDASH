/*
 * panda-client.cc
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#include "panda-client.h"
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/dash-client.h>

NS_LOG_COMPONENT_DEFINE("PandaClient");

namespace ns3
{
  NS_OBJECT_ENSURE_REGISTERED(PandaClient);

  TypeId
  PandaClient::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::PandaClient").SetParent<DashClient>().AddConstructor<
            PandaClient>();
    return tid;
  }

  PandaClient::PandaClient():
      p_minDt(Seconds(26)), fast_start(false)
  {
    // TODO Auto-generated constructor stub

  }

  PandaClient::~PandaClient()
  {
    // TODO Auto-generated destructor stub
  }

  void
  PandaClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate,
      double aveRate)
  {
    double k = 0.5, w = 0.3, e = 0.15;  // k. a was 0.14  0.2
    double a = 0.75, b = 0.8;
    double O_up, O_down = 0.0;

    double preTargetDataRate = m_targetDataRate;
    double aa1 = (preTargetDataRate - realBitRate)/1000000.0 + w;
    m_targetDataRate = (aa1>0 ? w-aa1 : w)*k*m_segmentFetchTime.GetSeconds() * 1000000 + preTargetDataRate;
    m_smoothDataRate = (1-a*m_segmentFetchTime.GetSeconds())*m_smoothDataRate + a*m_segmentFetchTime.GetSeconds()*m_targetDataRate;
    std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/PandaClient/ T: " << m_segmentFetchTime.GetSeconds()
        << " 1st: " << (1-a*m_segmentFetchTime.GetSeconds())*m_smoothDataRate << " 2st: " << a*m_segmentFetchTime.GetSeconds()*m_targetDataRate
        << std::endl;

    O_up = e*m_smoothDataRate;
    uint32_t rate_up = GetBitRate(m_smoothDataRate - O_up);
    uint32_t rate_down = GetBitRate(m_smoothDataRate - O_down);
    if (currRate < rate_up)
    {
        nextRate = rate_up;
        std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/PandaClient-- 1 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << std::endl;
    }
    else if (currRate >= rate_up && currRate <= rate_down)
    {
        nextRate = currRate;
        std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/PandaClient-- 2 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << std::endl;
    }
    else
    {
        nextRate = rate_down;
        std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/PandaClient-- 3 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << std::endl;
    }

    if (m_currDt <= p_minDt)
        m_idleTime = Seconds(0);
    else
        m_idleTime = Seconds(b*(m_currDt.GetSeconds()-p_minDt.GetSeconds()));

    std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/PandaClient/ aa1: " << aa1
        << " preTarget: " << preTargetDataRate << " nowtarget: " << m_targetDataRate << " smooth: " << m_smoothDataRate
        << " idleTime: " << m_idleTime.GetSeconds() << " fetchTime: " <<  m_segmentFetchTime.GetSeconds() << std::endl;

    // simple fast start logic. Copy form estc.
    uint32_t rates[] =
      { 100000, 200000, 300000, 400000, 500000, 600000, 700000, 900000, 1000000, 1200000,
          1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 4500000, 5000000, 5500000, 6000000 };
    int i=0;
    while (currRate != rates[i])
        i++;
    if (m_bitrateEstimate > currRate && fast_start)
    {
        nextRate = rates[i+1];
        m_targetDataRate = rates[i];
        m_smoothDataRate = rates[i];
    }
    if (nextRate == 2000000 && fast_start)
    {
        fast_start = false;
        m_targetDataRate = rates[i];
        m_smoothDataRate = rates[i];
    }
  }

  uint32_t
  PandaClient::GetBitRate(double throughtPut)
  {
      uint32_t rates[] =
        { 100000, 200000, 300000, 400000, 500000, 600000, 700000, 900000, 1000000, 1200000,
          1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 4500000, 5000000, 5500000, 6000000 };
      uint32_t rates_size = sizeof(rates) / sizeof(rates[0]);

      if (throughtPut >= rates[rates_size-1])
        return rates[rates_size-1];
      else if (throughtPut <= rates[0])
        return rates[0];
      else
        for (unsigned int i = 1; i < rates_size; i++)
            if (throughtPut < rates[i])
                return rates[i-1];
      return rates[0];
  }
} /* namespace ns3 */
