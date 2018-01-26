#include "rsa_common.h"
#include <stdio.h>


int main() {

    unsigned char opn_buf[256], opd_buf[256], cipher_buf[256], msg_buf[256];

    // read the server private key
    read_buffer("opn.bin", opn_buf,    256);
    read_buffer("opd.bin", opd_buf,    256);
    read_buffer("key.bin", cipher_buf, 256);

    rsa_op *opn, *opd;
    rsa_op_init(&opn, opn_buf, 256, 0);
    rsa_op_init(&opd, opd_buf, 256, 0);

    decrypt(opn, opd, cipher_buf, msg_buf, 256);

    write_buffer("key_dec.bin", msg_buf, 256);

    return 0;
}
