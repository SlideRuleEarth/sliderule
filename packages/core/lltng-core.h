#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER core

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./tp.h"

#if !defined(_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TP_H

#include <lttng/tracepoint.h>



typedef struct {
    uint32_t    id;
    uint32_t    parent;
} trace_t;






TRACEPOINT_EVENT(
    sliderule,
    my_tracepoint,
    TP_ARGS(
        const struct my_custom_structure*, my_custom_structure,
        float, ratio,
        const char*, query
    ),
    TP_FIELDS(
        ctf_string(query_field, query)
        ctf_float(double, ratio_field, ratio)
        ctf_integer(int, recv_size, my_custom_structure->recv_size)
        ctf_integer(int, send_size, my_custom_structure->send_size)
    )
)





/* The tracepoint class */
TRACEPOINT_EVENT_CLASS(
    sliderule,              /* Tracepoint provider name */
    span,                   /* Tracepoint class name */
    TP_ARGS(                /* Input arguments */
        uint32_t, id,
        uint32_t, parent
    ),
    TP_FIELDS(              /* Output event fields */
        ctf_integer(uint32_t, id, id)
        ctf_integer(uint32_t, parent, parent)
    )
)

/* The tracepoint instances */
TRACEPOINT_EVENT_INSTANCE(
    sliderule,
    span,
    start,                  /* Tracepoint name */
    TP_ARGS(                /* Input arguments */
        uint32_t, id,
        uint32_t, parent
    ),
)
TRACEPOINT_EVENT_INSTANCE(
    my_app,
    my_class,
    get_settings,
    TP_ARGS(
        int, userid,
        size_t, len
    )
)
TRACEPOINT_EVENT_INSTANCE(
    my_app,
    my_class,
    get_transaction,
    TP_ARGS(
        int, userid,
        size_t, len
    )
)

#endif /* _TP_H */

#include <lttng/tracepoint-event.h>