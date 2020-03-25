/* Copyright 2017 Adam Green (http://mbed.org/users/AdamGreen/)

   Copyright 2020 (insert proper SiFive copyright boilerplate here)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
/* Routines to expose the RISC-V functionality to the mri debugger. */

/* At this stage, any code here was copied from the ARM-targeted version,
   and may well not stick around long. */

#include <signal.h>
#include <platforms.h>
#include <gdb_console.h>
#include "riscv.h"
#include "riscv_inst.h"

#if defined ( __GNUC__ )
  #define __ASM            __asm                                      /*!< asm keyword for GNU Compiler          */
  #define __INLINE         inline                                     /*!< inline keyword for GNU Compiler       */
  #define __STATIC_INLINE  static inline
#endif

/* Disable any macro used for errno and use the int global instead. */
#undef errno
extern int errno;

RiscVState    __mriRiscVState;

/* trigger free list and allocated list are singly-linked lists, NULL terminated */
typedef struct RiscVTrigger RISCV_TRIGGER;
struct RiscVTrigger {
  int idx;
  int allocated;
  int programmed;
  uint32_t addr;
};

#define MAX_TRIGGERS 16
RISCV_TRIGGER triggerPool[MAX_TRIGGERS];
int triggerMaxValid;  /* actually "one beyond the max valid trigger index" */

static RISCV_TRIGGER *triggerForSingleStep;

#define DISABLE_APPARENTLY_ARM_SPECIFIC_CODE 1

/* NOTE: This is the original version of the following XML which has had things stripped to reduce the amount of
         FLASH consumed by the debug monitor.  This includes the removal of the copyright comment.
<?xml version="1.0"?>
<!-- Copyright (C) 2010, 2011 Free Software Foundation, Inc.

     Copying and distribution of this file, with or without modification,
     are permitted in any medium without royalty provided the copyright
     notice and this notice are preserved.  -->

<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<feature name="org.gnu.gdb.arm.m-profile">
  <reg name="r0" bitsize="32"/>
  <reg name="r1" bitsize="32"/>
  <reg name="r2" bitsize="32"/>
  <reg name="r3" bitsize="32"/>
  <reg name="r4" bitsize="32"/>
  <reg name="r5" bitsize="32"/>
  <reg name="r6" bitsize="32"/>
  <reg name="r7" bitsize="32"/>
  <reg name="r8" bitsize="32"/>
  <reg name="r9" bitsize="32"/>
  <reg name="r10" bitsize="32"/>
  <reg name="r11" bitsize="32"/>
  <reg name="r12" bitsize="32"/>
  <reg name="sp" bitsize="32" type="data_ptr"/>
  <reg name="lr" bitsize="32"/>
  <reg name="pc" bitsize="32" type="code_ptr"/>
  <reg name="xpsr" bitsize="32" regnum="25"/>
</feature>
*/
static const char g_targetXml[] = 
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n"
    "<target>\n"
    "<feature name=\"org.gnu.gdb.arm.m-profile\">\n"
    "<reg name=\"r0\" bitsize=\"32\"/>\n"
    "<reg name=\"r1\" bitsize=\"32\"/>\n"
    "<reg name=\"r2\" bitsize=\"32\"/>\n"
    "<reg name=\"r3\" bitsize=\"32\"/>\n"
    "<reg name=\"r4\" bitsize=\"32\"/>\n"
    "<reg name=\"r5\" bitsize=\"32\"/>\n"
    "<reg name=\"r6\" bitsize=\"32\"/>\n"
    "<reg name=\"r7\" bitsize=\"32\"/>\n"
    "<reg name=\"r8\" bitsize=\"32\"/>\n"
    "<reg name=\"r9\" bitsize=\"32\"/>\n"
    "<reg name=\"r10\" bitsize=\"32\"/>\n"
    "<reg name=\"r11\" bitsize=\"32\"/>\n"
    "<reg name=\"r12\" bitsize=\"32\"/>\n"
    "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
    "<reg name=\"lr\" bitsize=\"32\"/>\n"
    "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
    "<reg name=\"xpsr\" bitsize=\"32\" regnum=\"25\"/>\n"
    "</feature>\n"
    "<feature name=\"org.gnu.gdb.arm.m-system\">\n"
    "<reg name=\"msp\" bitsize=\"32\" regnum=\"26\"/>\n"
    "<reg name=\"psp\" bitsize=\"32\" regnum=\"27\"/>\n"
    "<reg name=\"primask\" bitsize=\"32\" regnum=\"28\"/>\n"
    "<reg name=\"basepri\" bitsize=\"32\" regnum=\"29\"/>\n"
    "<reg name=\"faultmask\" bitsize=\"32\" regnum=\"30\"/>\n"
    "<reg name=\"control\" bitsize=\"32\" regnum=\"31\"/>\n"
    "</feature>\n"
