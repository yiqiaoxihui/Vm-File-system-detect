#ifndef QCOW2_H_INCLUDED
#define QCOW2_H_INCLUDED

#define L1E_OFFSET_MASK 0x00fffffffffffe00ULL
#define L2E_OFFSET_MASK 0x00fffffffffffe00ULL

typedef struct QCowHeader {
    __U32_TYPE magic;
    __U32_TYPE version;
    __U64_TYPE backing_file_offset;
    __U32_TYPE backing_file_size;
    __U32_TYPE cluster_bits;
    __U64_TYPE size; /* in bytes */
    __U32_TYPE crypt_method;
    __U32_TYPE l1_size; /* XXX: save number of clusters instead ? */
    __U64_TYPE l1_table_offset;
    __U64_TYPE refcount_table_offset;
    __U32_TYPE refcount_table_clusters;
    __U32_TYPE nb_snapshots;
    __U64_TYPE snapshots_offset;

    /* The following fields are only valid for version >= 3 */
    __U64_TYPE incompatible_features;
    __U64_TYPE compatible_features;
    __U64_TYPE autoclear_features;

    __U32_TYPE refcount_order;
    __U32_TYPE header_length;
} QCowHeader;

#endif // QCOW2_H_INCLUDED
