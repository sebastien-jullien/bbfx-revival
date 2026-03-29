#include "ChordCommands.h"

namespace bbfx {

// ── AddChordCommand ──────────────────────────────────────────────────────────

AddChordCommand::AddChordCommand(std::vector<ChordBlock>& blocks,
                                 const ChordBlock& chord)
    : mBlocks(blocks), mChord(chord) {}

void AddChordCommand::execute() {
    mIndex = mBlocks.size();
    mBlocks.push_back(mChord);
}

void AddChordCommand::undo() {
    if (mIndex < mBlocks.size()) {
        mBlocks.erase(mBlocks.begin() + static_cast<ptrdiff_t>(mIndex));
    }
}

std::string AddChordCommand::description() const {
    return "Add chord '" + mChord.name + "'";
}

// ── DeleteChordCommand ───────────────────────────────────────────────────────

DeleteChordCommand::DeleteChordCommand(std::vector<ChordBlock>& blocks, size_t index)
    : mBlocks(blocks), mIndex(index) {}

void DeleteChordCommand::execute() {
    if (mIndex < mBlocks.size()) {
        mSaved = mBlocks[mIndex];
        mBlocks.erase(mBlocks.begin() + static_cast<ptrdiff_t>(mIndex));
    }
}

void DeleteChordCommand::undo() {
    if (mIndex <= mBlocks.size()) {
        mBlocks.insert(mBlocks.begin() + static_cast<ptrdiff_t>(mIndex), mSaved);
    }
}

std::string DeleteChordCommand::description() const {
    return "Delete chord '" + mSaved.name + "'";
}

// ── RenameChordCommand ───────────────────────────────────────────────────────

RenameChordCommand::RenameChordCommand(std::vector<ChordBlock>& blocks,
                                       size_t index,
                                       const std::string& newName)
    : mBlocks(blocks), mIndex(index), mNewName(newName) {}

void RenameChordCommand::execute() {
    if (mIndex < mBlocks.size()) {
        mOldName = mBlocks[mIndex].name;
        mBlocks[mIndex].name = mNewName;
    }
}

void RenameChordCommand::undo() {
    if (mIndex < mBlocks.size()) {
        mBlocks[mIndex].name = mOldName;
    }
}

std::string RenameChordCommand::description() const {
    return "Rename chord '" + mOldName + "' → '" + mNewName + "'";
}

// ── ResizeChordCommand ───────────────────────────────────────────────────────

ResizeChordCommand::ResizeChordCommand(std::vector<ChordBlock>& blocks,
                                       size_t index,
                                       float oldStart, float oldEnd,
                                       float newStart, float newEnd)
    : mBlocks(blocks), mIndex(index),
      mOldStart(oldStart), mOldEnd(oldEnd),
      mNewStart(newStart), mNewEnd(newEnd) {}

void ResizeChordCommand::execute() {
    if (mIndex < mBlocks.size()) {
        mBlocks[mIndex].startBeat = mNewStart;
        mBlocks[mIndex].endBeat = mNewEnd;
    }
}

void ResizeChordCommand::undo() {
    if (mIndex < mBlocks.size()) {
        mBlocks[mIndex].startBeat = mOldStart;
        mBlocks[mIndex].endBeat = mOldEnd;
    }
}

std::string ResizeChordCommand::description() const {
    return "Resize chord";
}

} // namespace bbfx
