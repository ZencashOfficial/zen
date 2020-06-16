#ifndef ORIGINALFORK_H
#define ORIGINALFORK_H

#include "fork.h"
#include "chain.h"

namespace sic {

/**
 * @brief The OriginalFork class represents the original fork at block 0
 * This is the ZClassic original fork before the chainsplit
 */
class OriginalFork : public Fork
{
public:
    
    /**
     * @brief OriginalFork constructor
     */
    OriginalFork();

    /**
     * @brief getCommunityFundAddress returns the community fund address based on the passed in height and maxHeight
     */
    virtual const std::string& getCommunityFundAddress(CBaseChainParams::Network network, int height, int maxHeight, CommunityFundType cfType) const;

    /**
     * @brief getCommunityFundReward returns the community fund reward based on the height and passed-in reward
     */
    virtual CAmount getCommunityFundReward(const CAmount& amount, CommunityFundType cfType) const;

    /**
     * @brief canSendCommunityFundsToTransparentAddress true if community funds can be sent to a transparent address
     */
    inline virtual bool canSendCommunityFundsToTransparentAddress() const { return false; }

    /**
     * @brief getReplayProtectionLevel returns the replay protection level provided by the current fork
     */
    inline virtual ReplayProtectionLevel getReplayProtectionLevel() const { return RPLEVEL_NONE; }

    /**
     * @brief isAfterChainsplit returns true if this height is after the original chain split, false otherwise
     */
    inline virtual bool isAfterChainsplit() const { return false; }

    /**
     * @brief isTransactionTypeAllowed returns true if this transaction type is allowed in this fork, false otherwise
     */
    virtual bool isTransactionTypeAllowed(txnouttype transactionType) const;

    /**
	 * @brief returns phpgr,groth,... tx version based on block height
	 */
    inline virtual int getShieldedTxVersion() const { return 2; }

    /**
	 * @brief returns true if the miner has to use MAX_FUTURE_BLOCK_TIME_MTP
	 */
    inline virtual bool isFutureMiningTimeStampActive() const { return false; }

    /**
     * @brief returns true if the contextualcheckblockheader uses the MAX_FUTURE_BLOCK_TIME_MTP check blocktime,
     */
    inline virtual bool isFutureTimeStampActive(int height, CBaseChainParams::Network network) const { return false; }

};

}
#endif // ORIGINALFORK_H
