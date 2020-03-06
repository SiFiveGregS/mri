#include "token.h"
#include <stdint.h>

#if __riscv_xlen == 32
typedef uint32_t RISCV_X_VAL;
#else
typedef uint64_t RISCV_X_VAL;
#endif

/* Important!  Do not modify this structure layout without making corresponding changes to
   the assembly code referred to by the label "mri_context" */

#define MRI_CONTEXT_FLAG_VALID (1 << 0)      /* Does the context struct have valid data? 1=yes, 0=no */
#define MRI_CONTEXT_FLAG_EXITING (1 << 1)    /* Is the debugger exiting back to the state prior to initial debugger entry? */
#define MRI_CONTEXT_FLAG_MEM_FAULT (1 << 2)  /* Has a memory fault been encountered while in the debugger? */
#define MRI_CONTEXT_FLAG_REENTERED (1 << 3)  /* Has the debugger been re-entered (and thus, are the reentered_* fields valid? */

typedef struct {
  RISCV_X_VAL flags;
  RISCV_X_VAL x_1_31[31];  // Not including x0!  Its value is fixed at zero always, of course.
  RISCV_X_VAL mepc;
  RISCV_X_VAL mcause;
  RISCV_X_VAL mstatus;  
  RISCV_X_VAL reentered_mepc;
  RISCV_X_VAL reentered_mcause;
  RISCV_X_VAL reentered_mstatus;  
} MRI_CONTEXT_RISCV;


/* NOTE: The MriExceptionHandler function definition is dependent on the layout of this structure.  It
         is also dictated by the version of gdb which supports the RISC-V processors.  It should only be changed if the
         gdb RISC-V support code is modified and then the context saving and restoring code will need to be modified to
         use the correct offsets as well.
*/

/* NOTE: The largest buffer is required for receiving the 'G' command which receives the contents of the registers from 
   the debugger as two hex digits per byte.  Also need a character for the 'G' command itself. */
/* BUT, for RISC-V we don't know yet what the context is going to look like so let's just say 1024, for now */
#define RISCV_PACKET_BUFFER_SIZE 1024

typedef struct
{
#if 0  // this stuff was copied directly from ARM-support code; may or may not turn out to be applicable for RISC-V
    uint64_t            debuggerStack[CORTEXM_DEBUGGER_STACK_SIZE];
    volatile uint32_t   flags;
    volatile uint32_t   taskSP;
    Context             context;
    uint32_t            originalPC;
    uint32_t            originalMPUControlValue;
    uint32_t            originalMPURegionNumber;
    uint32_t            originalMPURegionAddress;
    uint32_t            originalMPURegionAttributesAndSize;
    uint32_t            originalBasePriority;
    int                 maxStackUsed;
#endif
    MRI_CONTEXT_RISCV   context;
    char                packetBuffer[RISCV_PACKET_BUFFER_SIZE];
} RiscVState;

extern RiscVState     __mriRiscVState;
#if 0
extern const uint32_t   __mriCortexMFakeStack[8];
#endif

void     __mriRiscVInit(Token* pParameterTokens);
