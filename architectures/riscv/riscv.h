#include "token.h"
#include <stdint.h>

#if __riscv_xlen == 32
typedef uint32_t RISCV_X_VAL;
#else
typedef uint64_t RISCV_X_VAL;
#endif

/* Important!  Do not modify this structure layout without making corresponding changes to
   the assembly code referred to by the label "mri_context" */

#define MRI_RISCV_FLAG_DEBUGGER_ACTIVE (1 << 0)      /* Is the debugger in control? 1=yes, 0=no */
#define MRI_RISCV_FLAG_EXITING (1 << 1)    /* Is the debugger exiting back to the state prior to initial debugger entry? */
#define MRI_RISCV_FLAG_REENTERED (1 << 2)  /* Has the debugger been re-entered (and thus, are the reentered_* fields valid? */
#define MRI_RISCV_FLAG_SINGLE_STEPPING (1 << 3)    /* Is the debugger in the process of driving a single step? */

typedef struct {
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
    volatile uint32_t   flags;
    MRI_CONTEXT_RISCV   context;
    RISCV_X_VAL         originalPC;  
    char                packetBuffer[RISCV_PACKET_BUFFER_SIZE];
} RiscVState;

extern RiscVState     __mriRiscVState;
#if 0
extern const uint32_t   __mriCortexMFakeStack[8];
#endif

void     __mriRiscVInit(Token* pParameterTokens);
