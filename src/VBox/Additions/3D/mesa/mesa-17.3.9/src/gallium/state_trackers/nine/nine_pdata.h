
#ifndef _NINE_PDATA_H_
#define _NINE_PDATA_H_

struct pheader
{
    boolean unknown;
    GUID guid;
    DWORD size;
};

static int
ht_guid_compare( void *a,
                 void *b )
{
    return GUID_equal(a, b) ? 0 : 1;
}

static unsigned
ht_guid_hash( void *key )
{
    unsigned i, hash = 0;
    const unsigned char *str = key;

    for (i = 0; i < sizeof(GUID); i++) {
        hash = (unsigned)(str[i]) + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static enum pipe_error
ht_guid_delete( void *key,
                void *value,
                void *data )
{
    struct pheader *header = value;
#ifndef VBOX_WITH_MESA3D_MSC
    void *header_data = (void *)header + sizeof(*header);
#else
    void *header_data = (uint8_t *)header + sizeof(*header);
#endif

    if (header->unknown) { IUnknown_Release(*(IUnknown **)header_data); }
    FREE(header);

    return PIPE_OK;
}

#endif /* _NINE_PDATA_H_ */
