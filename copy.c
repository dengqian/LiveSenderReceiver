int sockfd,len;  
    struct sockaddr_in addr;  
    int addr_len = sizeof(struct sockaddr_in);  
    char buffer[256];  
    /*建立socket*/  
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){  
        perror ("socket");  
        exit(1);  
    }  
    /*填写sockaddr_in 结构*/  
    bzero ( &addr, sizeof(addr) );  
    addr.sin_family=AF_INET;  
    addr.sin_port=htons(PORT);  
    addr.sin_addr.s_addr=htonl(INADDR_ANY) ;  
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))<0){  
        perror("connect");  
        exit(1);  
    }  
