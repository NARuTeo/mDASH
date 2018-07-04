/*
 * aaash-client.cc
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#include "aaash-client.h"
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/dash-client.h>
#include <algorithm>

NS_LOG_COMPONENT_DEFINE("AaashClient");

namespace ns3
{
  NS_OBJECT_ENSURE_REGISTERED(AaashClient);

  TypeId
  AaashClient::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::AaashClient").SetParent<DashClient>().AddConstructor<
            AaashClient>();
    return tid;
  }

  AaashClient::AaashClient() :
      fast_start(false), a_m(0)
  {
    // TODO Auto-generated constructor stub

  }

  AaashClient::~AaashClient()
  {
    // TODO Auto-generated destructor stub
  }

  void
  AaashClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate,
      double aveRate)
  {
      double k = 0.2, w = 0.3, u = 0.08;  // u used to be 0.08,k a was 0.14, 0.2
      double a = 0.2, b = 0.8;
      double O_up, O_down = 0.0, e = 0.15;

      double preTargetDataRate = m_targetDataRate;
      //double preSmoothDataRate = m_smoothDataRate;
      double aa1 = (preTargetDataRate - realBitRate)/1000000.0 + w;
      //double aa1 = (preTargetDataRate - realBitRate)/1000000.0 - 0.1;
      double aa2 = u*(aveRate-currRate)/std::min(aveRate, double(currRate));
      //m_targetDataRate = ((aa1>0 ? -(0.1+aa1) : w)*k*m_segmentFetchTime.GetSeconds() + (std::fabs(aa2)>0.3 ? aa2 : 0))*
      //  1000000 + preTargetDataRate;
      if (a_m>0)
      {
          aa2 = 0;
          a_m = a_m - 1;
      }
      //m_targetDataRate = ((aa1>0 ? w-aa1 : w)*k*m_segmentFetchTime.GetSeconds() + aa2)*
      //  1000000 + preTargetDataRate;
      double m_FT = m_segmentFetchTime.GetSeconds();
      if (m_FT < 0.5)
        m_FT = 0.5;
      else if (m_FT > 4.95)
        m_FT = 4.95;
      m_targetDataRate = ((aa1>0 ? w-aa1 : w)*k*m_FT + aa2)*
        1000000 + preTargetDataRate;
      m_smoothDataRate = (1-a*m_FT)*m_smoothDataRate + a*m_FT*m_targetDataRate;
      //m_smoothDataRate = (1-a)*m_smoothDataRate + a*m_targetDataRate;
/*
      a_smoothBits[Simulator::Now()] = m_targetDataRate;
      double s_sum = 0.0;
      int s_count = 0;
      for (std::map<Time, double>::iterator it = a_smoothBits.begin();
        it != a_smoothBits.end(); ++it)
      {
          if (it->first < (Simulator::Now() - Seconds(40)))
          {
              a_smoothBits.erase(it->first);
          }
          else
          {
              s_sum += 1 / it->second;
              s_count++;
          }
      }
      if (s_count < 20)
          m_smoothDataRate = m_targetDataRate;
      else
          m_smoothDataRate = s_count / s_sum;
*/
      double sum = 0.0;
      for (std::map<uint32_t, uint32_t>::iterator it = m_bitSwitch.begin();
        it != m_bitSwitch.end(); it++)
        {
            if (m_switchCount < 10)
            {
                sum += it->second*((it->first-m_switchCount+11)/10.0);
            }
            else
            {
                if (it->first < m_switchCount-10)
                    m_bitSwitch.erase(it->first);
                else
                {
                    sum += it->second*((it->first-m_switchCount+11)/10.0);
                }
            }
        }

      O_up = e*m_smoothDataRate;
      uint32_t rate_up = GetBitRate(m_smoothDataRate - O_up);
      uint32_t rate_down = GetBitRate(m_smoothDataRate - O_down);
      std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/aaaClient-- rate_up~: " << m_smoothDataRate - O_up
        << " rate_down~: " << m_smoothDataRate - O_down << std::endl;
      if (currRate < rate_up)
      {
          nextRate = rate_up;
          std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/aaaClient-- 1 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << " realBit: " << realBitRate << std::endl;
      }
      else if (currRate >= rate_up && currRate <= rate_down)
      {
          nextRate = currRate;
          std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/aaaClient-- 2 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << " realBit: " << realBitRate << std::endl;
      }
      else
      {
          nextRate = rate_down;
          std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/aaaClient-- 3 / rate_up: " << rate_up
                    << " rate_down: " << rate_down << " realBit: " << realBitRate << std::endl;
      }

      //if (m_currDt <= m_minDt)
      if (false)
      {
          nextRate = GetBitRate(realBitRate);
          std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id <<  "/AaashClient / ====  realBitRate=" << realBitRate
                << std::endl;
      }
      else
      {
          if (nextRate > currRate)
          {
              inCount++;
              deCount = 0;
              if (inCount >= 2*sum)
              {
                  inCount = 0;
              }
              else
                nextRate = currRate;
              std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id <<  "/AaashClient/ in++de=0  inCount=" << inCount
                << " deCount=" << deCount << " 2sum=" << 2*sum << " realBit: " << realBitRate << " fetchTime: "
                << m_segmentFetchTime.GetSeconds() << " FT: " << m_FT <<std::endl;
          }
          else if (nextRate < currRate)
          {
              //if ((currRate-nextRate)/nextRate >= 2)
              if (false)
              {
                  deCount = 0;
                  inCount = 0;
                  std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id <<  "/AaashClient/ jump#  inCount=" << inCount
                    << " deCount=" << deCount << " 2sum=" << 2*sum << " realBit: " << realBitRate <<  std::endl;
              }
              else
              {
                  deCount++;
                  inCount = 0;
                  if (deCount >= 2*sum)
                  {
                    deCount = 0;
                  }
                  else
                    nextRate = currRate;
                  std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id <<  "/AaashClient/ in=0de++  inCount=" << inCount
                    << " deCount=" << deCount << " 2sum=" << 2*sum << " realBit: " << realBitRate <<  std::endl;
              }
          }
          else
          {
              deCount++;
              inCount++;
              std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id <<  "/AaashClient/ in++de++  inCount=" << inCount
                << " deCount=" << deCount << " 2sum=" << 2*sum << " realBit: " << realBitRate <<  std::endl;
          }
      }

      if (m_currDt > m_maxDt)
            m_idleTime = Seconds(b*(m_currDt.GetSeconds()-m_maxDt.GetSeconds()));
      else
            m_idleTime = Seconds(0);

      if (std::abs(Geti(nextRate)-Geti(currRate)) >= 9)
          a_m = 5;

      // simple fast start logic. Copy form panda.
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

      std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << "/AaashClient/ m: " << sum
        << " preTarget: " << preTargetDataRate << " nowtarget: " << m_targetDataRate << " smooth: " << m_smoothDataRate
        << " aa1: " << aa1 << " aa2: " << aa2 << " aveRate: " << aveRate << std::endl;
  }

  uint32_t
  AaashClient::GetBitRate(double throughtPut)
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

  uint32_t
  AaashClient::Geti(double bitrate)
  {
      uint32_t rates[] =
        { 100000, 200000, 300000, 400000, 500000, 600000, 700000, 900000, 1000000, 1200000,
          1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 4500000, 5000000, 5500000, 6000000 };
      int rates_size = sizeof(rates) / sizeof(rates[0]);

      for (int i=0; i<rates_size; i++)
      {
          if (bitrate == rates[i])
            return i;
      }
      return 0;
  }

} /* namespace ns3 */