#if MRI_DEVICE_HAS_FPU
    "<feature name=\"org.gnu.gdb.arm.vfp\">\n"
    "<reg name=\"d0\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d1\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d2\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d3\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d4\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d5\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d6\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d7\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d8\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d9\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d10\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d11\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d12\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d13\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d14\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"d15\" bitsize=\"64\" type=\"ieee_double\"/>\n"
    "<reg name=\"fpscr\" bitsize=\"32\" type=\"int\" group=\"float\"/>\n"
    "</feature>\n"
#endif
    "</target>\n";

/* Macro to provide index for specified register in the SContext structure. */
#define CONTEXT_MEMBER_INDEX(MEMBER) (offsetof(Context, MEMBER)/sizeof(uint32_t))


/* Reference this handler in the ASM module to make sure that it gets linked in. */
void __mriExceptionHandler(void);


static void clearState(void);
static void initSingleStep(void);
static void disableSingleStep(void);
static void enableSingleStep(void);
static uint32_t getCurrentlyExecutingExceptionNumber(void);
static int isInstruction32Bit(uint16_t firstWordOfInstruction);
static uint16_t getHalfWord(uint32_t address);
static void triggersInit();
static int triggersIsValidIdx(uint32_t idx);
static RISCV_TRIGGER *triggerAlloc(uint32_t addr);
static void triggerSetAddr(RISCV_TRIGGER *t, uint32_t addr);
static RISCV_TRIGGER *triggerFindByAddr(uint32_t addr);
static void triggerProgramHw(RISCV_TRIGGER *t);
static void triggerClearHw(RISCV_TRIGGER *t);
static void triggerFree(RISCV_TRIGGER *t);


void __mriRiscVInit(Token* pParameterTokens)
{

    /* Reference routine in ASM module to make sure that is gets linked in. */
    (void)pParameterTokens;

    clearState();
    Platform_DisableSingleStep();
    triggersInit();
    initSingleStep();  /* Must happen after triggersInit, for implementation reasons, */
                       /* because it is using an execution trigger internally */
}



static __INLINE int isMPUNotPresent(void)
{
    // RESOLVE - implement for RISC-V
  return 1;
}


static uint32_t getCurrentlyExecutingExceptionNumber(void)
{
  return __mriRiscVState.context.mcause;
}

static void clearState(void)
{
    // RESOLVE - implement for RISC-V
}

static __INLINE int prepareToAccessMPURegion(uint32_t regionNumber)
{
    // RESOLVE - implement for RISC-V
  return 0;
}

static __INLINE uint32_t getCurrentMPURegionNumber(void)
{
    // RESOLVE - implement for RISC-V
  return 0;
}

static __INLINE void setMPURegionAddress(uint32_t address)
{
    if (isMPUNotPresent())
        return;

    // RESOLVE - implement for RISC-V    
}

static __INLINE uint32_t getMPURegionAddress(void)
{
    if (isMPUNotPresent())
        return 0;

    // RESOLVE - implement for RISC-V        
}

static __INLINE void setMPURegionAttributeAndSize(uint32_t attributeAndSize)
{
    if (isMPUNotPresent())
        return;

}

