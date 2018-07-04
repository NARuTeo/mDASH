/*
 * panda-client.h
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#ifndef PANDA_CLIENT_H_
#define PANDA_CLIENT_H_

#include <ns3/dash-client.h>
namespace ns3
{

  class PandaClient : public DashClient
  {
  public:
    static TypeId
    GetTypeId(void);

    PandaClient();

    virtual
    ~PandaClient();

    virtual void
    CalcNextSegment(uint32_t currRate, uint32_t & nextRate, double aveRate);

  private:
    Time p_minDt;
    uint32_t
    GetBitRate(double throughtPut);
    bool fast_start;

  };

} /* namespace ns3 */

#endif /* PANDA_CLIENT_H_ */
