/*
 * aaash-client.h
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#ifndef AAASH_CLIENT_H_
#define AAASH_CLIENT_H_

#include <ns3/dash-client.h>
namespace ns3
{

  class AaashClient : public DashClient
  {
  public:
    static TypeId
    GetTypeId(void);

    AaashClient();

    virtual
    ~AaashClient();

    virtual void
    CalcNextSegment(uint32_t currRate, uint32_t & nextRate, double aveRate);

  private:
    uint32_t
    GetBitRate(double throughtPut);
    uint32_t
    Geti(double bitrate);

    bool fast_start;
    uint32_t a_m;
    std::map<Time, double> a_smoothBits;
  };

} /* namespace ns3 */

#endif /* AAASH_CLIENT_H_ */