static __INLINE uint32_t getMPURegionAttributeAndSize(void)
{
    if (isMPUNotPresent())
        return 0;

}


static void triggerSetAddr(RISCV_TRIGGER *trigger, uint32_t addr)
{
  if (trigger) {
    trigger->addr = addr;
  }
}

static RISCV_TRIGGER *triggerFindByAddr(uint32_t addr)
{
  int i;
  
  for (i = 0; i < triggerMaxValid; i++)
    if (triggerPool[i].allocated && triggerPool[i].addr == addr)
      return &triggerPool[i];

  return NULL;
}

static void triggerProgramHw(RISCV_TRIGGER *trigger)
{
  volatile uint32_t idxVal;
  volatile uint32_t tdata1Val;
  volatile uint32_t addrVal;

  if (trigger == NULL)
    return;
  
  idxVal = trigger->idx;
  addrVal = trigger->addr;

  /*
    Bit 19 should be 0 (match virtual address)
    bits 16 and 17 should be 0 (sizelo = 0, meaning any sized access)
    action (bits 12 - 15) should be zero (break exception)
    chain = 0
    match = 0
    m=1
    s=1
    u=1
    execute=1
    store=0
    load=0

    So basically OR in 0b101_1100 or 0x5C
  */  
  
    __asm volatile ("csrw tselect, %0" : : "r" (idxVal));
    __asm volatile ("csrw tdata2, %0" : : "r" (addrVal));    
    __asm volatile ("csrr %0, tdata1" : "=r"(tdata1Val) : );
    tdata1Val |= 0x5C;
    __asm volatile ("csrw tdata1, %0" : : "r" (tdata1Val));

    trigger->programmed = 1;
}

static void triggerClearHw(RISCV_TRIGGER *trigger)
{
  volatile uint32_t idxVal = trigger->idx;
  volatile uint32_t tdata1Val;

  if (trigger == NULL)
    return;

  /* RISC-V triggers don't really have a "clear", but setting m, s, and u to 0,
     or setting execute, load, and store to 0 will make the trigger a no-op.
     We'll do both.  So, set bits 6..0 to 0 */
    __asm volatile ("csrw tselect, %0" : : "r" (idxVal));
    __asm volatile ("csrr %0, tdata1" : "=r"(tdata1Val) : );
    tdata1Val &= ~0x7F;
    __asm volatile ("csrw tdata1, %0" : : "r" (tdata1Val));

    /* Might as well clear the "programmed" field which can help debugging this debugger
       (in conjunction with NOT clearing the address field so we can see which address was
       most recently used for this trigger slot) */
    trigger->programmed = 0;
}


static void initSingleStep(void)
{
  triggerForSingleStep = triggerAlloc(0 /* placeholder address for now */);
}

static void disableSingleStep(void)
{
    // Clear execution trigger we're  using internally for single step
    if (triggerForSingleStep != NULL) {
      triggerClearHw(triggerForSingleStep);      
    }
    
}

static void enableSingleStep(void)
{
  // For now, we'll assume there is at least one RISC-V execution trigger available
  // and we'll use that to set a temporary breakpoint on where we expect the pc to move
  // after the current instruction.
    uint16_t firstWordOfCurrentInstruction;
    uint16_t secondWordOfCurrentInstruction;    
    RISCV_X_VAL addrToBreakOn;
    uint32_t inst32;
    RISCV_X_UNSIGNED gprs[32];
    int i;
    
    __try
    {
        firstWordOfCurrentInstruction = getHalfWord(__mriRiscVState.context.mepc);
        secondWordOfCurrentInstruction = getHalfWord(__mriRiscVState.context.mepc + 2);	
    }
    __catch
    {
        /* Will get here if PC isn't pointing to valid memory so treat as other. */
        clearExceptionCode();
        return;
    }

    inst32 = (secondWordOfCurrentInstruction << 16) | firstWordOfCurrentInstruction;

    gprs[0] = 0;
    for (i = 1; i <= 31; i++) {
      gprs[i] = __mriRiscVState.context.x_1_31[i-1];
    }

    addrToBreakOn = riscv_next_pc(__mriRiscVState.context.mepc, inst32, gprs);    

    // set an execution trigger
    if (triggerForSingleStep != NULL) {
      triggerSetAddr(triggerForSingleStep, addrToBreakOn);
      triggerProgramHw(triggerForSingleStep);      
    }

}


