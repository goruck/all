// compile: "gcc -Wall -o alarm-client2-ssl alarm-client2-ssl.c -lssl -lcrypto"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// openssl
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#define CIPHER_LIST "AES128-SHA"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

/*---------------------------------------------------------------------*/
/*--- InitCTX - initialize the SSL engine.                          ---*/
/*---------------------------------------------------------------------*/
SSL_CTX* InitCTX(void)
{   const SSL_METHOD *method;
    SSL_CTX *ctx;

    SSL_library_init();
    OpenSSL_add_all_algorithms();		/* Load cryptos, et.al. */
    SSL_load_error_strings();			/* Bring in and register error messages */
    method = SSLv23_client_method();		/* Create new client-method instance */
    ctx = SSL_CTX_new(method);			/* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

/*---------------------------------------------------------------------*/
/*--- ShowCerts - print out the certificates.                       ---*/
/*---------------------------------------------------------------------*/
void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);	/* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);							/* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);							/* free the malloc'ed string */
        X509_free(cert);					/* free the malloc'ed certificate copy */
    }
    else
        printf("No certificates.\n");
}

int main(int argc, char *argv[])
{
    int sockfd, portno, res;
    //const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    //const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";
    struct sockaddr_in serv_addr;
    struct hostent *server;
    SSL_CTX *ctx;
    SSL *ssl;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
        exit(0);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
        exit(0);
    }

    ctx = InitCTX();

    /*Set the Cipher List*/
    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST) <= 0) {
      printf("Error setting the cipher list.\n");
      exit(0);
    }

    /*Indicate the certificate file to be used*/
    /*if (SSL_CTX_use_certificate_file(ctx,"certificate.crt", SSL_FILETYPE_PEM) <= 0) {
      printf("Error setting the certificate file.\n");
      exit(0);
    }*/

    /*Indicate the key file to be used*/
    /*if (SSL_CTX_use_PrivateKey_file(ctx, "privateKey.key", SSL_FILETYPE_PEM) <= 0) {
      printf("Error setting the key file.\n");
      exit(0);
    }*/

    /*Make sure the key and certificate file match*/
    /*if (SSL_CTX_check_private_key(ctx) == 0) {
      printf("Private key does not match the certificate public key\n");
      exit(0);
    }*/

    /* Set the list of trusted CAs based on the file and/or directory provided*/
    if(SSL_CTX_load_verify_locations(ctx,"certificate.crt",NULL)<1) {
      printf("Error setting verify location\n");
      exit(0);
    }

    /* Set for server verification*/
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);

    //SSL_CTX_set_options(ctx, flags);			/* leave only TLS as an option */

    //SSL_CTX_set_verify_depth(ctx, 4);

    ssl = SSL_new(ctx);					/* create new SSL connection state */
    SSL_set_fd(ssl, sockfd);				/* attach the socket descriptor */
    res = SSL_connect(ssl);				/* perform the connection */
    if (res <= 0) {
          ERR_print_errors_fp(stderr);
          close(sockfd);
          exit(EXIT_FAILURE);
        }
    else {
        printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
        ShowCerts(ssl);					/* get any certs */

        printf("Please enter the message: ");

        bzero(buffer,256);
        fgets(buffer,255,stdin);

        res = SSL_write(ssl, buffer, strlen(buffer));
        if (res <= 0) {
          ERR_print_errors_fp(stderr);
          SSL_free(ssl);
          close(sockfd);
          exit(EXIT_FAILURE);
        }

        bzero(buffer,256);

        res = SSL_read(ssl, buffer, 255);
        if (res <= 0) {
          ERR_print_errors_fp(stderr);
          SSL_free(ssl);
          close(sockfd);
          exit(EXIT_FAILURE);
        }

        printf("%s\n",buffer);

     }

     close(sockfd);
     SSL_CTX_free(ctx);

    return 0;
}
