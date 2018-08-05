// pifwrap.cpp --------------------------------------------------------
//
// Copyright (c) 2001 to 2013  te
//
// a C wrapper for the pif code
//
// Licence: Creative Commons Attribution-ShareAlike 3.0 Unported License.
//          http://creativecommons.org/licenses/by-sa/3.0/
//---------------------------------------------------------------------
#if defined(_WIN32)
  #define _CRT_SECURE_NO_WARNINGS   // Disable deprecation warning in VS2005
#else
  #define _XOPEN_SOURCE 600         // For PATH_MAX on linux
#endif

#include <stdio.h>
#include <string.h>

#include "pifwrap.h"
#include "pif.h"

#define pPif ((Tpif *)h)

//---------------------------------------------------------------------
int pifVersion(char *outStr, int outLen) {
printf("%s\n", __func__);
  if (outLen<=0)
    return 0;
  const char * retval = (const char *)("libpif," __DATE__ "," __TIME__);
  int retlen = strlen(retval);
  strncpy(outStr, retval, outLen);
  outStr[outLen-1] = 0;
  return retlen;
  }

int pifGetDeviceIdCode(pifHandle h, uint32_t* v) {
printf("%s\n", __func__);
  return pPif->getDeviceIdCode(*v);
  }
int pifGetStatusReg(pifHandle h, uint32_t* v) {
printf("%s\n", __func__);
  return pPif->getStatusReg(*v);
  }
int pifGetFeatureBits(pifHandle h, uint16_t* v) {
printf("%s\n", __func__);
  return pPif->getFeatureBits(*v);
  }
int pifSetFeatureBits(pifHandle h, uint16_t v) {
printf("%s\n", __func__);
  return pPif->setFeatureBits(v);
  }
int pifGetFeatureRow(pifHandle h, uint64_t* v) {
printf("%s\n", __func__);
  return pPif->getFeatureRow(*v);
  }
int pifSetFeatureRow(pifHandle h, uint64_t v) {
printf("%s\n", __func__);
  return pPif->setFeatureRow(v);
  }
int pifGetTraceId(pifHandle h, uint8_t* p) {
printf("%s\n", __func__);
  return pPif->getTraceId(p);
  }
int pifEnableCfgInterfaceOffline(pifHandle h) {
printf("%s\n", __func__);
  return pPif->enableCfgInterfaceOffline();
  }
int pifEnableCfgInterfaceTransparent(pifHandle h) {
printf("%s\n", __func__);
  return pPif->enableCfgInterfaceTransparent();
  }
int pifDisableCfgInterface(pifHandle h) {
printf("%s\n", __func__);
  return pPif->disableCfgInterface();
  }
int pifRefresh(pifHandle h) {
printf("%s\n", __func__);
  return pPif->refresh();
  }
int pifProgDone(pifHandle h) {
printf("%s\n", __func__);
  return pPif->progDone();
  }
int pifErase(pifHandle h, int Amask) {
printf("%s\n", __func__);
  return pPif->erase(Amask);
  }
int pifEraseAll(pifHandle h) {
printf("%s\n", __func__);
  return pPif->eraseAll();
  }
int pifInitCfgAddr(pifHandle h) {
printf("%s\n", __func__);
  return pPif->initCfgAddr();
  }
int pifEraseCfg(pifHandle h) {
printf("%s\n", __func__);
  return pPif->eraseCfg();
  }
int pifEraseFeature(pifHandle h) {
printf("%s\n", __func__);
  return pPif->eraseFeature();
  }
int pifProgCfgPage(pifHandle h, const uint8_t *p) {
printf("%s\n", __func__);
  return pPif->progCfgPage(p);
  }
int pifReadCfgPages(pifHandle h, int numPages, uint8_t *p) {
printf("%s\n", __func__);
  return pPif->readCfgPages(numPages, p);
  }
int pifEraseUfm(pifHandle h) {
printf("%s\n", __func__);
  return pPif->eraseUfm();
  }
int pifReadUfmPages(pifHandle h, int numPages, uint8_t *p) {
printf("%s\n", __func__);
  return pPif->readUfmPages(numPages, p);
  }
int pifReadUfmPages(pifHandle h, int pageNumber, int numPages, uint8_t *p) {
printf("%s\n", __func__);
  return pPif->readUfmPages(pageNumber, numPages, p);
  }
int pifWriteUfmPages(pifHandle h, int pageNumber, int numPages, uint8_t *p) {
printf("%s\n", __func__);
  return pPif->writeUfmPages(pageNumber, numPages, p);
  }
int pifGetBusyFlag(pifHandle h, int *pFlag) {
printf("%s\n", __func__);
  return pPif->getBusyFlag(pFlag);
  }
int pifWaitUntilNotBusy(pifHandle h, int maxLoops) {
printf("%s\n", __func__);
  return pPif->waitUntilNotBusy(maxLoops);
  }
int pifSetUsercode(pifHandle h, uint8_t* p) {
printf("%s\n", __func__);
  return pPif->setUsercode(p);
  }
int pifGetUsercode(pifHandle h, uint8_t* p) {
printf("%s\n", __func__);
  return pPif->getUsercode(p);
  }

int pifMcpWrite(pifHandle h, uint8_t* p, int len) {
printf("%s\n", __func__);
  return pPif->mcpWrite(p, len);
  }
int pifMcpRead(pifHandle h, int reg, uint8_t* v) {
printf("%s\n", __func__);
  return pPif->mcpRead(reg, v);
  }

int pifAppRead(pifHandle h, uint8_t *p, int AnumBytes) {
printf("%s\n", __func__);
  return pPif->appRead(p, AnumBytes);
  }
int pifAppWrite(pifHandle h, uint8_t *p, int AnumBytes) {
printf("%s\n", __func__);
  return pPif->appWrite(p, AnumBytes);
  }

pifHandle pifInit() {
  printf("%s\n", __func__);
  return (pifHandle)(new Tpif());
  }
void pifClose(pifHandle h) {
printf("%s\n", __func__);
  delete pPif;
  }

// EOF ----------------------------------------------------------------