static void clearSingleSteppingFlag(void);
void Platform_DisableSingleStep(void)
{
    disableSingleStep();
    clearSingleSteppingFlag();
}

static void clearSingleSteppingFlag(void)
{
  __mriRiscVState.flags &= ~MRI_RISCV_FLAG_SINGLE_STEPPING;
}


//static uint32_t getNvicVector(IRQn_Type irq);
static void     setSingleSteppingFlag(void);
static void     setSingleSteppingFlag(void);



void Platform_EnableSingleStep(void)
{
  setSingleSteppingFlag();
  enableSingleStep();
}



static void setSingleSteppingFlag(void)
{
  __mriRiscVState.flags |= MRI_RISCV_FLAG_SINGLE_STEPPING;
}


static uint16_t throwingMemRead16(uint32_t address)
{
    uint16_t instructionWord = Platform_MemRead16((const uint16_t*)address);
    if (Platform_WasMemoryFaultEncountered())
        __throw_and_return(memFaultException, 0);
    return instructionWord;
}


static uint16_t getHalfWord(uint32_t address)
{
  return throwingMemRead16(address);
}


int Platform_IsSingleStepping(void)
{
    return __mriRiscVState.flags & MRI_RISCV_FLAG_SINGLE_STEPPING;    
}


char* Platform_GetPacketBuffer(void)
{
    return __mriRiscVState.packetBuffer;
}


uint32_t Platform_GetPacketBufferSize(void)
{
    return sizeof(__mriRiscVState.packetBuffer);    
}


uint8_t Platform_DetermineCauseOfException(void)
{
    uint32_t exceptionNumber = getCurrentlyExecutingExceptionNumber();
    
    switch(exceptionNumber)
    {
    case 0x3:
        /* Breakpoint exception */
        return SIGTRAP;
    case 0x8000000B:
        /* External interrupt (machine mode)... should be UART in our case (otherwise we're getting interrupts we didn't ask for) */
        return SIGINT;
    default:
        /* NOTE: Catch all signal will be SIGSTOP. */
        return SIGSTOP;
    }
}



static void displayHardFaultCauseToGdbConsole(void);
static void displayMemFaultCauseToGdbConsole(void);
static void displayBusFaultCauseToGdbConsole(void);
static void displayUsageFaultCauseToGdbConsole(void);
void Platform_DisplayFaultCauseToGdbConsole(void)
{
  return;
    switch (getCurrentlyExecutingExceptionNumber())
    {
    case 3:
        /* HardFault */
        displayHardFaultCauseToGdbConsole();
        break;
    case 4:
        /* MemManage */
        displayMemFaultCauseToGdbConsole();
        break;
    case 5:
        /* BusFault */
        displayBusFaultCauseToGdbConsole();
        break;
    case 6:
        /* UsageFault */
        displayUsageFaultCauseToGdbConsole();
        break;
    default:
        return;
    }
    WriteStringToGdbConsole("\n");
}

static void displayHardFaultCauseToGdbConsole(void)
{
#if 0
    static const uint32_t debugEventBit = 1 << 31;
    static const uint32_t forcedBit = 1 << 30;
    static const uint32_t vectorTableReadBit = 1 << 1;
    uint32_t              hardFaultStatusRegister = SCB->HFSR;
    
    WriteStringToGdbConsole("\n**Hard Fault**");
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(hardFaultStatusRegister);
    
    if (hardFaultStatusRegister & debugEventBit)
        WriteStringToGdbConsole("\n    Debug Event");

    if (hardFaultStatusRegister & vectorTableReadBit)
        WriteStringToGdbConsole("\n    Vector Table Read");

    if (hardFaultStatusRegister & forcedBit)
    {
        WriteStringToGdbConsole("\n    Forced");
        displayMemFaultCauseToGdbConsole();
        displayBusFaultCauseToGdbConsole();
        displayUsageFaultCauseToGdbConsole();
    }
#endif  
  
}

