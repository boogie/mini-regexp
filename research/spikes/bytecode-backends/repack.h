#ifndef REPACK_H
#define REPACK_H

struct repack_record {
    unsigned source, end, destination, width, opcode;
    int target;
};

int re_repack(const unsigned char *input, unsigned length, unsigned char *output,
              unsigned capacity, struct repack_record *scratch, unsigned records);

#endif
