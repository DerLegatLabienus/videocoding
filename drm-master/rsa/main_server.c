#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <string.h>
#include <errno.h>
#include "msgtypes.h"
#include "rsa_common.h"
#include "rsa.h"

unsigned char cipher_buf[256];
unsigned char msg_buf[256];

#define u8   unsigned char
#define u32  unsigned int
#define u64  unsigned long
#define bool unsigned int

#define printk printf

#ifdef TO_KERNEL

//#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/

struct sockaddr_nl src_addr, dest_addr;
struct iovec iov;
int sock_fd;
struct msghdr msg;
struct nlmsghdr *nlh = NULL;

int init_socket() {
	if (sock_fd > 0)
	    return sock_fd;

    sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_USER);  
    if(sock_fd < 0)  {
        perror("socket");
        return -1;  
    }
	printf("socket created\n");
	return sock_fd;
}

void prepare_msg(unsigned char* buf, size_t len) {
	memset(&src_addr,0,sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	bind(sock_fd, (const struct sockadddr*)&src_addr,sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));  
	memset(&dest_addr, 0, sizeof(dest_addr));  
	dest_addr.nl_family = AF_NETLINK;  
	dest_addr.nl_pid = 0;   /* For Linux Kernel */  
	dest_addr.nl_groups = 0; /* unicast */  

	nlh = (struct nlmsghdr *)malloc(  
		              NLMSG_SPACE(MAX_PAYLOAD));  
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));  
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);  
	nlh->nlmsg_pid = getpid();  
	nlh->nlmsg_flags = 0;  

    memcpy(NLMSG_DATA(nlh), buf, len);  

	iov.iov_base = (void *)nlh;  
	iov.iov_len = nlh->nlmsg_len;  
	msg.msg_name = (void *)&dest_addr;  
	msg.msg_namelen = sizeof(dest_addr);  
	msg.msg_iov = &iov;  
	msg.msg_iovlen = 1;
}
#endif

int keygen(rsa_op** n, rsa_op** e, rsa_op** d) {
#ifdef TO_KERNEL
	unsigned char commandBuf[1] = {KEYGEN};
	if (init_socket() < 0) {
	    return -1;
    }

	prepare_msg(commandBuf, 1);

	printf("Sending message to kernel\n");  
	int i = sendmsg(sock_fd,&msg,0);  
	printf("i = %d\n",i);
	
	printf("Waiting for e\n");  
	i = recvmsg(sock_fd,&msg,0);
	unsigned char* buf = (u8*) NLMSG_DATA(nlh);
#else
    unsigned char buffer[640];
    unsigned char* buf = buffer;
    rsa_keygen(buf);
#endif
	rsa_op_init(e,buf,4,0);
	rsa_op_print(*e,1);
	buf+=128;
	printf("Waiting for d\n");  
	rsa_op_init(d,buf,256,0);
	rsa_op_print(*d,1);
	buf+=256;
	printf("Waiting for n\n");  
	rsa_op_init(n,buf,256,0);
	rsa_op_print(*n,1);
	return 0;
}

int encrypt(rsa_op* n, rsa_op* e, u8* m, u8** c, size_t len) {
	unsigned char buf[1024] = {0};
	buf[0] = ENCRYPT;
	rsa_op_serialize(e,buf+1,0);
	rsa_op_serialize(n,buf+129,0);
	memcpy(buf+768,m,len);

	rsa_op* mop;
	rsa_op_init(&mop,buf+768,256,0);
	printf("mop before encrypt: ");
	rsa_op_print(mop,1);

#ifdef TO_KERNEL
	if (init_socket() < 0) {
	    return -1;
    }

    prepare_msg(buf,1024);
	printf("Sending message 2 to kernel\n");
	int i = sendmsg(sock_fd,&msg,0);  
	printf("i = %d\n",i);
	i = recvmsg(sock_fd,&msg,0);
	*c = (u8*) NLMSG_DATA(nlh);
#else
    *c = (u8*)cipher_buf;
    rsa_encrypt(buf,*c);
#endif
	rsa_op_init(&mop,*c,256,0);
	printf("this is cop: ");
	rsa_op_print(mop,1);
	return 0;
}

