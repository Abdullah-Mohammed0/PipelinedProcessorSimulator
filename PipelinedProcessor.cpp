#include <iostream>
#include <fstream>

#define ICACHE_FILE   "input/ICache.txt"
#define DCACHE_FILE   "input/DCache.txt"
#define REGISTER_FILE "input/RF.txt"
#define ODCACHE_FILE  "output/ODCache.txt"
#define STATS_FILE    "output/Output.txt"

const int NUM_REGISTERS = 16;
const int NUM_SETS = 64;
const int BLOCK_SIZE = 4;

int totalInstructions, arithmeticInstructions, logicalInstructions, dataInstructions,
    controlInstructions, haltInstructions, cycles, stalls, dataStalls;
struct set
{
    int offset[BLOCK_SIZE];
};

class Cache
{
protected:
    set block[NUM_SETS];

public:
    virtual int read(int address) = 0;
    virtual void write(int address, int data) = 0;
};

class InstructionCache : public Cache
{
public:
    int read(int address) { return (block[address >> 2].offset[address & 3] << 8) + block[address >> 2].offset[(address & 3) + 1]; }
    void write(int address, int data)
    {
        block[address >> 2].offset[address & 3] = data >> 8;
        block[address >> 2].offset[(address & 3) + 1] = data & 0xFF;
    }
};

class DataCache : public Cache
{
public:
    int read(int address) { return block[address >> 2].offset[address & 3]; }
    void write(int address, int data) { block[address >> 2].offset[address & 3] = data; }
};

class ProgramCounter
{
private:
    int val;

public:
    void increment() { val += 2; }
    void decrement() { val -= 2; }
    int read() { return val; }
    void write(int data) { val = data; }
};

class Register
{
private:
    int content;
    bool valid, dataHazard;

public:
    Register() : valid(true), dataHazard(false) {}

    int getContent() { return content; }
    void setContent(int newContent) { content = newContent; }
    bool getValid() { return valid; }
    void setValid(bool newValid) { valid = newValid; }
    bool getDataHazard() { return dataHazard; }
    void setDataHazard(bool newDataHazard) { dataHazard = newDataHazard; }

    friend class RegisterFile;
};

class InstructionRegister
{
private:
    int content;

public:
    int getContent() { return content; }
    void setContent(int newContent) { content = newContent; }
};

class RegisterFile
{
private:
    Register R[NUM_REGISTERS];

public:
    RegisterFile() {} 

    int readContent(int index) { return R[index].getContent(); }
    void writeContent(int index, int newContent) { R[index].setContent(newContent); }
    bool checkValid(int index) { return R[index].valid; }
    void setValid(int index, bool newValid) { R[index].valid = newValid; }
    bool checkDataHazard(int index) { return R[index].dataHazard; }
    void setDataHazard(int index, bool newDataHazard) { R[index].dataHazard = newDataHazard; }
};

class ArithmeticLogicalUnit
{
public:
    int ADD(int a, int b) { return a + b; }
    int SUB(int a, int b) { return a - b; }
    int MUL(int a, int b) { return a * b; }
    int INC(int a) { return a + 1; }
    int AND(int a, int b) { return a & b; }
    int OR(int a, int b) { return a | b; }
    int NOT(int a) { return ~a; }
    int XOR(int a, int b) { return a ^ b; }
    bool BEQZ(int a) { return a == 0; }
};

ProgramCounter PC;
ArithmeticLogicalUnit ALU;
InstructionRegister IR;
InstructionCache iCache;
DataCache dCache;
RegisterFile RF;

class FetchDecodeBuffer
{
private:
    bool valid;
    int instruction;

public:
    FetchDecodeBuffer() : valid(false) {}
    int getInstruction() { return instruction; }
    void setInstruction(int newInstruction) { instruction = newInstruction; }
    bool checkValid() { return valid; }
    void setValid(bool newValid) { valid = newValid; }
};

enum
{
    ARITHMETIC = 0,
    LOGICAL = 4,
    LOAD = 8,
    STORE = 9,
    JMP = 10,
    BEQZ = 11,
    HALT = 15
};

class DecodeExecuteBuffer
{
private:
    bool valid;
    int instructionType;
    int opcode, src1, src2, dest, offset;

public:
    DecodeExecuteBuffer() : valid(false) {}

