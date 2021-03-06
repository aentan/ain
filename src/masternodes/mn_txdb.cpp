// Copyright (c) 2019 DeFi Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <masternodes/mn_txdb.h>

#include <chainparams.h>
#include <uint256.h>

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

// Prefixes to the masternodes database (masternodes/)
static const char DB_MASTERNODES = 'M';     // main masternodes table
static const char DB_MASTERNODESUNDO = 'U'; // undo table
//static const char DB_TEAM = 'T';
//static const char DB_PRUNEDEAD = 'D';
static const char DB_MN_HEIGHT = 'H';       // single record with last processed chain height
static const char DB_PRUNE_HEIGHT = 'P';    // single record with pruned height (for validation of reachable data window)


CMasternodesViewDB::CMasternodesViewDB(size_t nCacheSize, bool fMemory, bool fWipe)
    : db(new CDBWrapper(GetDataDir() / "masternodes", nCacheSize, fMemory, fWipe))
{
}

// for test purposes only
CMasternodesViewDB::CMasternodesViewDB()
    : db(nullptr)
{
}

void CMasternodesViewDB::CommitBatch()
{
    if (batch)
    {
        db->WriteBatch(*batch);
        batch.reset();
    }
}

//void CMasternodesViewDB::DropBatch()
//{
//    if (batch)
//    {
//        batch.reset();
//    }
//}

bool CMasternodesViewDB::ReadHeight(int & h)
{
    // it's a hack, cause we don't know active chain tip at the loading time
    if (!db->Read(DB_MN_HEIGHT, h))
    {
        h = 0;
    }
    return true;
}

void CMasternodesViewDB::WriteHeight(int h)
{
    BatchWrite(DB_MN_HEIGHT, h);
}

void CMasternodesViewDB::WriteMasternode(uint256 const & txid, CMasternode const & node)
{
    BatchWrite(make_pair(DB_MASTERNODES, txid), node);
}

void CMasternodesViewDB::EraseMasternode(uint256 const & txid)
{
    BatchErase(make_pair(DB_MASTERNODES, txid));
}

//void CMasternodesViewDB::WriteDeadIndex(int height, uint256 const & txid, char type)
//{
//    BatchWrite(make_pair(make_pair(DB_PRUNEDEAD, static_cast<int32_t>(height)), txid), type);
//}

//void CMasternodesViewDB::EraseDeadIndex(int height, uint256 const & txid)
//{
//    BatchErase(make_pair(make_pair(DB_PRUNEDEAD, static_cast<int32_t>(height)), txid));
//}

void CMasternodesViewDB::WriteUndo(int height, CMnTxsUndo const & undo)
{
    BatchWrite(make_pair(DB_MASTERNODESUNDO, static_cast<int32_t>(height)), undo);
}

void CMasternodesViewDB::EraseUndo(int height)
{
    BatchErase(make_pair(DB_MASTERNODESUNDO, static_cast<int32_t>(height)));
}

//void CMasternodesViewDB::WriteTeam(int blockHeight, const CTeam & team)
//{
//    // we are sure that we have no spoiled records (all of them are deleted)
//    for (CTeam::const_iterator it = team.begin(); it != team.end(); ++it)
//    {
//        BatchWrite(make_pair(make_pair(DB_TEAM, static_cast<int32_t>(blockHeight)), it->first), make_pair(it->second.joinHeight, it->second.operatorAuth));
//    }
//}

/*
 * Loads all data from DB, creates indexes
 */
bool CMasternodesViewDB::Load()
{
    Clear();

    bool result = true;
    result = result && ReadHeight(lastHeight);

    result = result && LoadTable(DB_MASTERNODES, allNodes, [this] (uint256 nodeId, CMasternode & node) {
        nodesByOwner.insert(std::make_pair(node.ownerAuthAddress, nodeId));
        nodesByOperator.insert(std::make_pair(node.operatorAuthAddress, nodeId));
    });
    result = result && LoadTable(DB_MASTERNODESUNDO, blocksUndo);

    // Load teams information
//    result = result && LoadTable(DB_TEAM, teams);

    if (result)
//        LogPrintf("MN: db loaded: last height: %d; masternodes: %d; common undo: %d; teams: %d\n", lastHeight, allNodes.size(), txsUndo.size(), teams.size());
        LogPrintf("MN: db loaded: last height: %d; masternodes: %d; common undo: %d\n", lastHeight, allNodes.size(), blocksUndo.size());
    else {
        LogPrintf("MN: fail to load database!\n");
    }
    return result;
}

bool CMasternodesViewDB::Flush()
{
    batch.reset();

    /// @todo @max optimize with new diff model of view
    int nMasternodes{0};
    for (auto && it = allNodes.begin(); it != allNodes.end(); ) {
        if (it->second == CMasternode()) {
            EraseMasternode(it->first);
            it = allNodes.erase(it);
        }
        else {
            WriteMasternode(it->first, it->second);
            ++nMasternodes;
            ++it;
        }
    }

    int nUndo{0};
    for (auto && it = blocksUndo.begin(); it != blocksUndo.end(); )
    {
        if (it->second.size() == 0) {
            EraseUndo(it->first);
            it = blocksUndo.erase(it);
        }
        else {
            WriteUndo(it->first, it->second);
            ++nUndo;
            ++it;
        }
    }

//    for (auto && it = teams.begin(); it != teams.end(); ++it)
//    {
//        WriteTeam(it->first, it->second);
//    }

    WriteHeight(lastHeight);

    CommitBatch();
//    LogPrintf("MN: db saved: last height: %d; masternodes: %d; common undo: %d; teams: %d\n", lastHeight, nMasternodes, nUndo, teams.size());
    LogPrintf("MN: db saved: last height: %d; masternodes: %d; common undo: %d\n", lastHeight, nMasternodes, nUndo);

    return true;
}