static void displayMemFaultCauseToGdbConsole(void)
{
#if 0
    static const uint32_t MMARValidBit = 1 << 7;
    static const uint32_t FPLazyStatePreservationBit = 1 << 5;
    static const uint32_t stackingErrorBit = 1 << 4;
    static const uint32_t unstackingErrorBit = 1 << 3;
    static const uint32_t dataAccess = 1 << 1;
    static const uint32_t instructionFetch = 1;
    uint32_t              memManageFaultStatusRegister = SCB->CFSR & 0xFF;
    
    /* Check to make sure that there is a memory fault to display. */
    if (memManageFaultStatusRegister == 0)
        return;
    
    WriteStringToGdbConsole("\n**MPU Fault**");
    WriteStringToGdbConsole("\n  Status Register: ");
    WriteHexValueToGdbConsole(memManageFaultStatusRegister);
    
    if (memManageFaultStatusRegister & MMARValidBit)
    {
        WriteStringToGdbConsole("\n    Fault Address: ");
        WriteHexValueToGdbConsole(SCB->MMFAR);
    }
    if (memManageFaultStatusRegister & FPLazyStatePreservationBit)
        WriteStringToGdbConsole("\n    FP Lazy Preservation");

    if (memManageFaultStatusRegister & stackingErrorBit)
    {
        WriteStringToGdbConsole("\n    Stacking Error w/ SP = ");
        WriteHexValueToGdbConsole(__mriCortexMState.taskSP);
    }
    if (memManageFaultStatusRegister & unstackingErrorBit)
    {
        WriteStringToGdbConsole("\n    Unstacking Error w/ SP = ");
        WriteHexValueToGdbConsole(__mriCortexMState.taskSP);
    }
    if (memManageFaultStatusRegister & dataAccess)
        WriteStringToGdbConsole("\n    Data Access");

    if (memManageFaultStatusRegister & instructionFetch)
        WriteStringToGdbConsole("\n    Instruction Fetch");
#endif  
}

static void displayBusFaultCauseToGdbConsole(void)
{
  
}

static void displayUsageFaultCauseToGdbConsole(void)
{
  
}























/* From mri.c:
   These two routines can be provided by the debuggee to get notified on debugger entry/exit.  Can be used to safely
   turn off some external hardware so that it doesn't keep running while sitting at a breakpoint.
void __mriPlatform_EnteringDebuggerHook(void) __attribute__((weak));
void __mriPlatform_LeavingDebuggerHook(void) __attribute__((weak));
*/

void __mriPlatform_EnteringDebuggerHook(void)
{

}

void __mriPlatform_LeavingDebuggerHook(void)
{

}


static void checkStack(void);
void Platform_LeavingDebugger(void)
{
    checkStack();
    __mriRiscVState.flags |= MRI_RISCV_FLAG_EXITING;
}


static void checkStack(void)
{
#if 0
    uint32_t* pCurr = (uint32_t*)__mriCortexMState.debuggerStack;
    uint8_t*  pEnd = (uint8_t*)__mriCortexMState.debuggerStack + sizeof(__mriCortexMState.debuggerStack);
    int       spaceUsed;
    
    while ((uint8_t*)pCurr < pEnd && *pCurr == CORTEXM_DEBUGGER_STACK_FILL)
        pCurr++;

    spaceUsed = pEnd - (uint8_t*)pCurr;
    if (spaceUsed > __mriCortexMState.maxStackUsed)
        __mriCortexMState.maxStackUsed = spaceUsed;
#endif  
}


int Platform_CommSharingWithApplication(void)
{
  return 0;
}

