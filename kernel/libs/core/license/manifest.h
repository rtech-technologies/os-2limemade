#ifndef LICENSE_MANIFEST_H
#define LICENSE_MANIFEST_H

#include <stdint.h>

/* Immutable Build-Time Manifest */
typedef struct {
    const char* build_hash;
    const char* license_text;
    const char* changelog;
    uint64_t signature;
} license_manifest_t;

static const license_manifest_t g_license = {
    .build_hash = "OSX2-LIMEMADE-2023-B23",
    .license_text = "RTECH Sovereign License v2.0 - Non-Commercial Only.",
    .changelog = "1. PCI Discovery\n2. NVMe SQ/CQ\n3. xHCI Handover\n4. RTC64 GUI",
    .signature = 0xDEADC0DEDEADC0DE /* Signed Build Hash */
};

#endif