    int getInstructionType() { return instructionType; }
    void setInstructionType(int newInstructionType) { instructionType = newInstructionType; }
    int getOpcode() { return opcode; }
    void setOpcode(int newOpcode) { opcode = newOpcode; }
    int getSrc1() { return src1; }
    void setSrc1(int newSrc1) { src1 = newSrc1; }
    int getSrc2() { return src2; }
    void setSrc2(int newSrc2) { src2 = newSrc2; }
    int getDest() { return dest; }
    void setDest(int newDest) { dest = newDest; }
    bool checkValid() { return valid; }
    void setValid(bool newValid) { valid = newValid; }
    int getOffset() { return offset; }
    void setOffset(int newOffset) { offset = newOffset; }
};

class ExecuteMemoryBuffer
{
private:
    bool valid;
    int instructionType, dest, src;
    Register ALUOutput;

public:
    ExecuteMemoryBuffer() : valid(false) {}

    int getInstructionType() { return instructionType; }
    void setInstructionType(int newInstructionType) { instructionType = newInstructionType; }
    int getDest() { return dest; }
    void setDest(int newDest) { dest = newDest; }
    int getSrc() { return src; }
    void setSrc(int newSrc) { src = newSrc; }
    bool checkValid() { return valid; }
    void setValid(bool newValid) { valid = newValid; }
    int getALUOutput() { return ALUOutput.getContent(); }
    void setALUOutput(int newOutput) { ALUOutput.setContent(newOutput); }
};

class MemoryWriteBackBuffer
{
private:
    bool valid;
    int instructionType, dest;
    Register ALUOutput;

public:
    MemoryWriteBackBuffer() : valid(false) {}

    int getInstructionType() { return instructionType; }
    void setInstructionType(int newInstructionType) { instructionType = newInstructionType; }
    int getDest() { return dest; }
    void setDest(int newDest) { dest = newDest; }
    bool checkValid() { return valid; }
    void setValid(bool newValid) { valid = newValid; }
    int getALUOutput() { return ALUOutput.getContent(); }
    void setALUOutput(int newOutput) { ALUOutput.setContent(newOutput); }
};

int currHazardousRegisters, prevHazardousRegisters;
bool stopFetch, branchUndecided, prevBranchUndecided;

class FetchStage
{
public:
    FetchDecodeBuffer &bufRight;
    bool stall;

    FetchStage(FetchDecodeBuffer &FDBuf) : stall(false), bufRight(FDBuf) { prevHazardousRegisters = 0; }

    void execute()
    {
        stall = stopFetch;
        stall = stall || branchUndecided;
        stall = stall || (currHazardousRegisters > 0);

        bufRight.setValid(!stall);

        if (!stall)
        {

            bufRight.setInstruction(iCache.read(PC.read()));
            IR.setContent(bufRight.getInstruction());
            PC.increment();
            stall = false;
        }
        else
        {
            prevHazardousRegisters = currHazardousRegisters;
        }
    }
};

class DecodeStage
{
public:
    FetchDecodeBuffer &bufLeft;
    DecodeExecuteBuffer &bufRight;
    bool stall;

    DecodeStage(FetchDecodeBuffer &FDBuf, DecodeExecuteBuffer &DEBuf) : stall(false), bufLeft(FDBuf), bufRight(DEBuf) {}