uint32_t Platform_GetProgramCounter(void)
{
    return __mriRiscVState.context.mepc;
}


void Platform_SetProgramCounter(uint32_t newPC)
{
    // This interface will need attention in order to support RV64
    __mriRiscVState.context.mepc = newPC;
}


static int isInstruction32Bit(uint16_t firstWordOfInstruction);
void Platform_AdvanceProgramCounterToNextInstruction(void)
{
    uint16_t  firstWordOfCurrentInstruction;
    
    __try
    {
        firstWordOfCurrentInstruction = getHalfWord(__mriRiscVState.context.mepc);
    }
    __catch
    {
        /* Will get here if PC isn't pointing to valid memory so don't bother to advance. */
        clearExceptionCode();
        return;
    }
    
    if (isInstruction32Bit(firstWordOfCurrentInstruction))
    {
        /* 32-bit Instruction. */
	__mriRiscVState.context.mepc += 4;
    }
    else
    {
        /* 16-bit Instruction. */      
	__mriRiscVState.context.mepc += 2;	
    }
}

static int isInstruction32Bit(uint16_t firstWordOfInstruction)
{
    uint16_t maskedOffLower2BitsOfWord = firstWordOfInstruction & 0x3;
    
    /* 32-bit have binary 11 in lowest bits */
    return  (maskedOffLower2BitsOfWord == 0x3);
}


int Platform_WasProgramCounterModifiedByUser(void)
{
    return __mriRiscVState.context.mepc != __mriRiscVState.originalPC;      
}


static int isInstructionMbedSemihostBreakpoint(uint16_t instruction);
static int isInstructionNewlibSemihostBreakpoint(uint16_t instruction);
static int isInstructionHardcodedBreakpoint(uint16_t instruction);
PlatformInstructionType Platform_TypeOfCurrentInstruction(void)
{
    uint16_t currentInstruction;
    
    __try
    {
      currentInstruction = getHalfWord(__mriRiscVState.context.mepc);
    }
    __catch
    {
        /* Will get here if PC isn't pointing to valid memory so treat as other. */
        clearExceptionCode();
        return MRI_PLATFORM_INSTRUCTION_OTHER;
    }
    
    if (isInstructionMbedSemihostBreakpoint(currentInstruction))
        return MRI_PLATFORM_INSTRUCTION_MBED_SEMIHOST_CALL;
    else if (isInstructionNewlibSemihostBreakpoint(currentInstruction))
        return MRI_PLATFORM_INSTRUCTION_NEWLIB_SEMIHOST_CALL;
    else if (isInstructionHardcodedBreakpoint(currentInstruction))
        return MRI_PLATFORM_INSTRUCTION_HARDCODED_BREAKPOINT;
    else
        return MRI_PLATFORM_INSTRUCTION_OTHER;
}

static int isInstructionMbedSemihostBreakpoint(uint16_t instruction)
{
    static const uint16_t mbedSemihostBreakpointMachineCode = 0xbeab;

    return mbedSemihostBreakpointMachineCode == instruction;
}

static int isInstructionNewlibSemihostBreakpoint(uint16_t instruction)
{
    static const uint16_t newlibSemihostBreakpointMachineCode = 0xbeff;

    return (newlibSemihostBreakpointMachineCode == instruction);
}

static int isInstructionHardcodedBreakpoint(uint16_t instruction)
{
    static const uint16_t hardCodedBreakpointMachineCode = 0xbe00;

    return (hardCodedBreakpointMachineCode == instruction);
}


PlatformSemihostParameters Platform_GetSemihostCallParameters(void)
{
    PlatformSemihostParameters parameters;  
    parameters.parameter1 = 0;
    parameters.parameter2 = 0;
    parameters.parameter3 = 0;
    parameters.parameter4 = 0;
    // implement appropriately for RISC-V
    return parameters;  
}


