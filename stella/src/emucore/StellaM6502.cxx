//============================================================================
//
// MM     MM  6666  555555  0000   2222
// MMMM MMMM 66  66 55     00  00 22  22
// MM MMM MM 66     55     00  00     22
// MM  M  MM 66666  55555  00  00  22222  --  "A 6502 Microprocessor Emulator"
// MM     MM 66  66     55 00  00 22
// MM     MM 66  66 55  55 00  00 22
// MM     MM  6666   5555   0000  222222
//
// Copyright (c) 1995-2014 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: M6502.cxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

  // Flags for disassembly types
  #define DISASM_CODE  0
  #define DISASM_GFX   0
  #define DISASM_PGFX  0
  #define DISASM_DATA  0
  #define DISASM_ROW   0
  #define DISASM_NONE  0
#include "Settings.hxx"

#include "M6502.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
M6502::M6502(uInt32 systemCyclesPerProcessorCycle, const Settings& settings)
  : myExecutionStatus(0),
    mySystem(0),
    mySettings(settings),
    mySystemCyclesPerProcessorCycle(systemCyclesPerProcessorCycle),
    myLastAccessWasRead(true),
    myTotalInstructionCount(0),
    myNumberOfDistinctAccesses(0),
    myLastAddress(0),
    myLastPeekAddress(0),
    myLastPokeAddress(0),
    myLastSrcAddressS(-1),
    myLastSrcAddressA(-1),
    myLastSrcAddressX(-1),
    myLastSrcAddressY(-1),
    myDataAddressForPoke(0)
{

  // Compute the System Cycle table
  for(uInt32 t = 0; t < 256; ++t)
  {
    myInstructionSystemCycleTable[t] = ourInstructionCycleTable[t] *
        mySystemCyclesPerProcessorCycle;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
M6502::~M6502()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void M6502::install(System& system)
{
  // Remember which system I'm installed in
  mySystem = &system;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void M6502::reset()
{
  // Clear the execution status flags
  myExecutionStatus = 0;

  // Set registers to default values
  SP = 0xff;
#ifndef TARGET_GNW
  if(mySettings.getBool("cpurandom"))
  {
    A = mySystem->randGenerator().next();
    X = mySystem->randGenerator().next();
    Y = mySystem->randGenerator().next();
    PS(mySystem->randGenerator().next());
  }
  else
  {
    A = X = Y = 0;
    PS(0x20);
  }
#else
  A = X = Y = 0;
  PS(0x20);
#endif

  // Reset access flag
  myLastAccessWasRead = true;

  // Load PC from the reset vector
  PC = (uInt16)mySystem->peek(0xfffc) | ((uInt16)mySystem->peek(0xfffd) << 8);

  myTotalInstructionCount = 0;

  myLastAddress = myLastPeekAddress = myLastPokeAddress = 0;
  myLastSrcAddressS = myLastSrcAddressA =
    myLastSrcAddressX = myLastSrcAddressY = -1;
  myDataAddressForPoke = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline uInt8 M6502::peek(uInt16 address, uInt8 flags)
{
  ////////////////////////////////////////////////
  // TODO - move this logic directly into CartAR
  if(address != myLastAddress)
  {
    myNumberOfDistinctAccesses++;
    myLastAddress = address;
  }
  ////////////////////////////////////////////////
  mySystem->incrementCycles(mySystemCyclesPerProcessorCycle);

  uInt8 result = mySystem->peek(address, flags);
  myLastAccessWasRead = true;
  myLastPeekAddress = address;
  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline void M6502::poke(uInt16 address, uInt8 value)
{
  ////////////////////////////////////////////////
  // TODO - move this logic directly into CartAR
  if(address != myLastAddress)
  {
    myNumberOfDistinctAccesses++;
    myLastAddress = address;
  }
  ////////////////////////////////////////////////
  mySystem->incrementCycles(mySystemCyclesPerProcessorCycle);

  mySystem->poke(address, value);
  myLastAccessWasRead = false;
  myLastPokeAddress = address;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool M6502::execute(uInt32 number)
{
  // Clear all of the execution status bits except for the fatal error bit
  myExecutionStatus &= FatalErrorBit;

  // Loop until execution is stopped or a fatal error occurs
  for(;;)
  {
    for(; !myExecutionStatus && (number != 0); --number)
    {
      uInt16 operandAddress = 0, intermediateAddress = 0;
      uInt8 operand = 0;

      // Reset the peek/poke address pointers
      myLastPeekAddress = myLastPokeAddress = myDataAddressForPoke = 0;

      // Fetch instruction at the program counter
      IR = peek(PC++, DISASM_CODE);  // This address represents a code section

      // Call code to execute the instruction
      switch(IR)
      {
        // 6502 instruction emulation is generated by an M4 macro file
        #include "M6502.ins"

        default:
          // Oops, illegal instruction executed so set fatal error flag
          myExecutionStatus |= FatalErrorBit;
      }
      myTotalInstructionCount++;
    }

    // See if we need to handle an interrupt
    if((myExecutionStatus & MaskableInterruptBit) || 
        (myExecutionStatus & NonmaskableInterruptBit))
    {
      // Yes, so handle the interrupt
      interruptHandler();
    }

    // See if execution has been stopped
    if(myExecutionStatus & StopExecutionBit)
    {
      // Yes, so answer that everything finished fine
      return true;
    }

    // See if a fatal error has occured
    if(myExecutionStatus & FatalErrorBit)
    {
      // Yes, so answer that something when wrong
      return false;
    }

    // See if we've executed the specified number of instructions
    if(number == 0)
    {
      // Yes, so answer that everything finished fine
      return true;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void M6502::interruptHandler()
{
  // Handle the interrupt
  if((myExecutionStatus & MaskableInterruptBit) && !I)
  {
    mySystem->incrementCycles(7 * mySystemCyclesPerProcessorCycle);
    mySystem->poke(0x0100 + SP--, (PC - 1) >> 8);
    mySystem->poke(0x0100 + SP--, (PC - 1) & 0x00ff);
    mySystem->poke(0x0100 + SP--, PS() & (~0x10));
    D = false;
    I = true;
    PC = (uInt16)mySystem->peek(0xFFFE) | ((uInt16)mySystem->peek(0xFFFF) << 8);
  }
  else if(myExecutionStatus & NonmaskableInterruptBit)
  {
    mySystem->incrementCycles(7 * mySystemCyclesPerProcessorCycle);
    mySystem->poke(0x0100 + SP--, (PC - 1) >> 8);
    mySystem->poke(0x0100 + SP--, (PC - 1) & 0x00ff);
    mySystem->poke(0x0100 + SP--, PS() & (~0x10));
    D = false;
    PC = (uInt16)mySystem->peek(0xFFFA) | ((uInt16)mySystem->peek(0xFFFB) << 8);
  }

  // Clear the interrupt bits in myExecutionStatus
  myExecutionStatus &= ~(MaskableInterruptBit | NonmaskableInterruptBit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool M6502::save(Serializer& out) const
{
  const string& CPU = name();

#ifndef TARGET_GNW
  try
  {
    out.putString(CPU);

    out.putByte(A);    // Accumulator
    out.putByte(X);    // X index register
    out.putByte(Y);    // Y index register
    out.putByte(SP);   // Stack Pointer
    out.putByte(IR);   // Instruction register
    out.putShort(PC);  // Program Counter

    out.putBool(N);    // N flag for processor status register
    out.putBool(V);    // V flag for processor status register
    out.putBool(B);    // B flag for processor status register
    out.putBool(D);    // D flag for processor status register
    out.putBool(I);    // I flag for processor status register
    out.putBool(notZ); // Z flag complement for processor status register
    out.putBool(C);    // C flag for processor status register

    out.putByte(myExecutionStatus);

    // Indicates the number of distinct memory accesses
    out.putInt(myNumberOfDistinctAccesses);
    // Indicates the last address(es) which was accessed
    out.putShort(myLastAddress);
    out.putShort(myLastPeekAddress);
    out.putShort(myLastPokeAddress);
    out.putShort(myDataAddressForPoke);
    out.putInt(myLastSrcAddressS);
    out.putInt(myLastSrcAddressA);
    out.putInt(myLastSrcAddressX);
    out.putInt(myLastSrcAddressY);
  }
  catch(...)
  {
    return false;
  }

  return true;
#else
  return true;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool M6502::load(Serializer& in)
{
  const string& CPU = name();

#ifndef TARGET_GNW
  try
  {
    if(in.getString() != CPU)
      return false;

    A = in.getByte();    // Accumulator
    X = in.getByte();    // X index register
    Y = in.getByte();    // Y index register
    SP = in.getByte();   // Stack Pointer
    IR = in.getByte();   // Instruction register
    PC = in.getShort();  // Program Counter

    N = in.getBool();    // N flag for processor status register
    V = in.getBool();    // V flag for processor status register
    B = in.getBool();    // B flag for processor status register
    D = in.getBool();    // D flag for processor status register
    I = in.getBool();    // I flag for processor status register
    notZ = in.getBool(); // Z flag complement for processor status register
    C = in.getBool();    // C flag for processor status register

    myExecutionStatus = in.getByte();

    // Indicates the number of distinct memory accesses
    myNumberOfDistinctAccesses = in.getInt();
    // Indicates the last address(es) which was accessed
    myLastAddress = in.getShort();
    myLastPeekAddress = in.getShort();
    myLastPokeAddress = in.getShort();
    myDataAddressForPoke = in.getShort();
    myLastSrcAddressS = in.getInt();
    myLastSrcAddressA = in.getInt();
    myLastSrcAddressX = in.getInt();
    myLastSrcAddressY = in.getInt();
  }
  catch(...)
  {
    return false;
  }

  return true;
#else
  return true;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 M6502::ourInstructionCycleTable[256] = {
//  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,  // 0
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 1
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,  // 2
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 3
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,  // 4
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 5
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,  // 6
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // 7
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,  // 8
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,  // 9
    2, 6, 2, 6, 3, 3, 3, 4, 2, 2, 2, 2, 4, 4, 4, 4,  // a
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,  // b
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // c
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,  // d
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // e
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7   // f
};
