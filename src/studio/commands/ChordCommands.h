#pragma once

#include "CommandManager.h"
#include <string>
#include <vector>

namespace bbfx {

struct ChordBlock {
    std::string name;
    float startBeat;
    float endBeat;
    float hue;
};

class AddChordCommand : public Command {
public:
    AddChordCommand(std::vector<ChordBlock>& blocks, const ChordBlock& chord);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::vector<ChordBlock>& mBlocks;
    ChordBlock mChord;
    size_t mIndex = 0;
};

class DeleteChordCommand : public Command {
public:
    DeleteChordCommand(std::vector<ChordBlock>& blocks, size_t index);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::vector<ChordBlock>& mBlocks;
    ChordBlock mSaved;
    size_t mIndex;
};

class RenameChordCommand : public Command {
public:
    RenameChordCommand(std::vector<ChordBlock>& blocks, size_t index,
                       const std::string& newName);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::vector<ChordBlock>& mBlocks;
    size_t mIndex;
    std::string mOldName, mNewName;
};

class ResizeChordCommand : public Command {
public:
    ResizeChordCommand(std::vector<ChordBlock>& blocks, size_t index,
                       float oldStart, float oldEnd,
                       float newStart, float newEnd);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::vector<ChordBlock>& mBlocks;
    size_t mIndex;
    float mOldStart, mOldEnd, mNewStart, mNewEnd;
};

} // namespace bbfx
