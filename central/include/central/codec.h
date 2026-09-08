#pragma once
// Bridge to the LNC's C codec so the Central Computer speaks the identical TLV
// protocol. clang++ compiles the reused tlv.c/bytes.c as C++, so these headers
// and their .c files share C++ linkage — no extern "C" needed.
#include "tlv.h"
#include "bytes.h"
#include "protocol.h"   // #defines only