    void execute() 
    {
        stall = ((currHazardousRegisters > 0) || branchUndecided || stopFetch || !bufLeft.checkValid());

        bufRight.setValid(!stall);

        if (stall)
            return;

        int instruction = bufLeft.getInstruction(), opcode = instruction >> 12;
        bufRight.setOpcode(opcode);
        bufRight.setInstructionType(opcode); 

        switch (opcode)
        {
        case HALT:
        {
            stopFetch = true;
            break;
        }

        case BEQZ:
        {
            int R1 = (instruction >> 8) & 0xf;
            stall = !RF.checkValid(R1);
            bufRight.setValid(!stall);

            if (!stall)
            {
                branchUndecided = true;
                bufRight.setSrc1(RF.readContent(R1));
                bufRight.setOffset(instruction & 0xff);
            }
            else
            {
                RF.setDataHazard(R1, true);
                ++currHazardousRegisters;
            }
            break;
        }

        case JMP:
        {
            bufRight.setOffset((instruction >> 4) & 0xff);
            branchUndecided = true;
            break;
        }

        case STORE:
        {
            int R1 = (instruction >> 8) & 0xf, R2 = (instruction >> 4) & 0xf;

            stall = !RF.checkValid(R1) || !RF.checkValid(R2);
            bufRight.setValid(!stall);

            if (!stall)
            {
                bufRight.setSrc1(RF.readContent(R1));
                bufRight.setSrc2(RF.readContent(R2));
                bufRight.setDest(instruction & 0xf);
            }
            else
            {
                RF.setDataHazard(R1, !RF.checkValid(R1));
                RF.setDataHazard(R2, !RF.checkValid(R2));
                currHazardousRegisters += !RF.checkValid(R1) + !RF.checkValid(R2) - (R1 == R2);
            }
            break;
        }

        case LOAD:
        {
            int R1 = (instruction >> 8) & 0xf, R2 = (instruction >> 4) & 0xf;
            stall = !RF.checkValid(R2);
            bufRight.setValid(!stall);

            if (!stall)
            {
                bufRight.setSrc1(R1);
                RF.setValid(R1, false);
                bufRight.setSrc2(RF.readContent(R2));
                bufRight.setOffset(instruction & 0xf);
            }
            else
            {
                RF.setDataHazard(R2, true);
                ++currHazardousRegisters;
            }
            break;
        }

        default:

        {
            if (opcode < 4)
                bufRight.setInstructionType(ARITHMETIC);
            else
                bufRight.setInstructionType(LOGICAL);

            int R1 = (instruction >> 8) & 0xf, R2 = (instruction >> 4) & 0xf, R3 = instruction & 0xf;

            if (opcode != 3 && opcode != 6)
            {
                stall = !RF.checkValid(R2) || !RF.checkValid(R3);
                bufRight.setValid(!stall);

                if (!stall)
                {
                    bufRight.setSrc1(RF.readContent(R2));
                    bufRight.setSrc2(RF.readContent(R3));
                    bufRight.setDest(R1);
                    RF.setValid(R1, false);
                }
                else
                {
                    RF.setDataHazard(R2, !RF.checkValid(R2));
                    RF.setDataHazard(R3, !RF.checkValid(R3));
                    currHazardousRegisters += !RF.checkValid(R2) + !RF.checkValid(R3) - (R2 == R3);
                }
            }
            else
            {
                if (opcode == 3)
                    stall = !RF.checkValid(R1);
                else
                    stall = !RF.checkValid(R2);

                bufRight.setValid(!stall);

                if (!stall)
                {
                    if (opcode == 3)
                        bufRight.setSrc1(RF.readContent(R1));
                    else
                        bufRight.setSrc1(RF.readContent(R2));

                    bufRight.setDest(R1);
                    RF.setValid(R1, false);
                }
                else
                {
                    if (opcode == 3)
                        RF.setDataHazard(R1, true);
                    else
                        RF.setDataHazard(R2, true);

                    ++currHazardousRegisters;
                }
            }
        }
        }
    }
};

class ExecuteStage
{
public:
    DecodeExecuteBuffer &bufLeft;
    ExecuteMemoryBuffer &bufRight;
    bool stall;

    ExecuteStage(DecodeExecuteBuffer &DEBuf, ExecuteMemoryBuffer &EMBuf) : stall(false), bufLeft(DEBuf), bufRight(EMBuf) {}

    int signExtendAddress(int address)
    {
        bool neg = address >> 7;
        return address - neg ? 256 : 0;
    }

    int signExtendOffset(int offset)
    {
        bool neg = offset >> 3;
        return offset - neg ? 16 : 0;
    }

