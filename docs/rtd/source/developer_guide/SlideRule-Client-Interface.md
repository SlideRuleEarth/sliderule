
### De-serialization

There are two types of SlideRule services distinguished by the type of response they provide: (1) **normal** services, (2) **stream** services.

Normal
    Normal services are accessed via the `GET` HTTP method and return a discrete block of ASCII text, typically formatted as JSON.

    These services can easily be accessed via `curl` or other HTTP-based tools, and contain self-describing data.
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=False)`` call.

Stream
    Stream services are accessed via the `POST` HTTP method and return a serialized stream of binary records containing the results of the processing request.

    These services are more difficult to work with third-party tools since the returned data must be parsed and the data itself is not self-describing.
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=True)`` call.  Inside this call, the SlideRule Python client
    takes care of any additional service calls needed in order to parse the results and return a self-describing Python data structure (i.e. the elements of
    the data structure are named in such a way as to indicate the type and content of the returned data).

    If you want to process streamed results outside of the SlideRule Python client, then a brief description of the format of the data follows.
    For additional guidance, the hidden functions inside the *sliderule.py* source code provide the code which performs
    the stream processing for the SlideRule Python client.

    Each response record is formatted as: `<record header><record type><record data>` where,

    record header
        64-bit big endian structure providing the version and length of the record: `<version:16 bits><type size:16 bits><data size:32 bits>`.
    record type
        null-terminated ASCII string containing the name of the record type
    record data
        binary contents of data

    In order to know how to process the contents of the **record data**, the user must perform an additional query to the SlideRule ``definition`` service,
    providing the **record type**.  The **definition** service returns a JSON response object that provides a format definition of the record type that can
    be used by the client to decode the binary record data.  The format of the **definition** response object is:

    .. code-block:: python

        {
            "__datasize": # minimum size of record
            "field_1":
            {
                "type": # data type (see sliderule.basictypes for full definition), or record type if a nested structure
                "elements": # number of elements, 1 if not an array
                "offset": # starting bit offset into record data
                "flags": # processing flags - LE: little endian, BE: big endian, PTR: pointer
            },
            ...
            "field_n":
            {
                ...
            }
        }