void Platform_SetSemihostCallReturnAndErrnoValues(int returnValue, int err)
{
#if 0
    __mriCortexMState.context.R0 = returnValue;  
#else
    // implement for RISC-V
#endif  

    if (returnValue < 0)
        errno = err;
}



#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
static void sendRegisterForTResponse(Buffer* pBuffer, uint8_t registerOffset, uint32_t registerValue);
static void writeBytesToBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount);
#endif

void Platform_WriteTResponseRegistersToBuffer(Buffer* pBuffer)
{
#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
    sendRegisterForTResponse(pBuffer, CONTEXT_MEMBER_INDEX(R7), __mriCortexMState.context.R7);
    sendRegisterForTResponse(pBuffer, CONTEXT_MEMBER_INDEX(SP), __mriCortexMState.context.SP);
    sendRegisterForTResponse(pBuffer, CONTEXT_MEMBER_INDEX(LR), __mriCortexMState.context.LR);
    sendRegisterForTResponse(pBuffer, CONTEXT_MEMBER_INDEX(PC), __mriCortexMState.context.PC);
#else
    // implement for RISC-V
#endif  
}

#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
static void sendRegisterForTResponse(Buffer* pBuffer, uint8_t registerOffset, uint32_t registerValue)
{
    Buffer_WriteByteAsHex(pBuffer, registerOffset);
    Buffer_WriteChar(pBuffer, ':');
    writeBytesToBufferAsHex(pBuffer, &registerValue, sizeof(registerValue));
    Buffer_WriteChar(pBuffer, ';');
}

#endif

static void writeBytesToBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount)
{
    uint8_t* pByte = (uint8_t*)pBytes;
    size_t   i;
    
    for (i = 0 ; i < byteCount ; i++)
        Buffer_WriteByteAsHex(pBuffer, *pByte++);
}



void Platform_CopyContextToBuffer(Buffer* pBuffer)
{
    /* x0 isn't part of the context structure, so fake that one */
    RISCV_X_VAL zero = 0;
    writeBytesToBufferAsHex(pBuffer, &zero, sizeof(zero));
    /* x1 through x31 are from context structure */
    writeBytesToBufferAsHex(pBuffer, &__mriRiscVState.context.x_1_31, sizeof(__mriRiscVState.context.x_1_31));
    /* pc (this is the captured MEPC value) */
    writeBytesToBufferAsHex(pBuffer, &__mriRiscVState.context.mepc, sizeof(__mriRiscVState.context.mepc));    
}

#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
static void readBytesFromBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount);
#endif

void Platform_CopyContextFromBuffer(Buffer* pBuffer)
{
#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
    readBytesFromBufferAsHex(pBuffer, &__mriCortexMState.context, sizeof(__mriCortexMState.context));
#else
    // implement for RISC-V
#endif  
}

#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
static void readBytesFromBufferAsHex(Buffer* pBuffer, void* pBytes, size_t byteCount)
{
    uint8_t* pByte = (uint8_t*)pBytes;
    size_t   i;
    
    for (i = 0 ; i < byteCount; i++)
        *pByte++ = Buffer_ReadByteAsHex(pBuffer);
}
#endif

void Platform_SetHardwareBreakpoint(uint32_t address, uint32_t kind)
{
    RISCV_TRIGGER *trigger;

    trigger = triggerAlloc(address);

    if (trigger == NULL)
        __throw(exceededHardwareResourcesException);

    triggerSetAddr(trigger, address);
    triggerProgramHw(trigger);
}

#if 0  /* it appears this is unused for RISC-V, and generates a warnings-as-errors compilation error */
static int doesKindIndicate32BitInstruction(uint32_t kind);
static int doesKindIndicate32BitInstruction(uint32_t kind)
{
    switch (kind)
    {
    case 2:
        return 0;
    case 3:  /* Not even sure if kind==3 is ever used for RISC-V; probably not, in which case this will do no harm */
    case 4:
        return 1;
    default:
        __throw_and_return(invalidArgumentException, -1);
    }
}
#endif

