#include "platforms.h"

void Platform_Init(Token* pParameterTokens)
{
  // implement
}


int Platform_CommReceiveChar(void)
{
  // implement
  return 0;
}


int Platform_CommCausedInterrupt(void)
{
  // implement
  return 0;
}

int Platform_CommShouldWaitForGdbConnect(void)
{
  // implement
  return 0;
}

uint32_t Platform_CommHasReceiveData(void)
{
  // implement
  return 0;
}

void Platform_CommClearInterrupt(void)
{
  // implement  
}

int Platform_CommIsWaitingForGdbToConnect(void)
{
  // implement
  return 0;
}

void Platform_CommWaitForReceiveDataToStop(void)
{
  // implement    
}

void Platform_CommPrepareToWaitForGdbConnection(void)
{
  // implement
}


void Platform_CommSendChar(int Character)
{
  // implement  
}

uint32_t Platform_GetDeviceMemoryMapXmlSize(void)
{
  // implement
  return 0;
}


const char* Platform_GetDeviceMemoryMapXml(void)
{
  return NULL;  // implement
}
