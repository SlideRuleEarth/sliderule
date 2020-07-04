#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER core

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./lltng-core.h"

#if !defined(_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TP_H

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
    sliderule,
    start,
    TP_ARGS(
        uint32_t, id,
        uint32_t, parent,
        const char*, name,
        const char*, attributes,
    ),
    TP_FIELDS(
        ctf_integer(int, id, id)
        ctf_integer(int, parent, parent)
        ctf_string(name, name)
        ctf_string(attributes, attributes)
    )
)

TRACEPOINT_EVENT(
    sliderule,
    stop,
    TP_ARGS(
        uint32_t, id,
    ),
    TP_FIELDS(
        ctf_integer(int, id, id)
    )
)

#endif /* _TP_H */

#include <lttng/tracepoint-event.h>