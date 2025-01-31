/*-------------------------------------------------------------------------
 * This source code is confidential proprietary information which is
 * copyright (c) 2016, 2021 TrueBlocks, LLC (http://trueblocks.io)
 * All Rights Reserved
 *------------------------------------------------------------------------*/
#include "options.h"
#include "consolidator.h"

//--------------------------------------------------------------------------
CConsolidator::CConsolidator(void) : pin(false), distFromHead(0), prevBlock(0) {
    blazeStart = 0;
    blazeRipe = 0;
    blazeCnt = 0;

    CBlockProgress prog = getBlockProgress();
    unripe = prog.unripe;
    ripe = prog.ripe;
    staging = prog.staging;
    finalized = prog.finalized;
    client = prog.client;

    prevBlock = (prog.staging == NOPOS ? (prog.finalized == NOPOS ? 0 : prog.finalized) : prog.staging);

    tmp_fn = indexFolder_staging + "000000000-temp.txt";
    tmp_stream.open(tmp_fn, ios::out | ios::trunc);
}

//--------------------------------------------------------------------------
void CConsolidator::Format(ostream& os) const {
    os << "oldStage:     " << oldStage << endl;
    os << "newStage:     " << newStage << endl;
    os << "tmpFile:      " << tmpFile << endl;
    os << "tmp_fn:       " << tmp_fn << endl;
    os << "blazeStart:   " << blazeStart << endl;
    os << "blazeRipe:    " << blazeRipe << endl;
    os << "blazeCnt:     " << blazeCnt << endl;
    os << "distFromHead: " << distFromHead << endl;
    os << "prevBlock:    " << prevBlock << endl;
    os << "client:       " << client << endl;
    os << "finalized:    " << finalized << endl;
    os << "staging:      " << staging << endl;
    os << "ripe:         " << ripe << endl;
    os << "unripe:       " << unripe << endl;
    os << "pin:          " << (pin ? "true" : "false") << endl;
    uint32_t x = 0;
    for (auto pi : pinList)
        os << "pinList[" << padNum3(x++) << "]: " << pi << endl;
}

//--------------------------------------------------------------------------
bool appendFile(const string_q& toFile, const string_q& fromFile) {
    ofstream output;
    output.open(toFile, ios::out | ios::app);
    if (!output.is_open())
        return false;

    ifstream input(fromFile, ios::in);
    if (!input.is_open()) {
        output.close();
        return false;
    }

    output << input.rdbuf();
    output.flush();
    input.close();

    return true;
}

//--------------------------------------------------------------------------
bool copyRipeToStage(const string_q& path, void* data) {
    if (endsWith(path, '/')) {
        return forEveryFileInFolder(path + "*", copyRipeToStage, data);

    } else {
        blknum_t e_unused;
        timestamp_t ts;
        blknum_t bn = bnFromPath(path, e_unused, ts);

        // If we're not one behind, we have a problem
        CConsolidator* con = reinterpret_cast<CConsolidator*>(data);
        if ((con->prevBlock + 1) != bn) {
            // For some reason, we're missing a file. Quit and try again next time
            LOG_WARN("Current file (", path, ") does not sequentially follow previous file ", con->prevBlock, ".");
            return false;
        }

        ifstream inputStream(path, ios::in);
        if (!inputStream.is_open()) {
            // Something went wrong, try again next time
            LOG_WARN("Could not open input stream ", path);
            return false;
        }

        lockSection();
        con->tmp_stream << inputStream.rdbuf();
        con->tmp_stream.flush();
        inputStream.close();
        ::remove(path.c_str());
        con->prevBlock = bn;
        unlockSection();

        if (bn > FIRST_SNAP_TO_GRID && !(bn % SNAP_TO_GRID_BLKS)) {
            LOG4("  Snapping to block number ", bn);

            // We've been copying each block's data into a temporary file. At this point, we've
            // encountered a block that is a snap-to block (for example, every 100,000 blocks).
            // We write a short chunk here. We do this in order to make correcting errors in the
            // index easier. As we do during normal processing, we clean up of the chunking does
            // not complete properly. 'stage_chunks' creates the newStage.

            // First, we complete the staging at the current progress. Cleanup and quit if something
            // goes wrong. Quit the scan and start over later...
            if (!con->stage_chunks()) {
                cleanFolder(indexFolder_unripe);
                cleanFolder(indexFolder_ripe);
                ::remove(con->tmp_fn.c_str());
                return false;
            }

            // We continually want to check to see if the user has hit control+c so we stay responsive.
            if (shouldQuit())
                return true;

            // This may be less than a single chunk or larger. We want to do at most MAX_ROWS records
            // but we want to do a chunk no matter what (since we've snapped to grid).

            // How many records are there?
            blknum_t nRecords = fileSize(con->newStage) / 59;

            // How big are we going to make this chunk?
            blknum_t chunkSize = min(nRecords, MAX_ROWS);

            // Now we try to write the chunk. We will write at least one chunk no matter how many records there are
            con->write_chunks(chunkSize, true /* atLeastOnce */);

            // We return false here to pretend this scrape did not complete. We do this because we've actually already
            // written at least one chunk and we know we are at a snap-to boundary. We want the processor to clean up
            // and start fresh next time at the grid location plus one. It will do this naturally if we pretend to
            // have failed processing by returning false;
            return false;
        }
    }

    // Return true if the user has not hit control+c
    return !shouldQuit();
}