    void execute()
    {
        stall = !bufLeft.checkValid();
        bufRight.setValid(!stall);

        if (stall)
            return;

        int instructionType = bufLeft.getInstructionType();

        bufRight.setInstructionType(instructionType);
        int opcode = bufLeft.getOpcode();

        switch (instructionType)
        {
        case HALT:
        {
            break;
        }

        case BEQZ:
        {
            bool condition = ALU.BEQZ(bufLeft.getSrc1());
            if (condition)
            {
                int byteOffset = signExtendAddress(bufLeft.getOffset() << 1), newAddress = ALU.ADD(PC.read(), byteOffset);
                bufRight.setALUOutput(newAddress);
                PC.write(newAddress);
            }
            branchUndecided = false;
            break;
        }
        case JMP:
        {
            int byteOffset = signExtendAddress(bufLeft.getOffset() << 1), newAddress = ALU.ADD(PC.read(), byteOffset);
            bufRight.setALUOutput(newAddress);
            PC.write(newAddress);
            branchUndecided = false;
            break;
        }

        case STORE:
        {
            bufRight.setSrc(bufLeft.getSrc1());
            bufRight.setALUOutput(ALU.ADD(bufLeft.getSrc2(), bufLeft.getOffset()));
            bufRight.setDest(bufRight.getALUOutput());
            break;
        }

        case LOAD:
        {
            bufRight.setDest(bufLeft.getSrc1());
            bufRight.setALUOutput(ALU.ADD(bufLeft.getSrc2(), bufLeft.getOffset()));
            break;
        }

        case LOGICAL:
        {
            bufRight.setDest(bufLeft.getDest());

            switch (opcode & 3)
            {
            case 0:
                bufRight.setALUOutput(ALU.AND(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;

            case 1:
                bufRight.setALUOutput(ALU.OR(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;

            case 2:
                bufRight.setALUOutput(ALU.NOT(bufLeft.getSrc1()));
                break;

            case 3:
                bufRight.setALUOutput(ALU.XOR(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;
            }
            break;
        }
        case ARITHMETIC:
        {
            bufRight.setDest(bufLeft.getDest());
            switch (opcode & 3)
            {
            case 0:
                bufRight.setALUOutput(ALU.ADD(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;

            case 1:
                bufRight.setALUOutput(ALU.SUB(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;

            case 2:
                bufRight.setALUOutput(ALU.MUL(bufLeft.getSrc1(), bufLeft.getSrc2()));
                break;

            case 3:
                bufRight.setALUOutput(ALU.INC(bufLeft.getSrc1()));
                break;
            }
        }
        }
    }
};

class MemoryStage
{
public:
    bool stall;
    ExecuteMemoryBuffer &bufLeft;
    MemoryWriteBackBuffer &bufRight;
    Register &LMD;

    MemoryStage(ExecuteMemoryBuffer &EMBuf, MemoryWriteBackBuffer &MWBBuf, Register &LMD) : stall(false), bufLeft(EMBuf), bufRight(MWBBuf), LMD(LMD) {}
    void execute()
    {
        stall = !bufLeft.checkValid();
        bufRight.setValid(!stall);

        if (stall)
            return;

        int instructionType = bufLeft.getInstructionType();
        bufRight.setInstructionType(instructionType);

        switch (instructionType)
        {
        case LOAD:
        {
            LMD.setContent(dCache.read(bufLeft.getALUOutput()));
            break;
        }

        case STORE:
        {
            dCache.write(bufLeft.getALUOutput(), bufLeft.getSrc());
            break;
        }
        }

        bufRight.setDest(bufLeft.getDest());
        bufRight.setALUOutput(bufLeft.getALUOutput());
    }
};

class WritebackStage
{
public:
    bool stall, &halt;
    Register &LMD;
    MemoryWriteBackBuffer &bufLeft;

    WritebackStage(Register &LMD, MemoryWriteBackBuffer &MWBBuf, bool &halt) : stall(false), LMD(LMD), bufLeft(MWBBuf), halt(halt) {}

    void execute()
    {
        stall = !bufLeft.checkValid();

        if (stall)
            return;

        int instructionType = bufLeft.getInstructionType();

        switch (instructionType)
        {

        case LOAD:
        {
            if (RF.checkDataHazard(bufLeft.getDest()))
            {
                RF.setDataHazard(bufLeft.getDest(), false);
                --currHazardousRegisters;
            }
            RF.setValid(bufLeft.getDest(), true);
            RF.writeContent(bufLeft.getDest(), LMD.getContent());

            break;
        }

        case ARITHMETIC:
        case LOGICAL:
        {
            if (RF.checkDataHazard(bufLeft.getDest()))
            {
                RF.setDataHazard(bufLeft.getDest(), false);
                --currHazardousRegisters;
            }
            RF.setValid(bufLeft.getDest(), true);
            RF.writeContent(bufLeft.getDest(), bufLeft.getALUOutput());
            break;
        }

        case HALT:
        {
            halt = true;
            return;
        }
        }
    }
};

class PipelinedProcessor
{
public:
    FetchDecodeBuffer FDBuf_left, FDBuf_right;
    DecodeExecuteBuffer DEBuf_left, DEBuf_right;
    ExecuteMemoryBuffer EMBuf_left, EMBuf_right;
    MemoryWriteBackBuffer MWBBuf_left, MWBBuf_right;

    FetchStage fetchStage;
    DecodeStage decodeStage;
    ExecuteStage executeStage;
    MemoryStage memoryStage;
    WritebackStage writebackStage;

    Register LMD_left, LMD_right;

    bool halt;

    PipelinedProcessor() : fetchStage(FDBuf_left), decodeStage(FDBuf_right, DEBuf_left), executeStage(DEBuf_right, EMBuf_left), memoryStage(EMBuf_right, MWBBuf_left, LMD_left), writebackStage(LMD_right, MWBBuf_right, halt)
    {
        std::ifstream instructionInput(ICACHE_FILE), dataInput(DCACHE_FILE), registerFile(REGISTER_FILE);

        int Byte1, Byte2, address = 0;

        while (instructionInput >> std::hex >> Byte1)
        {
            instructionInput >> std::hex >> Byte2;
            iCache.write(address, (Byte1 << 8) + Byte2);
            address += 2;
        }

        address = 0;
        while (dataInput >> std::hex >> Byte1)
        {
            dCache.write(address, Byte1);
            ++address;
        }

        for (int i = 0; i < NUM_REGISTERS; i++)
        {
            int input;
            registerFile >> std::hex >> input;
            RF.writeContent(i, input);
            RF.setValid(i, true);
            RF.setDataHazard(i, false);
        }

        totalInstructions = arithmeticInstructions = logicalInstructions = dataInstructions = controlInstructions = haltInstructions = dataStalls = 0;
        cycles = 1;
        stalls = -4;

        currHazardousRegisters = prevHazardousRegisters = 0;
        branchUndecided = halt = stopFetch = false;
    }

    void executeCycle()
    {
        fetchStage.execute();
        decodeStage.execute();
        if (branchUndecided && !prevBranchUndecided)
            flushFetch();
        prevBranchUndecided = branchUndecided;
        executeStage.execute();
        memoryStage.execute();
        int prevHR = currHazardousRegisters;
        writebackStage.execute();
        if (prevHR && !currHazardousRegisters)
        {
            decodeStage.execute();
            FDBuf_right = FDBuf_left;
            FDBuf_right.setValid(true);
        }

        if (!currHazardousRegisters && !prevHR)
            FDBuf_right = FDBuf_left;

        LMD_right = LMD_left;

        DEBuf_right = DEBuf_left;
        EMBuf_right = EMBuf_left;
        MWBBuf_right = MWBBuf_left;
    }

    void flushFetch()
    {
        FDBuf_left.setValid(false);
        PC.decrement();
    }

    void simulate()
    {
        while (!halt)
        {
            cycles++;
            DecodeExecuteBuffer DEBuf = executeStage.bufLeft;
            executeCycle();
            reviseStats(DEBuf);
        }
    }

    void reviseStats(DecodeExecuteBuffer DEBuf)
    {
        if (currHazardousRegisters > 0)
            ++dataStalls;

        if (executeStage.stall)
            ++stalls;
        else
        {
            ++totalInstructions;
            switch (DEBuf.getInstructionType())
            {
            case ARITHMETIC:
                ++arithmeticInstructions;
                break;

            case LOGICAL:
                ++logicalInstructions;
                break;

            case LOAD:
            case STORE:
                ++dataInstructions;
                break;

            case JMP:
            case BEQZ:
                ++controlInstructions;
                break;

            case HALT:
                ++haltInstructions;
                break;
            }
        }
    }

    void printOutputs()
    {
        std::ofstream DCacheOutput(ODCACHE_FILE), statsOutput(STATS_FILE);

        for (int i = 0; i < NUM_SETS * BLOCK_SIZE; i++)
        {
                DCacheOutput << std::hex << ((dCache.read(i) & 0xf0)>>4) << (dCache.read(i) & 0xf) << std::endl;
        }

        statsOutput << std::dec << "Total number of instructions executed: " << totalInstructions << std::endl;
        statsOutput << std::dec << "Number of instructions in each class" << std::endl;
        statsOutput << std::dec << "Arithmetic instructions              : " << arithmeticInstructions << std::endl;
        statsOutput << std::dec << "Logical instructions                 : " << logicalInstructions << std::endl;
        statsOutput << std::dec << "Data instructions                    : " << dataInstructions << std::endl;
        statsOutput << std::dec << "Control instructions                 : " << controlInstructions << std::endl;
        statsOutput << std::dec << "Halt instructions                    : " << haltInstructions << std::endl;
        statsOutput << std::dec << "Cycles Per Instruction               : " << (double)(cycles - 1) / totalInstructions << std::endl;
        statsOutput << std::dec << "Total number of stalls               : " << stalls << std::endl;
        statsOutput << std::dec << "Data stalls (RAW)                    : " << dataStalls << std::endl;
        statsOutput << std::dec << "Control stalls                       : " << stalls - dataStalls << std::endl;

        DCacheOutput.close();
        statsOutput.close();
    }
};

int main()
{
    PipelinedProcessor simulator;

    simulator.simulate();
    simulator.printOutputs();
}