int decrypt(rsa_op* n, rsa_op* d, u8* c, u8** m, size_t len) {
    unsigned char buf[1024] = {0};
    buf[0] = DECRYPT;
    
    rsa_op_serialize(d,buf+1,0);
    rsa_op_serialize(n,buf+257,0);
    
    memcpy(buf+(768),c,len);

    rsa_op* mop;
#ifdef TO_KERNEL
	if (init_socket() < 0) {
	    return -1;
    }

    prepare_msg(buf, 1024);
    printf("Sending message 2 to kernel\n");
    int i = sendmsg(sock_fd,&msg,0);  
    printf("i = %d\n",i);

    i = recvmsg(sock_fd,&msg,0);
    *m = (u8*) NLMSG_DATA(nlh);
#else
    *m = (u8*)msg_buf;
    rsa_decrypt(buf, *m);
#endif
    rsa_op_init(&mop,*m,256,0);
    printf("this is mop2: ");
    rsa_op_print(mop,1);

    return 0;
}


int test_encryption_decryption() {
	char msg[] = "hello world";
	unsigned char* encrypted;
	char* decrypted;
	rsa_op *ope, *opd, *opn, *temp;

    printk("1. keygen.\n");
	keygen(&opn, &ope, &opd);

    printk("2. encrypt.\n");
	encrypt(opn, ope, (unsigned char*)msg, &encrypted, strlen(msg));

    printk("3. decrypt.\n");
	decrypt(opn, opd, encrypted, (unsigned char**)&decrypted, 256); 
	
	printk("msg: %s, decrypted:%s\n",msg, decrypted);
	int res = strcmp(msg, decrypted);
	printk("res = %d\n", res);
	return res;
}


int write_buffer(const char path[], u8* buf, int len) {
	FILE* pFile = fopen(path, "wb");
	int res = 0;
	if (pFile) {
	    res = fwrite(buf, len, 1, pFile);
    } else {
        perror("fwrite");
    }
    fclose(pFile);
    return res == len;
}

int read_buffer(const char path[], u8* buf, int len) {
	FILE* pFile = fopen(path, "rb");
	int res = 0;
	if (pFile) {
	    res = fread(buf, len, 1, pFile);
    } else {
        perror("fread");
    }
    fclose(pFile);
    return res == len;
}


int server_keygen() {
    rsa_op *ope, *opd, *opn;
	keygen(&opn, &ope, &opd);

	u8 opn_buf[256], opd_buf[256], ope_buf[4]; 
	rsa_op_serialize(opn, opn_buf, 0);
	rsa_op_serialize(opd, opd_buf, 0);
	rsa_op_serialize(ope, ope_buf, 0);

	write_buffer("opn.bin", opn_buf, 256);
	write_buffer("opd.bin", opd_buf, 256);
	write_buffer("ope.bin", ope_buf, 4);
	return 0;
}

int client_encrypt() {

	char msg[] = "hello world";
	unsigned char* encrypted;

	u8 opn_buf[256], ope_buf[4]; 
	read_buffer("opn.bin", opn_buf, 256);
	read_buffer("ope.bin", ope_buf, 4);

	rsa_op *ope, *opn;
	rsa_op_init(&ope, ope_buf, 4,   0);
	rsa_op_init(&opn, opn_buf, 256, 0);

	encrypt(opn, ope, (unsigned char*)msg, &encrypted, strlen(msg));

	write_buffer("encrypted.bin", encrypted, 256);
	return 0;
}

int server_decrypt() {
	char* decrypted = 0;
	
	u8 opn_buf[256], opd_buf[256];
	u8 encrypted[256];
	read_buffer("opn.bin", opn_buf, 256);
	read_buffer("opd.bin", opd_buf, 256);
	read_buffer("encrypted.bin", encrypted, 256);

	rsa_op *opn, *opd;
	rsa_op_init(&opd, opd_buf, 256, 0);
	rsa_op_init(&opn, opn_buf, 256, 0);

	decrypt(opn, opd, encrypted, (unsigned char**)&decrypted, 256); 
	printf("decrypted: %s\n", decrypted);
	return 0;
}


int main(int argc, char* argv[])
{
    if (rsaLoad() < 0) {
        fprintf(stderr, "rsaLoad failed\n");
        return -1;
    }
	/* char msg[] = "hello world"; */
	/* unsigned char* encrypted; */
	/* char* decrypted; */
	/* rsa_op *ope, *opd, *opn; */

    /* printf("1. keygen.\n"); */
	/* keygen(&opn, &ope, &opd); */

    /* printf("2. encrypt.\n"); */
	/* encrypt(opn, ope, (unsigned char*)msg, &encrypted, strlen(msg)); */

    /* printf("3. encrypt.\n"); */
	/* decrypt(opn, opd, encrypted, (unsigned char**)&decrypted, 256); */ 
	
	/* printf("msg: %s, decrypted:%s\n",msg, decrypted); */
	/* int res = strcmp(msg, decrypted); */
	/* printf("res = %d\n", res); */
	/* return res; */
    server_keygen();
    client_encrypt();
    server_decrypt();

    return 0;
}

