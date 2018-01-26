#include "rc4.h"
#include <stdio.h>

int main()
{
    int i = 0;
    const unsigned char plaintext[] = "plaintext";
    unsigned char ciphertext[]  = "plaintext";
    unsigned char ciphertext2[] = "plaintext";
    const unsigned char key[]  = "6aa2f1447406a0d336ab4ff3a118fd16637e887a5b5259f001af42f50e9a1c18";
    const unsigned char key2[] = "19fa33366002178d1c2f4faba8c78dfb83c757b7dc3a1c435a935605ec3cf8b3";
    unsigned char result[]  = "resultresult";
    unsigned char result2[] = "resultresult";

    rc4_state state, state2;
    rc4_init(&state, key, 9);
    rc4_init(&state2, key2, 6);
    rc4_crypt(&state, plaintext, ciphertext, 9);
    rc4_crypt(&state2, plaintext, ciphertext2, 9);

    for (i = 0; i != 9; ++i)
        printf("%2x|", ciphertext[i]);
    printf(" - ");
    for (i = 0; i != 9; ++i)
        printf("%2x|", ciphertext2[i]);
    printf("\n");

    rc4_init(&state, key, 9);
    rc4_init(&state2, key2, 6);
    rc4_crypt(&state, ciphertext, result, 9);
    rc4_crypt(&state2, ciphertext2, result2, 9);
    for (i = 0; i != 9; ++i)
        printf("%c", result[i]);
    printf(" - ");
    for (i = 0; i != 9; ++i)
        printf("%c", result2[i]);
    printf("\n");
    return 0;
}

