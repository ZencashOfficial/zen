// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include <boost/foreach.hpp>

// uncomment for debugging mkl root hash calculations
//#define DEBUG_MKLTREE_HASH 1

// uncomment for debugging mkl branch check
//#define DEBUG_MERKLE_BRANCH 1

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlock::BuildMerkleTree(bool* fMutated) const
{
    /* WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
    */
    vMerkleTree.clear();
#if 0
    vMerkleTree.reserve(vtx.size() * 2 + 16); // Safe upper bound for the number of total nodes.
    for (std::vector<CTransaction>::const_iterator it(vtx.begin()); it != vtx.end(); ++it)
        vMerkleTree.push_back(it->GetHash());

    return BuildMerkleTree(vMerkleTree, vtx.size(), fMutated);
#else
    std::vector<const CTransactionBase*> vTxBase;
    GetTxAndCertsVector(vTxBase);

    vMerkleTree.reserve(vTxBase.size() * 2 + 16); // Safe upper bound for the number of total nodes.
    for (auto it(vTxBase.begin()); it != vTxBase.end(); ++it)
        vMerkleTree.push_back((*it)->GetHash());

    return BuildMerkleTree(vMerkleTree, vTxBase.size(), fMutated);
#endif
}

uint256 CBlock::BuildMerkleTree(std::vector<uint256>& vMerkleTreeIn, size_t vtxSize, bool* fMutated)
{
    int j = 0;
    bool mutated = false;
    for (int nSize = vtxSize; nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTreeIn[j+i] == vMerkleTreeIn[j+i2]) {
                // Two identical hashes at the end of the list at a particular level.
                mutated = true;
            }
            vMerkleTreeIn.push_back(Hash(BEGIN(vMerkleTreeIn[j+i]),  END(vMerkleTreeIn[j+i]),
                                         BEGIN(vMerkleTreeIn[j+i2]), END(vMerkleTreeIn[j+i2])));
#ifdef DEBUG_MKLTREE_HASH
            std::cout << " -------------------------------------------" << std::endl;
            std::cout << i << ") mkl hash: " << vMerkleTreeIn.back().ToString() << std::endl;
            std::cout <<      "      hash1: " << vMerkleTreeIn[j+i].ToString() << std::endl;
            std::cout <<      "      hash2: " << vMerkleTreeIn[j+i2].ToString() << std::endl;
#endif
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return (vMerkleTreeIn.empty() ? uint256() : vMerkleTreeIn.back());
}

uint256 SidechainTxsCommitmentBuilder::getMerkleRootHash(const std::vector<uint256>& vInput) 
{
    std::vector<uint256> vTempMerkleTree = vInput;
    return CBlock::BuildMerkleTree(vTempMerkleTree, vInput.size());
}

uint256 CBlock::BuildScTxsCommitment()
{
    SidechainTxsCommitmentBuilder scCommitmentBuilder;

    for (const auto& tx : vtx)
    {
        scCommitmentBuilder.add(tx);
    }

    for (const auto& cert : vcert)
    {
        scCommitmentBuilder.add(cert);
    }

    return scCommitmentBuilder.getCommitment();
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    std::vector<uint256> vMerkleBranch;
    int j = 0;
#if 0
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
#else
    for (int nSize = (vtx.size() + vcert.size()); nSize > 1; nSize = (nSize + 1) / 2)
#endif
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return uint256();
    for (std::vector<uint256>::const_iterator it(vMerkleBranch.begin()); it != vMerkleBranch.end(); ++it)
    {
#ifdef DEBUG_MERKLE_BRANCH
        std::cout << " -------------------------------------------" << std::endl;
        std::cout << "  idx: " << nIndex << std::endl;
        std::cout << "     (b)hash:  " << (*it).ToString() << std::endl;
        std::cout << "        hash:  " << hash.ToString() << std::endl;
#endif
        if (nIndex & 1)
            hash = Hash(BEGIN(*it), END(*it), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(*it), END(*it));
        nIndex >>= 1;
#ifdef DEBUG_MERKLE_BRANCH
        std::cout << "  ret hash: " << hash.ToString() << std::endl;
#endif

    }
    return hash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, hashScTxsCommitment=%s, nTime=%u, nBits=%08x, nNonce=%s, vtx=%u, vcert=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hashScTxsCommitment.ToString(),
        nTime, nBits, nNonce.ToString(),
        vtx.size(),
        vcert.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    for (unsigned int i = 0; i < vcert.size(); i++)
    {
        s << "  " << vcert[i].ToString() << "\n";
    }
    s << "  vMerkleTree: ";
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        s << " " << vMerkleTree[i].ToString();
    s << "\n";
    return s.str();
}