void Platform_ClearHardwareBreakpoint(uint32_t address, uint32_t kind)
{
  RISCV_TRIGGER *trigger;

  trigger = triggerFindByAddr(address);
  if (trigger != NULL) {
    triggerClearHw(trigger);
    triggerFree(trigger);
  } else {
    /* maybe send back an error code to GDB, but this should never happen unless there's a fundamental bug or "disconnect" */
    /* Currently doing nothing */
  }
}

#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE  
static uint32_t convertWatchpointTypeToCortexMType(PlatformWatchpointType type);
#endif

void Platform_SetHardwareWatchpoint(uint32_t address, uint32_t size, PlatformWatchpointType type)
{
#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE  
    uint32_t       nativeType = convertWatchpointTypeToCortexMType(type);
    DWT_COMP_Type* pComparator;
    
    if (!isValidDWTComparatorSetting(address, size, nativeType))
        __throw(invalidArgumentException);
    
    pComparator = enableDWTWatchpoint(address, size, nativeType);
    if (!pComparator)
        __throw(exceededHardwareResourcesException);
#else
    // implement for RISC-V
#endif    
}

#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE  
static uint32_t convertWatchpointTypeToCortexMType(PlatformWatchpointType type)
{
    switch (type)
    {
    case MRI_PLATFORM_WRITE_WATCHPOINT:
        return DWT_COMP_FUNCTION_FUNCTION_DATA_WRITE;
    case MRI_PLATFORM_READ_WATCHPOINT:
        return DWT_COMP_FUNCTION_FUNCTION_DATA_READ;
    case MRI_PLATFORM_READWRITE_WATCHPOINT:
        return DWT_COMP_FUNCTION_FUNCTION_DATA_READWRITE;
    default:
        return 0;
    }
}
#endif

void Platform_ClearHardwareWatchpoint(uint32_t address, uint32_t size, PlatformWatchpointType type)
{
#ifndef DISABLE_APPARENTLY_ARM_SPECIFIC_CODE
    uint32_t nativeType = convertWatchpointTypeToCortexMType(type);
    
    if (!isValidDWTComparatorSetting(address, size, nativeType))
        __throw(invalidArgumentException);
    
    disableDWTWatchpoint(address, size, nativeType);
#else
    // implement for RISC-V
#endif
}

uint32_t Platform_GetTargetXmlSize(void)
{
    return sizeof(g_targetXml) - 1;
}


const char* Platform_GetTargetXml(void)
{
    return g_targetXml;
}


static void triggersInit()
{
  int i;

  for (i = 0; i < MAX_TRIGGERS && triggersIsValidIdx(i); i++) {
    triggerPool[i].idx = i;
    triggerPool[i].allocated = 0;
    triggerPool[i].programmed = 0;
    triggerMaxValid = i+1;  /* max valid is really 'one beyond the max valid index' */
  }
}

static void tselectWrite(uint32_t val)
{
  __asm volatile ("csrw tselect, %0" : : "r" (val));  
}

static uint32_t tselectRead(void)
{
  uint32_t tselect;
  __asm volatile ("csrr %0, tselect" : "=r" (tselect) : );
  return tselect;
}

static int triggersIsValidIdx(uint32_t idx)
{
  /* save tselect value */
  /* try to set tselect to the given idx */
  /* see if tselect reads back with what we wrote */
  /* restore previous tselect value */
  int retval;
  
  uint32_t tselect_original = tselectRead();
  tselectWrite(idx);
  retval = (tselectRead() == idx);
  tselectWrite(tselect_original);  
  return retval;
}

RISCV_TRIGGER *triggerAlloc(uint32_t addr)
{
  int i;
  
  for (i = 0; i < triggerMaxValid; i++) {
    if (triggerPool[i].allocated == 0) {
      triggerPool[i].allocated = 1;
      triggerPool[i].addr = addr;
      return &triggerPool[i];
    }
  }
  return NULL;
}

void triggerFree(RISCV_TRIGGER *t)
{
  if (t) {
    t->allocated = 0;
  }
}