void CBlock::GetTxAndCertsVector(std::vector<const CTransactionBase*>& vBase) const
{
    vBase.clear();
    vBase.reserve(vtx.size() + vcert.size()); 

    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        vBase.push_back(&(vtx[i]));
    }
    for (unsigned int i = 0; i < vcert.size(); i++)
    {
        vBase.push_back(&(vcert[i]));
    }
}

const std::string SidechainTxsCommitmentBuilder::MAGIC_SC_STRING = "Horizen ScTxsCommitment null hash string";

const uint256& SidechainTxsCommitmentBuilder::getCrossChainNullHash()
{
    static bool generated = false;
    static uint256 theHash;

    if (!generated)
    {
        CHashWriter ss(SER_GETHASH, 0);
        ss << MAGIC_SC_STRING;
        theHash = ss.GetHash();
        LogPrintf("%s():%d - Generated sc null hash [%s]\n", __func__, __LINE__, theHash.ToString());
        generated = true;
    }
    return theHash;
}

void SidechainTxsCommitmentBuilder::add(const CTransaction& tx)
{
    tx.addToScCommitment(mScMerkleTreeLeavesFt, sScIds);
    // TODO btr to be implemented
}

void SidechainTxsCommitmentBuilder::add(const CScCertificate& cert)
{
    cert.addToScCommitment(mScCerts, sScIds);
}

uint256 SidechainTxsCommitmentBuilder::getCommitment()
{
    std::vector<uint256> vSortedScLeaves;

    // set of scid is ordered
    for (const auto& scid : sScIds)
    {
        uint256 FtHash(getCrossChainNullHash());
        uint256 BtrHash(getCrossChainNullHash());
        uint256 WCertHash(getCrossChainNullHash());

        auto itFt = mScMerkleTreeLeavesFt.find(scid);
        if (itFt != mScMerkleTreeLeavesFt.end() )
        {
            FtHash = getMerkleRootHash(itFt->second);
        }

        auto itBtr = mScMerkleTreeLeavesBtr.find(scid);
        if (itBtr != mScMerkleTreeLeavesBtr.end() )
        {
            BtrHash = getMerkleRootHash(itBtr->second);
        }

        auto itCert = mScCerts.find(scid);
        if (itCert != mScCerts.end() )
        {
            WCertHash = itCert->second;
        }

        const uint256& TxsHash = Hash(
            BEGIN(FtHash),    END(FtHash),
            BEGIN(BtrHash),   END(BtrHash) );

        const uint256& ScHash = Hash(
            BEGIN(TxsHash),   END(TxsHash),
            BEGIN(WCertHash), END(WCertHash),
            BEGIN(scid),      END(scid) );

#ifdef DEBUG_SC_COMMITMENT_HASH
        std::cout << " -------------------------------------------" << std::endl;
        std::cout << "  FtHash:  " << FtHash.ToString() << std::endl;
        std::cout << "  BtrHash: " << BtrHash.ToString() << std::endl;
        std::cout << "  => TxsHash:   " << TxsHash.ToString() << std::endl;
        std::cout << "     WCertHash: " << WCertHash.ToString() << std::endl;
        std::cout << "     scid:      " << scid.ToString() << std::endl;
        std::cout << "     => ScsHash:  " << ScHash.ToString() << std::endl;
#endif
        vSortedScLeaves.push_back(ScHash);
    }

    return getMerkleRootHash(vSortedScLeaves);
}

